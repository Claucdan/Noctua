#pragma once

#include <QObject>
#include <QThread>
#include <QTcpSocket>
#include <QTimer>
#include <QByteArray>
#include <QMutex>
#include <QMutexLocker>
#include <QMetaType>
#include <atomic>
#include <deque>
#include <memory>
#include <vector>
#include <algorithm>
#include <chrono>
#include <limits>
#include <cstdint>

class worker_t : public QObject {
  Q_OBJECT

public:
  struct stats_t {
    uint64_t total_requests = 0;
    uint64_t successful_requests = 0;
    uint64_t failed_requests = 0;
    uint64_t latency_sample_count = 0;
    uint64_t latency_total_micros = 0;
    uint64_t last_latency_micros = 0;
    bool has_latency_sample = false;
    double avg_latency_ms = 0.0;
    double min_latency_ms = 0.0;
    double max_latency_ms = 0.0;
    double p95_latency_ms = 0.0;
    double p99_latency_ms = 0.0;
  };

  explicit worker_t(const QString& host,
                    quint16 port,
                    const QString& topic_name,
                    uint32_t partition_id,
                    int qps,
                    QObject* parent = nullptr);
  virtual ~worker_t();

  void start();
  void stop();
  void pause();
  void resume();

  stats_t get_stats() const;

signals:
  void stats_updated(const worker_t::stats_t& stats);
  void error_occurred(const QString& error);

protected:
  virtual void perform_request() = 0;
  void update_stats(uint64_t latency_micros, bool success);
  void send_request(const QByteArray& data);
  void process_response(const QByteArray& data);
  void reconnect();
  void connect_to_host();
  void record_failed_pending_requests();
  [[nodiscard]] uint64_t current_time_micros() const;

protected slots:
  virtual void on_connected();
  virtual void on_disconnected();
  virtual void on_error_occurred(QAbstractSocket::SocketError socket_error);
  virtual void on_ready_read();

  void on_request_timer();

protected:
  QString host_;
  quint16 port_;
  QString topic_name_;
  uint32_t partition_id_;
  int qps_;
  std::atomic<bool> running_{false};
  std::atomic<bool> paused_{false};

  std::unique_ptr<QTcpSocket> socket_;
  QTimer* request_timer_;

  mutable QMutex stats_mutex_;
  stats_t stats_;
  QByteArray buffer_;
  mutable QMutex pending_mutex_;
  std::deque<uint64_t> pending_send_times_;
};

Q_DECLARE_METATYPE(worker_t::stats_t)
