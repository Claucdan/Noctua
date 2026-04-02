#pragma once

#include <QObject>
#include <QVector>
#include <QMutex>
#include <QHash>
#include <QTcpSocket>
#include <memory>
#include <chrono>
#include <vector>
#include "protocol-utils.h"
#include "worker.h"
#include "writer-worker.h"
#include "reader-worker.h"

class benchmark_gui_qt_test_t;

class benchmark_runner_t : public QObject {
  Q_OBJECT

  friend class benchmark_gui_qt_test_t;

public:
  struct config_t {
    QString host = "localhost";
    quint16 port = 8080;
    QString topic_name = "test_topic";
    uint32_t partition_count = 1;
    int num_writers = 1;
    int num_readers = 1;
    int writer_qps = 100;
    int reader_qps = 100;
    int message_size = 1024;
  };

  explicit benchmark_runner_t(QObject* parent = nullptr);
  ~benchmark_runner_t() override;

  void set_config(const config_t& config);

  void start();
  void stop();
  void pause();
  void resume();

  bool is_running() const {
    return running_.load();
  }

  struct aggregated_stats_t {
    uint64_t total_requests = 0;
    uint64_t successful_requests = 0;
    uint64_t failed_requests = 0;
    double avg_latency_ms = 0.0;
    double min_latency_ms = 0.0;
    double max_latency_ms = 0.0;
    double p95_latency_ms = 0.0;
    double p99_latency_ms = 0.0;
    double requests_per_second = 0.0;

    uint64_t writer_total_requests = 0;
    uint64_t writer_successful_requests = 0;
    uint64_t writer_failed_requests = 0;
    double writer_avg_latency_ms = 0.0;

    uint64_t reader_total_requests = 0;
    uint64_t reader_successful_requests = 0;
    uint64_t reader_failed_requests = 0;
    double reader_avg_latency_ms = 0.0;
  };

  aggregated_stats_t get_aggregated_stats() const;

signals:
  void stats_updated(const benchmark_runner_t::aggregated_stats_t& stats);
  void benchmark_started();
  void benchmark_stopped();
  void error_occurred(const QString& error);

private slots:
  void on_worker_stats_updated(const worker_t::stats_t& stats);

private:
  struct latency_accumulator_t {
    uint64_t sample_count = 0;
    uint64_t total_micros = 0;
    std::vector<uint64_t> samples;
  };

public:
  static constexpr uint32_t MAX_PARTITION_COUNT = 1024;

private:
  void create_workers();
  void destroy_workers();
  [[nodiscard]] bool initialize_topic(QString* error_message) const;
  [[nodiscard]] static bool read_response(QTcpSocket& socket,
                                          protocol_utils_t::response_t* response,
                                          QString* error_message);
  [[nodiscard]] aggregated_stats_t build_aggregated_stats_locked() const;
  static void record_latency_sample(const worker_t::stats_t& previous_stats,
                                    const worker_t::stats_t& current_stats,
                                    latency_accumulator_t& bucket,
                                    latency_accumulator_t& total_bucket);
  [[nodiscard]] static double percentile_ms(const std::vector<uint64_t>& samples, double percentile);
  [[nodiscard]] static double average_latency_ms(const latency_accumulator_t& accumulator);
  [[nodiscard]] static double min_latency_ms(const latency_accumulator_t& accumulator);
  [[nodiscard]] static double max_latency_ms(const latency_accumulator_t& accumulator);
  [[nodiscard]] uint32_t random_partition_for_worker() const noexcept;

  config_t config_;
  std::atomic<bool> running_{false};
  std::atomic<bool> paused_{false};

  QVector<QThread*> writer_threads_;
  QVector<QThread*> reader_threads_;
  QVector<writer_worker_t*> writers_;
  QVector<reader_worker_t*> readers_;

  mutable QMutex stats_mutex_;
  aggregated_stats_t aggregated_stats_;
  QHash<const worker_t*, worker_t::stats_t> writer_stats_by_worker_;
  QHash<const worker_t*, worker_t::stats_t> reader_stats_by_worker_;
  latency_accumulator_t total_latency_;
  latency_accumulator_t writer_latency_;
  latency_accumulator_t reader_latency_;
  std::chrono::steady_clock::time_point start_time_;
};
