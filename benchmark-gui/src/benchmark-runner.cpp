#include "benchmark-runner.h"

#include "protocol-utils.h"

#include <QMetaObject>
#include <QRandomGenerator>

#include <algorithm>
#include <cstring>
#include <limits>
#include <utility>

namespace {

template<typename Fn>
void invoke_on_worker_thread(worker_t* worker, Fn&& fn, Qt::ConnectionType type) {
  if (worker == nullptr) {
    return;
  }

  auto* target_thread = worker->thread();
  if (target_thread == nullptr || target_thread == QThread::currentThread() || !target_thread->isRunning()) {
    fn();
    return;
  }

  QMetaObject::invokeMethod(worker, std::forward<Fn>(fn), type);
}

} // namespace

benchmark_runner_t::benchmark_runner_t(QObject* parent)
    : QObject(parent) {
  qRegisterMetaType<worker_t::stats_t>("worker_t::stats_t");
}

benchmark_runner_t::~benchmark_runner_t() {
  stop();
}

void benchmark_runner_t::set_config(const config_t& config) {
  config_ = config;
}

void benchmark_runner_t::start() {
  if (running_.load()) {
    return;
  }

  stop();

  running_.store(true);
  paused_.store(false);
  start_time_ = std::chrono::steady_clock::now();

  {
    QMutexLocker locker(&stats_mutex_);
    aggregated_stats_ = aggregated_stats_t{};
    writer_stats_by_worker_.clear();
    reader_stats_by_worker_.clear();
    total_latency_ = latency_accumulator_t{};
    writer_latency_ = latency_accumulator_t{};
    reader_latency_ = latency_accumulator_t{};
  }

  QString initialization_error;
  if (!initialize_topic(&initialization_error)) {
    running_.store(false);
    emit error_occurred(initialization_error);
    return;
  }

  create_workers();

  emit benchmark_started();
}

void benchmark_runner_t::stop() {
  if (!running_.load()) {
    return;
  }

  running_.store(false);
  paused_.store(false);

  destroy_workers();

  emit benchmark_stopped();
}

void benchmark_runner_t::pause() {
  if (!running_.load() || paused_.load()) {
    return;
  }

  paused_.store(true);

  for (auto* writer : writers_) {
    invoke_on_worker_thread(writer, [writer]() { writer->pause(); }, Qt::QueuedConnection);
  }
  for (auto* reader : readers_) {
    invoke_on_worker_thread(reader, [reader]() { reader->pause(); }, Qt::QueuedConnection);
  }
}

void benchmark_runner_t::resume() {
  if (!running_.load() || !paused_.load()) {
    return;
  }

  paused_.store(false);

  for (auto* writer : writers_) {
    invoke_on_worker_thread(writer, [writer]() { writer->resume(); }, Qt::QueuedConnection);
  }
  for (auto* reader : readers_) {
    invoke_on_worker_thread(reader, [reader]() { reader->resume(); }, Qt::QueuedConnection);
  }
}

benchmark_runner_t::aggregated_stats_t benchmark_runner_t::get_aggregated_stats() const {
  QMutexLocker locker(&stats_mutex_);
  return build_aggregated_stats_locked();
}

void benchmark_runner_t::on_worker_stats_updated(const worker_t::stats_t& stats) {
  QMutexLocker locker(&stats_mutex_);
  auto* worker = qobject_cast<worker_t*>(sender());
  if (worker == nullptr) {
    return;
  }

  if (qobject_cast<writer_worker_t*>(worker) != nullptr) {
    record_latency_sample(writer_stats_by_worker_.value(worker), stats, writer_latency_, total_latency_);
    writer_stats_by_worker_.insert(worker, stats);
  } else if (qobject_cast<reader_worker_t*>(worker) != nullptr) {
    record_latency_sample(reader_stats_by_worker_.value(worker), stats, reader_latency_, total_latency_);
    reader_stats_by_worker_.insert(worker, stats);
  } else {
    return;
  }

  aggregated_stats_ = build_aggregated_stats_locked();
  emit stats_updated(aggregated_stats_);
}

