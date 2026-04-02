#include "worker.h"

#include "protocol-utils.h"

#include "rpc/rpc-protocol.h"

#include <algorithm>
#include <cmath>
#include <cstring>

worker_t::worker_t(
        const QString& host, quint16 port, const QString& topic_name, uint32_t partition_id, int qps, QObject* parent)
    : QObject(parent)
    , host_(host)
    , port_(port)
    , topic_name_(topic_name)
    , partition_id_(partition_id)
    , qps_(qps)
    , socket_(std::make_unique<QTcpSocket>(this))
    , request_timer_(new QTimer(this)) {
  socket_->setSocketOption(QAbstractSocket::LowDelayOption, 1);

  connect(socket_.get(), &QTcpSocket::connected, this, &worker_t::on_connected);
  connect(socket_.get(), &QTcpSocket::disconnected, this, &worker_t::on_disconnected);
  connect(socket_.get(), &QTcpSocket::errorOccurred, this, &worker_t::on_error_occurred);
  connect(socket_.get(), &QTcpSocket::readyRead, this, &worker_t::on_ready_read);

  connect(request_timer_, &QTimer::timeout, this, &worker_t::on_request_timer);
  request_timer_->setInterval(1000 / qps);
}

worker_t::~worker_t() {
  stop();
}

void worker_t::start() {
  if (running_.load()) {
    return;
  }

  running_.store(true);
  paused_.store(false);

  connect_to_host();
}

void worker_t::stop() {
  if (!running_.load()) {
    return;
  }

  running_.store(false);
  paused_.store(false);
  request_timer_->stop();

  if (socket_->isOpen()) {
    socket_->disconnectFromHost();
  }

  {
    QMutexLocker locker(&pending_mutex_);
    pending_send_times_.clear();
  }
  buffer_.clear();
}

void worker_t::pause() {
  if (!running_.load()) {
    return;
  }

  paused_.store(true);
  request_timer_->stop();
}

void worker_t::resume() {
  if (!running_.load() || !paused_.load()) {
    return;
  }

  paused_.store(false);
  request_timer_->start();
}

worker_t::stats_t worker_t::get_stats() const {
  QMutexLocker locker(&stats_mutex_);
  return stats_;
}

void worker_t::connect_to_host() {
  socket_->connectToHost(host_, port_);
}

void worker_t::update_stats(uint64_t latency_micros, bool success) {
  QMutexLocker locker(&stats_mutex_);

  stats_.total_requests++;
  stats_.has_latency_sample = false;

  if (success) {
    stats_.successful_requests++;
  } else {
    stats_.failed_requests++;
  }

  if (latency_micros > 0) {
    stats_.latency_sample_count++;
    stats_.latency_total_micros += latency_micros;
    stats_.last_latency_micros = latency_micros;
    stats_.has_latency_sample = true;

    if (stats_.latency_sample_count == 1) {
      stats_.min_latency_ms = static_cast<double>(latency_micros) / 1000.0;
      stats_.max_latency_ms = static_cast<double>(latency_micros) / 1000.0;
    } else {
      stats_.min_latency_ms = std::min(stats_.min_latency_ms, static_cast<double>(latency_micros) / 1000.0);
      stats_.max_latency_ms = std::max(stats_.max_latency_ms, static_cast<double>(latency_micros) / 1000.0);
    }

    stats_.avg_latency_ms = static_cast<double>(stats_.latency_total_micros)
                            / static_cast<double>(stats_.latency_sample_count) / 1000.0;
    stats_.p95_latency_ms = stats_.max_latency_ms;
    stats_.p99_latency_ms = stats_.max_latency_ms;
  }

  emit stats_updated(stats_);
}

void worker_t::send_request(const QByteArray& data) {
  if (socket_->state() == QAbstractSocket::ConnectedState) {
    const auto send_time = current_time_micros();
    const auto bytes_queued = socket_->write(data);
    if (bytes_queued == data.size()) {
      QMutexLocker locker(&pending_mutex_);
      pending_send_times_.push_back(send_time);
    }
  }
}

void worker_t::process_response(const QByteArray& data) {
  auto response = protocol_utils_t::parse_response(data);

  uint64_t send_time = 0;
  {
    QMutexLocker locker(&pending_mutex_);
    if (!pending_send_times_.empty()) {
      send_time = pending_send_times_.front();
      pending_send_times_.pop_front();
    }
  }

  uint64_t latency_micros = 0;
  if (send_time != 0) {
    latency_micros = current_time_micros() - send_time;
  }

  const bool success = response.valid && response.error_code == noctua::rpc::error_code_t::OK
                       && response.opcode != noctua::rpc::opcode_t::ERROR;
  update_stats(latency_micros, success);
}

void worker_t::reconnect() {
  if (socket_->state() != QAbstractSocket::UnconnectedState) {
    socket_->disconnectFromHost();
  }
  socket_->connectToHost(host_, port_);
}

void worker_t::on_connected() {
  if (running_.load() && !paused_.load()) {
    request_timer_->start();
  }
}

void worker_t::on_disconnected() {
  request_timer_->stop();
  record_failed_pending_requests();
  if (running_.load()) {
    reconnect();
  }
}

void worker_t::on_error_occurred(QAbstractSocket::SocketError socket_error) {
  Q_UNUSED(socket_error);
  emit error_occurred(socket_->errorString());
  record_failed_pending_requests();
}

void worker_t::on_ready_read() {
  buffer_.append(socket_->readAll());

  while (buffer_.size() >= static_cast<int>(sizeof(noctua::rpc::response_header_t))) {
    noctua::rpc::response_header_t header{};
    std::memcpy(&header, buffer_.constData(), sizeof(header));
    const size_t total_length = sizeof(header) + static_cast<size_t>(header.message_len);

    if (buffer_.size() >= static_cast<int>(total_length)) {
      QByteArray response = buffer_.left(static_cast<int>(total_length));
      buffer_.remove(0, static_cast<int>(total_length));
      process_response(response);
    } else {
      break;
    }
  }
}

void worker_t::on_request_timer() {
  if (running_.load() && !paused_.load()) {
    perform_request();
  }
}

void worker_t::record_failed_pending_requests() {
  size_t failed_count = 0;
  {
    QMutexLocker locker(&pending_mutex_);
    failed_count = pending_send_times_.size();
    pending_send_times_.clear();
  }

  for (size_t i = 0; i < failed_count; ++i) {
    update_stats(0, false);
  }
}

uint64_t worker_t::current_time_micros() const {
  return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch())
          .count();
}
