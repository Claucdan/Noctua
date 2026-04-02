#pragma once

#include <QMainWindow>
#include <QLabel>
#include <QLineEdit>
#include <QSpinBox>
#include <QPushButton>
#include <QGroupBox>
#include <QTimer>
#include <memory>
#include "benchmark-runner.h"

class QGroupBox;

class main_window_t : public QMainWindow {
  Q_OBJECT

public:
  explicit main_window_t(QWidget* parent = nullptr);
  ~main_window_t() override;

private slots:
  void on_start_clicked();
  void on_stop_clicked();
  void on_pause_clicked();
  void on_resume_clicked();
  void on_stats_updated(const benchmark_runner_t::aggregated_stats_t& stats);
  void on_benchmark_started();
  void on_benchmark_stopped();
  void on_error(const QString& error);

private:
  void setup_ui();
  void create_connection_group();
  void create_worker_config_group();
  void create_control_buttons();
  void create_stats_display();
  void update_stats_display(const benchmark_runner_t::aggregated_stats_t& stats);

  std::unique_ptr<benchmark_runner_t> runner_;
  QTimer* update_timer_;

  QGroupBox* connection_group_;
  QGroupBox* worker_config_group_;
  QGroupBox* control_group_;
  QGroupBox* status_group_;
  QGroupBox* stats_group_;
  QGroupBox* writer_stats_group_;
  QGroupBox* reader_stats_group_;

  QLineEdit* host_edit_;
  QSpinBox* port_spin_box_;
  QLineEdit* topic_edit_;
  QSpinBox* partition_count_spin_box_;

  QSpinBox* num_writers_spin_box_;
  QSpinBox* num_readers_spin_box_;
  QSpinBox* writer_qps_spin_box_;
  QSpinBox* reader_qps_spin_box_;
  QSpinBox* message_size_spin_box_;

  QPushButton* start_button_;
  QPushButton* stop_button_;
  QPushButton* pause_button_;
  QPushButton* resume_button_;

  QLabel* status_label_;
  QLabel* duration_label_;
  QLabel* total_requests_label_;
  QLabel* successful_requests_label_;
  QLabel* failed_requests_label_;
  QLabel* rps_label_;
  QLabel* avg_latency_label_;
  QLabel* min_latency_label_;
  QLabel* max_latency_label_;
  QLabel* p95_latency_label_;
  QLabel* p99_latency_label_;

  QLabel* writer_requests_label_;
  QLabel* writer_successful_label_;
  QLabel* writer_failed_label_;
  QLabel* writer_avg_latency_label_;

  QLabel* reader_requests_label_;
  QLabel* reader_successful_label_;
  QLabel* reader_failed_label_;
  QLabel* reader_avg_latency_label_;
};
