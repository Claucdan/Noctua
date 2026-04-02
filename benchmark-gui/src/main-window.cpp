#include "main-window.h"

#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QTimer>
#include <QVBoxLayout>

main_window_t::main_window_t(QWidget* parent)
    : QMainWindow(parent), runner_(std::make_unique<benchmark_runner_t>(this)), update_timer_(new QTimer(this)) {
  setup_ui();

  connect(runner_.get(), &benchmark_runner_t::stats_updated, this, &main_window_t::on_stats_updated);
  connect(runner_.get(), &benchmark_runner_t::benchmark_started, this, &main_window_t::on_benchmark_started);
  connect(runner_.get(), &benchmark_runner_t::benchmark_stopped, this, &main_window_t::on_benchmark_stopped);
  connect(runner_.get(), &benchmark_runner_t::error_occurred, this, &main_window_t::on_error);

  connect(update_timer_, &QTimer::timeout, [this]() {
    if (runner_->is_running()) {
      const auto stats = runner_->get_aggregated_stats();
      duration_label_->setText(QString::number(stats.requests_per_second, 'f', 0) + " RPS");
    }
  });
  update_timer_->start(1000);

  stop_button_->setEnabled(false);
  pause_button_->setEnabled(false);
  resume_button_->setEnabled(false);
}

main_window_t::~main_window_t() = default;

void main_window_t::setup_ui() {
  auto* central_widget = new QWidget(this);
  auto* main_layout = new QHBoxLayout(central_widget);

  auto* left_panel = new QWidget;
  auto* left_layout = new QVBoxLayout(left_panel);

  create_connection_group();
  create_worker_config_group();
  create_control_buttons();

  left_layout->addWidget(connection_group_);
  left_layout->addWidget(worker_config_group_);
  left_layout->addStretch();
  left_layout->addWidget(control_group_);

  auto* right_panel = new QWidget;
  auto* right_layout = new QVBoxLayout(right_panel);

  create_stats_display();

  right_layout->addWidget(status_group_);
  right_layout->addWidget(stats_group_);
  right_layout->addWidget(writer_stats_group_);
  right_layout->addWidget(reader_stats_group_);
  right_layout->addStretch();

  main_layout->addWidget(left_panel, 1);
  main_layout->addWidget(right_panel, 1);

  setCentralWidget(central_widget);
  setWindowTitle("Noctua Benchmark Tool");
  resize(1000, 700);
}

void main_window_t::create_connection_group() {
  connection_group_ = new QGroupBox("Connection Settings");
  auto* layout = new QFormLayout;

  host_edit_ = new QLineEdit("localhost");
  port_spin_box_ = new QSpinBox;
  port_spin_box_->setRange(1, 65535);
  port_spin_box_->setValue(8080);

  topic_edit_ = new QLineEdit("test_topic");
  partition_count_spin_box_ = new QSpinBox;
  partition_count_spin_box_->setRange(1, static_cast<int>(benchmark_runner_t::MAX_PARTITION_COUNT));
  partition_count_spin_box_->setValue(1);

  layout->addRow("Host:", host_edit_);
  layout->addRow("Port:", port_spin_box_);
  layout->addRow("Topic:", topic_edit_);
  layout->addRow("Partition Count:", partition_count_spin_box_);

  connection_group_->setLayout(layout);
}

void main_window_t::create_worker_config_group() {
  worker_config_group_ = new QGroupBox("Worker Configuration");
  auto* layout = new QFormLayout;

  num_writers_spin_box_ = new QSpinBox;
  num_writers_spin_box_->setRange(0, 100);
  num_writers_spin_box_->setValue(1);

  num_readers_spin_box_ = new QSpinBox;
  num_readers_spin_box_->setRange(0, 100);
  num_readers_spin_box_->setValue(1);

  writer_qps_spin_box_ = new QSpinBox;
  writer_qps_spin_box_->setRange(1, 10000);
  writer_qps_spin_box_->setValue(100);

  reader_qps_spin_box_ = new QSpinBox;
  reader_qps_spin_box_->setRange(1, 10000);
  reader_qps_spin_box_->setValue(100);

  message_size_spin_box_ = new QSpinBox;
  message_size_spin_box_->setRange(1, 1024 * 1024);
  message_size_spin_box_->setValue(1024);
  message_size_spin_box_->setSuffix(" bytes");

  layout->addRow("Writer Threads:", num_writers_spin_box_);
  layout->addRow("Reader Threads:", num_readers_spin_box_);
  layout->addRow("Writer QPS:", writer_qps_spin_box_);
  layout->addRow("Reader QPS:", reader_qps_spin_box_);
  layout->addRow("Message Size:", message_size_spin_box_);

  worker_config_group_->setLayout(layout);
}