void benchmark_runner_t::create_workers() {
  destroy_workers();

  writers_.reserve(config_.num_writers);
  writer_threads_.reserve(config_.num_writers);

  for (int i = 0; i < config_.num_writers; ++i) {
    auto* thread = new QThread(this);
    const auto partition_id = random_partition_for_worker();
    auto* worker = new writer_worker_t(
            config_.host, config_.port, config_.topic_name, partition_id, config_.writer_qps, config_.message_size);

    connect(worker, &worker_t::stats_updated, this, &benchmark_runner_t::on_worker_stats_updated);
    connect(worker, &worker_t::error_occurred, this, &benchmark_runner_t::error_occurred);
    connect(worker, &QObject::destroyed, this, [this, worker]() {
      QMutexLocker locker(&stats_mutex_);
      writer_stats_by_worker_.remove(worker);
    });

    worker->moveToThread(thread);

    connect(thread, &QThread::started, worker, &worker_t::start);
    connect(thread, &QThread::finished, worker, &QObject::deleteLater);

    writer_threads_.append(thread);
    writers_.append(worker);
    thread->start();
  }

  readers_.reserve(config_.num_readers);
  reader_threads_.reserve(config_.num_readers);

  for (int i = 0; i < config_.num_readers; ++i) {
    auto* thread = new QThread(this);
    const auto partition_id = random_partition_for_worker();
    auto* worker =
            new reader_worker_t(config_.host, config_.port, config_.topic_name, partition_id, config_.reader_qps);

    connect(worker, &worker_t::stats_updated, this, &benchmark_runner_t::on_worker_stats_updated);
    connect(worker, &worker_t::error_occurred, this, &benchmark_runner_t::error_occurred);
    connect(worker, &QObject::destroyed, this, [this, worker]() {
      QMutexLocker locker(&stats_mutex_);
      reader_stats_by_worker_.remove(worker);
    });

    worker->moveToThread(thread);

    connect(thread, &QThread::started, worker, &worker_t::start);
    connect(thread, &QThread::finished, worker, &QObject::deleteLater);

    reader_threads_.append(thread);
    readers_.append(worker);
    thread->start();
  }
}

void benchmark_runner_t::destroy_workers() {
  for (auto* worker : writers_) {
    invoke_on_worker_thread(worker, [worker]() { worker->stop(); }, Qt::BlockingQueuedConnection);
  }
  for (auto* worker : readers_) {
    invoke_on_worker_thread(worker, [worker]() { worker->stop(); }, Qt::BlockingQueuedConnection);
  }

  for (auto* thread : writer_threads_) {
    thread->quit();
    thread->wait(1000);
  }
  for (auto* thread : reader_threads_) {
    thread->quit();
    thread->wait(1000);
  }

  writers_.clear();
  readers_.clear();
  writer_threads_.clear();
  reader_threads_.clear();

  QMutexLocker locker(&stats_mutex_);
  writer_stats_by_worker_.clear();
  reader_stats_by_worker_.clear();
  total_latency_ = latency_accumulator_t{};
  writer_latency_ = latency_accumulator_t{};
  reader_latency_ = latency_accumulator_t{};
}

bool benchmark_runner_t::initialize_topic(QString* error_message) const {
  if (config_.partition_count == 0) {
    if (error_message != nullptr) {
      *error_message = "Partition count must be greater than zero";
    }
    return false;
  }

  QTcpSocket socket;
  socket.connectToHost(config_.host, config_.port);
  if (!socket.waitForConnected(5000)) {
    if (error_message != nullptr) {
      *error_message = QString("Failed to connect for topic initialization: %1").arg(socket.errorString());
    }
    return false;
  }

  const auto init_partition = static_cast<uint16_t>(config_.partition_count - 1);
  const QByteArray init_message = "__benchmark_topic_init__";

  const auto push_request =
          protocol_utils_t::create_push_request(config_.topic_name.toUtf8(), init_partition, init_message);
  if (socket.write(push_request) != push_request.size() || !socket.waitForBytesWritten(5000)) {
    if (error_message != nullptr) {
      *error_message = QString("Failed to write initialization PUSH: %1").arg(socket.errorString());
    }
    return false;
  }

  protocol_utils_t::response_t push_response;
  if (!read_response(socket, &push_response, error_message)) {
    return false;
  }
  if (!push_response.valid || push_response.error_code != protocol_utils_t::error_code_t::OK
      || push_response.opcode != protocol_utils_t::opcode_t::PUSH) {
    if (error_message != nullptr) {
      *error_message = QString("Initialization PUSH failed: %1").arg(QString::fromUtf8(push_response.message));
    }
    return false;
  }

  const auto delete_request =
          protocol_utils_t::create_delete_request(config_.topic_name.toUtf8(), init_partition, init_message);
  if (socket.write(delete_request) != delete_request.size() || !socket.waitForBytesWritten(5000)) {
    if (error_message != nullptr) {
      *error_message = QString("Failed to write initialization DELETE: %1").arg(socket.errorString());
    }
    return false;
  }

  protocol_utils_t::response_t delete_response;
  if (!read_response(socket, &delete_response, error_message)) {
    return false;
  }
  if (!delete_response.valid || delete_response.error_code != protocol_utils_t::error_code_t::OK
      || delete_response.opcode != protocol_utils_t::opcode_t::DELETE) {
    if (error_message != nullptr) {
      *error_message = QString("Initialization DELETE failed: %1").arg(QString::fromUtf8(delete_response.message));
    }
    return false;
  }

  socket.disconnectFromHost();
  return true;
}