void main_window_t::create_control_buttons() {
  control_group_ = new QGroupBox("Controls");
  auto* layout = new QHBoxLayout;

  start_button_ = new QPushButton("Start");
  start_button_->setStyleSheet("QPushButton { background-color: #4CAF50; color: white; padding: 8px; }");
  stop_button_ = new QPushButton("Stop");
  stop_button_->setStyleSheet("QPushButton { background-color: #f44336; color: white; padding: 8px; }");
  pause_button_ = new QPushButton("Pause");
  pause_button_->setStyleSheet("QPushButton { background-color: #FF9800; color: white; padding: 8px; }");
  resume_button_ = new QPushButton("Resume");
  resume_button_->setStyleSheet("QPushButton { background-color: #2196F3; color: white; padding: 8px; }");

  connect(start_button_, &QPushButton::clicked, this, &main_window_t::on_start_clicked);
  connect(stop_button_, &QPushButton::clicked, this, &main_window_t::on_stop_clicked);
  connect(pause_button_, &QPushButton::clicked, this, &main_window_t::on_pause_clicked);
  connect(resume_button_, &QPushButton::clicked, this, &main_window_t::on_resume_clicked);

  layout->addWidget(start_button_);
  layout->addWidget(pause_button_);
  layout->addWidget(resume_button_);
  layout->addWidget(stop_button_);

  control_group_->setLayout(layout);
}

void main_window_t::create_stats_display() {
  status_group_ = new QGroupBox("Status");
  auto* status_layout = new QFormLayout;

  status_label_ = new QLabel("Stopped");
  status_label_->setStyleSheet("font-weight: bold; color: #666;");
  duration_label_ = new QLabel("0 RPS");

  status_layout->addRow("Status:", status_label_);
  status_layout->addRow("Rate:", duration_label_);

  status_group_->setLayout(status_layout);

  stats_group_ = new QGroupBox("Overall Statistics");
  auto* stats_layout = new QFormLayout;

  total_requests_label_ = new QLabel("0");
  successful_requests_label_ = new QLabel("0");
  failed_requests_label_ = new QLabel("0");
  rps_label_ = new QLabel("0");
  avg_latency_label_ = new QLabel("0 ms");
  min_latency_label_ = new QLabel("0 ms");
  max_latency_label_ = new QLabel("0 ms");
  p95_latency_label_ = new QLabel("0 ms");
  p99_latency_label_ = new QLabel("0 ms");

  stats_layout->addRow("Total Requests:", total_requests_label_);
  stats_layout->addRow("Successful:", successful_requests_label_);
  stats_layout->addRow("Failed:", failed_requests_label_);
  stats_layout->addRow("Avg Latency:", avg_latency_label_);
  stats_layout->addRow("Min Latency:", min_latency_label_);
  stats_layout->addRow("Max Latency:", max_latency_label_);
  stats_layout->addRow("P95 Latency:", p95_latency_label_);
  stats_layout->addRow("P99 Latency:", p99_latency_label_);

  stats_group_->setLayout(stats_layout);

  writer_stats_group_ = new QGroupBox("Writer Statistics");
  auto* writer_stats_layout = new QFormLayout;

  writer_requests_label_ = new QLabel("0");
  writer_successful_label_ = new QLabel("0");
  writer_failed_label_ = new QLabel("0");
  writer_avg_latency_label_ = new QLabel("0 ms");

  writer_stats_layout->addRow("Total:", writer_requests_label_);
  writer_stats_layout->addRow("Successful:", writer_successful_label_);
  writer_stats_layout->addRow("Failed:", writer_failed_label_);
  writer_stats_layout->addRow("Avg Latency:", writer_avg_latency_label_);

  writer_stats_group_->setLayout(writer_stats_layout);

  reader_stats_group_ = new QGroupBox("Reader Statistics");
  auto* reader_stats_layout = new QFormLayout;

  reader_requests_label_ = new QLabel("0");
  reader_successful_label_ = new QLabel("0");
  reader_failed_label_ = new QLabel("0");
  reader_avg_latency_label_ = new QLabel("0 ms");

  reader_stats_layout->addRow("Total:", reader_requests_label_);
  reader_stats_layout->addRow("Successful:", reader_successful_label_);
  reader_stats_layout->addRow("Failed:", reader_failed_label_);
  reader_stats_layout->addRow("Avg Latency:", reader_avg_latency_label_);

  reader_stats_group_->setLayout(reader_stats_layout);
}