bool benchmark_runner_t::read_response(QTcpSocket& socket,
                                       protocol_utils_t::response_t* response,
                                       QString* error_message) {
  QByteArray buffer;

  while (buffer.size() < static_cast<int>(sizeof(noctua::rpc::response_header_t))) {
    if (!socket.waitForReadyRead(5000)) {
      if (error_message != nullptr) {
        *error_message = QString("Timed out waiting for response header: %1").arg(socket.errorString());
      }
      return false;
    }
    buffer.append(socket.readAll());
  }

  noctua::rpc::response_header_t header{};
  std::memcpy(&header, buffer.constData(), sizeof(header));
  const auto total_length = sizeof(header) + static_cast<size_t>(header.message_len);

  while (buffer.size() < static_cast<int>(total_length)) {
    if (!socket.waitForReadyRead(5000)) {
      if (error_message != nullptr) {
        *error_message = QString("Timed out waiting for full response: %1").arg(socket.errorString());
      }
      return false;
    }
    buffer.append(socket.readAll());
  }

  if (response != nullptr) {
    *response = protocol_utils_t::parse_response(buffer.left(static_cast<int>(total_length)));
  }
  return true;
}

benchmark_runner_t::aggregated_stats_t benchmark_runner_t::build_aggregated_stats_locked() const {
  aggregated_stats_t stats{};
  for (const auto& worker : writer_stats_by_worker_) {
    stats.writer_total_requests += worker.total_requests;
    stats.writer_successful_requests += worker.successful_requests;
    stats.writer_failed_requests += worker.failed_requests;
  }
  for (const auto& worker : reader_stats_by_worker_) {
    stats.reader_total_requests += worker.total_requests;
    stats.reader_successful_requests += worker.successful_requests;
    stats.reader_failed_requests += worker.failed_requests;
  }

  stats.total_requests = stats.writer_total_requests + stats.reader_total_requests;
  stats.successful_requests = stats.writer_successful_requests + stats.reader_successful_requests;
  stats.failed_requests = stats.writer_failed_requests + stats.reader_failed_requests;

  stats.avg_latency_ms = average_latency_ms(total_latency_);
  stats.min_latency_ms = min_latency_ms(total_latency_);
  stats.max_latency_ms = max_latency_ms(total_latency_);
  stats.p95_latency_ms = percentile_ms(total_latency_.samples, 0.95);
  stats.p99_latency_ms = percentile_ms(total_latency_.samples, 0.99);
  stats.writer_avg_latency_ms = average_latency_ms(writer_latency_);
  stats.reader_avg_latency_ms = average_latency_ms(reader_latency_);

  const auto now = std::chrono::steady_clock::now();
  const auto elapsed_micros = std::chrono::duration_cast<std::chrono::microseconds>(now - start_time_).count();
  if (elapsed_micros > 0) {
    stats.requests_per_second =
            static_cast<double>(stats.total_requests) * 1'000'000.0 / static_cast<double>(elapsed_micros);
  }

  return stats;
}

void benchmark_runner_t::record_latency_sample(const worker_t::stats_t& previous_stats,
                                               const worker_t::stats_t& current_stats,
                                               latency_accumulator_t& bucket,
                                               latency_accumulator_t& total_bucket) {
  if (!current_stats.has_latency_sample || current_stats.latency_sample_count <= previous_stats.latency_sample_count) {
    return;
  }

  bucket.sample_count++;
  bucket.total_micros += current_stats.last_latency_micros;
  bucket.samples.push_back(current_stats.last_latency_micros);

  total_bucket.sample_count++;
  total_bucket.total_micros += current_stats.last_latency_micros;
  total_bucket.samples.push_back(current_stats.last_latency_micros);
}

double benchmark_runner_t::percentile_ms(const std::vector<uint64_t>& samples, double percentile) {
  if (samples.empty()) {
    return 0.0;
  }

  auto sorted = samples;
  std::sort(sorted.begin(), sorted.end());
  const auto index = static_cast<size_t>((sorted.size() - 1) * percentile);
  return static_cast<double>(sorted[index]) / 1000.0;
}

double benchmark_runner_t::average_latency_ms(const latency_accumulator_t& accumulator) {
  if (accumulator.sample_count == 0) {
    return 0.0;
  }
  return static_cast<double>(accumulator.total_micros) / static_cast<double>(accumulator.sample_count) / 1000.0;
}

double benchmark_runner_t::min_latency_ms(const latency_accumulator_t& accumulator) {
  if (accumulator.samples.empty()) {
    return 0.0;
  }
  return static_cast<double>(*std::min_element(accumulator.samples.begin(), accumulator.samples.end())) / 1000.0;
}

double benchmark_runner_t::max_latency_ms(const latency_accumulator_t& accumulator) {
  if (accumulator.samples.empty()) {
    return 0.0;
  }
  return static_cast<double>(*std::max_element(accumulator.samples.begin(), accumulator.samples.end())) / 1000.0;
}

uint32_t benchmark_runner_t::random_partition_for_worker() const noexcept {
  return QRandomGenerator::global()->bounded(config_.partition_count);
}