void main_window_t::on_start_clicked() {
  benchmark_runner_t::config_t config;
  config.host = host_edit_->text();
  config.port = static_cast<quint16>(port_spin_box_->value());
  config.topic_name = topic_edit_->text();
  config.partition_count = static_cast<uint32_t>(partition_count_spin_box_->value());
  config.num_writers = num_writers_spin_box_->value();
  config.num_readers = num_readers_spin_box_->value();
  config.writer_qps = writer_qps_spin_box_->value();
  config.reader_qps = reader_qps_spin_box_->value();
  config.message_size = message_size_spin_box_->value();

  runner_->set_config(config);
  runner_->start();

  start_button_->setEnabled(false);
  stop_button_->setEnabled(true);
  pause_button_->setEnabled(true);
  resume_button_->setEnabled(false);

  host_edit_->setEnabled(false);
  port_spin_box_->setEnabled(false);
  topic_edit_->setEnabled(false);
  partition_count_spin_box_->setEnabled(false);
  num_writers_spin_box_->setEnabled(false);
  num_readers_spin_box_->setEnabled(false);
  writer_qps_spin_box_->setEnabled(false);
  reader_qps_spin_box_->setEnabled(false);
  message_size_spin_box_->setEnabled(false);
}

void main_window_t::on_stop_clicked() {
  runner_->stop();

  start_button_->setEnabled(true);
  stop_button_->setEnabled(false);
  pause_button_->setEnabled(false);
  resume_button_->setEnabled(false);

  host_edit_->setEnabled(true);
  port_spin_box_->setEnabled(true);
  topic_edit_->setEnabled(true);
  partition_count_spin_box_->setEnabled(true);
  num_writers_spin_box_->setEnabled(true);
  num_readers_spin_box_->setEnabled(true);
  writer_qps_spin_box_->setEnabled(true);
  reader_qps_spin_box_->setEnabled(true);
  message_size_spin_box_->setEnabled(true);
}

void main_window_t::on_pause_clicked() {
  runner_->pause();

  pause_button_->setEnabled(false);
  resume_button_->setEnabled(true);

  status_label_->setText("Paused");
  status_label_->setStyleSheet("font-weight: bold; color: #FF9800;");
}

void main_window_t::on_resume_clicked() {
  runner_->resume();

  pause_button_->setEnabled(true);
  resume_button_->setEnabled(false);

  status_label_->setText("Running");
  status_label_->setStyleSheet("font-weight: bold; color: #4CAF50;");
}

void main_window_t::on_stats_updated(const benchmark_runner_t::aggregated_stats_t& stats) {
  update_stats_display(stats);
}

void main_window_t::on_benchmark_started() {
  status_label_->setText("Running");
  status_label_->setStyleSheet("font-weight: bold; color: #4CAF50;");
}

void main_window_t::on_benchmark_stopped() {
  status_label_->setText("Stopped");
  status_label_->setStyleSheet("font-weight: bold; color: #666;");
  duration_label_->setText("0 RPS");
}

void main_window_t::on_error(const QString& error) {
  qWarning() << "Benchmark error:" << error;
}

void main_window_t::update_stats_display(const benchmark_runner_t::aggregated_stats_t& stats) {
  total_requests_label_->setText(QString::number(stats.total_requests));
  successful_requests_label_->setText(QString::number(stats.successful_requests));
  failed_requests_label_->setText(QString::number(stats.failed_requests));
  rps_label_->setText(QString::number(stats.requests_per_second, 'f', 2));
  avg_latency_label_->setText(QString::number(stats.avg_latency_ms, 'f', 3) + " ms");
  min_latency_label_->setText(QString::number(stats.min_latency_ms, 'f', 3) + " ms");
  max_latency_label_->setText(QString::number(stats.max_latency_ms, 'f', 3) + " ms");
  p95_latency_label_->setText(QString::number(stats.p95_latency_ms, 'f', 3) + " ms");
  p99_latency_label_->setText(QString::number(stats.p99_latency_ms, 'f', 3) + " ms");

  writer_requests_label_->setText(QString::number(stats.writer_total_requests));
  writer_successful_label_->setText(QString::number(stats.writer_successful_requests));
  writer_failed_label_->setText(QString::number(stats.writer_failed_requests));
  writer_avg_latency_label_->setText(QString::number(stats.writer_avg_latency_ms, 'f', 3) + " ms");

  reader_requests_label_->setText(QString::number(stats.reader_total_requests));
  reader_successful_label_->setText(QString::number(stats.reader_successful_requests));
  reader_failed_label_->setText(QString::number(stats.reader_failed_requests));
  reader_avg_latency_label_->setText(QString::number(stats.reader_avg_latency_ms, 'f', 3) + " ms");
}
