#include "main-window.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QSpinBox>
#include <QPushButton>
#include <QTextEdit>
#include <QTimer>
#include <QMessageBox>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent), m_runner(std::make_unique<BenchmarkRunner>(this)), m_updateTimer(new QTimer(this)) {
  setupUi();

  // Connect runner signals
  connect(m_runner.get(), &BenchmarkRunner::statsUpdated, this, &MainWindow::onStatsUpdated);
  connect(m_runner.get(), &BenchmarkRunner::benchmarkStarted, this, &MainWindow::onBenchmarkStarted);
  connect(m_runner.get(), &BenchmarkRunner::benchmarkStopped, this, &MainWindow::onBenchmarkStopped);
  connect(m_runner.get(), &BenchmarkRunner::errorOccurred, this, &MainWindow::onError);

  // Update timer for elapsed time
  connect(m_updateTimer, &QTimer::timeout, [this]() {
    if (m_runner->isRunning()) {
      auto stats = m_runner->getAggregatedStats();
      m_durationLabel->setText(QString::number(stats.requestsPerSecond, 'f', 0) + " RPS");
    }
  });
  m_updateTimer->start(1000);

  // Set initial button states
  m_stopButton->setEnabled(false);
  m_pauseButton->setEnabled(false);
  m_resumeButton->setEnabled(false);
}

MainWindow::~MainWindow() = default;

void MainWindow::setupUi() {
  auto* centralWidget = new QWidget(this);
  auto* mainLayout = new QHBoxLayout(centralWidget);

  // Left panel: Configuration and controls
  auto* leftPanel = new QWidget;
  auto* leftLayout = new QVBoxLayout(leftPanel);

  createConnectionGroup();
  createWorkerConfigGroup();
  createControlButtons();

  leftLayout->addWidget(m_connectionGroup);
  leftLayout->addWidget(m_workerConfigGroup);
  leftLayout->addStretch();
  leftLayout->addWidget(m_controlGroup);

  // Right panel: Statistics display
  auto* rightPanel = new QWidget;
  auto* rightLayout = new QVBoxLayout(rightPanel);

  createStatsDisplay();

  rightLayout->addWidget(m_statusGroup);
  rightLayout->addWidget(m_statsGroup);
  rightLayout->addWidget(m_writerStatsGroup);
  rightLayout->addWidget(m_readerStatsGroup);
  rightLayout->addStretch();

  mainLayout->addWidget(leftPanel, 1);
  mainLayout->addWidget(rightPanel, 1);

  setCentralWidget(centralWidget);
  setWindowTitle("Noctua Benchmark Tool");
  resize(1000, 700);
}

void MainWindow::createConnectionGroup() {
  m_connectionGroup = new QGroupBox("Connection Settings");
  auto* layout = new QFormLayout;

  m_hostEdit = new QLineEdit("localhost");
  m_portSpinBox = new QSpinBox;
  m_portSpinBox->setRange(1, 65535);
  m_portSpinBox->setValue(8080);

  m_topicEdit = new QLineEdit("test_topic");
  m_partitionCountSpinBox = new QSpinBox;
  m_partitionCountSpinBox->setRange(1, static_cast<int>(BenchmarkRunner::kMaxPartitionCount));
  m_partitionCountSpinBox->setValue(1);

  layout->addRow("Host:", m_hostEdit);
  layout->addRow("Port:", m_portSpinBox);
  layout->addRow("Topic:", m_topicEdit);
  layout->addRow("Partition Count:", m_partitionCountSpinBox);

  m_connectionGroup->setLayout(layout);
}

void MainWindow::createWorkerConfigGroup() {
  m_workerConfigGroup = new QGroupBox("Worker Configuration");
  auto* layout = new QFormLayout;

  m_numWritersSpinBox = new QSpinBox;
  m_numWritersSpinBox->setRange(0, 100);
  m_numWritersSpinBox->setValue(1);

  m_numReadersSpinBox = new QSpinBox;
  m_numReadersSpinBox->setRange(0, 100);
  m_numReadersSpinBox->setValue(1);

  m_writerQpsSpinBox = new QSpinBox;
  m_writerQpsSpinBox->setRange(1, 10000);
  m_writerQpsSpinBox->setValue(100);

  m_readerQpsSpinBox = new QSpinBox;
  m_readerQpsSpinBox->setRange(1, 10000);
  m_readerQpsSpinBox->setValue(100);

  m_messageSizeSpinBox = new QSpinBox;
  m_messageSizeSpinBox->setRange(1, 1024 * 1024);
  m_messageSizeSpinBox->setValue(1024);
  m_messageSizeSpinBox->setSuffix(" bytes");

  layout->addRow("Writer Threads:", m_numWritersSpinBox);
  layout->addRow("Reader Threads:", m_numReadersSpinBox);
  layout->addRow("Writer QPS:", m_writerQpsSpinBox);
  layout->addRow("Reader QPS:", m_readerQpsSpinBox);
  layout->addRow("Message Size:", m_messageSizeSpinBox);

  m_workerConfigGroup->setLayout(layout);
}

void MainWindow::createControlButtons() {
  m_controlGroup = new QGroupBox("Controls");
  auto* layout = new QHBoxLayout;

  m_startButton = new QPushButton("Start");
  m_startButton->setStyleSheet("QPushButton { background-color: #4CAF50; color: white; padding: 8px; }");
  m_stopButton = new QPushButton("Stop");
  m_stopButton->setStyleSheet("QPushButton { background-color: #f44336; color: white; padding: 8px; }");
  m_pauseButton = new QPushButton("Pause");
  m_pauseButton->setStyleSheet("QPushButton { background-color: #FF9800; color: white; padding: 8px; }");
  m_resumeButton = new QPushButton("Resume");
  m_resumeButton->setStyleSheet("QPushButton { background-color: #2196F3; color: white; padding: 8px; }");

  connect(m_startButton, &QPushButton::clicked, this, &MainWindow::onStartClicked);
  connect(m_stopButton, &QPushButton::clicked, this, &MainWindow::onStopClicked);
  connect(m_pauseButton, &QPushButton::clicked, this, &MainWindow::onPauseClicked);
  connect(m_resumeButton, &QPushButton::clicked, this, &MainWindow::onResumeClicked);

  layout->addWidget(m_startButton);
  layout->addWidget(m_pauseButton);
  layout->addWidget(m_resumeButton);
  layout->addWidget(m_stopButton);

  m_controlGroup->setLayout(layout);
}

void MainWindow::createStatsDisplay() {
  // Status group
  m_statusGroup = new QGroupBox("Status");
  auto* statusLayout = new QFormLayout;

  m_statusLabel = new QLabel("Stopped");
  m_statusLabel->setStyleSheet("font-weight: bold; color: #666;");
  m_durationLabel = new QLabel("0 RPS");

  statusLayout->addRow("Status:", m_statusLabel);
  statusLayout->addRow("Rate:", m_durationLabel);

  m_statusGroup->setLayout(statusLayout);

  // Overall stats group
  m_statsGroup = new QGroupBox("Overall Statistics");
  auto* statsLayout = new QFormLayout;

  m_totalRequestsLabel = new QLabel("0");
  m_successfulRequestsLabel = new QLabel("0");
  m_failedRequestsLabel = new QLabel("0");
  m_rpsLabel = new QLabel("0");
  m_avgLatencyLabel = new QLabel("0 ms");
  m_minLatencyLabel = new QLabel("0 ms");
  m_maxLatencyLabel = new QLabel("0 ms");
  m_p95LatencyLabel = new QLabel("0 ms");
  m_p99LatencyLabel = new QLabel("0 ms");

  statsLayout->addRow("Total Requests:", m_totalRequestsLabel);
  statsLayout->addRow("Successful:", m_successfulRequestsLabel);
  statsLayout->addRow("Failed:", m_failedRequestsLabel);
  statsLayout->addRow("Avg Latency:", m_avgLatencyLabel);
  statsLayout->addRow("Min Latency:", m_minLatencyLabel);
  statsLayout->addRow("Max Latency:", m_maxLatencyLabel);
  statsLayout->addRow("P95 Latency:", m_p95LatencyLabel);
  statsLayout->addRow("P99 Latency:", m_p99LatencyLabel);

  m_statsGroup->setLayout(statsLayout);

  // Writer stats group
  m_writerStatsGroup = new QGroupBox("Writer Statistics");
  auto* writerStatsLayout = new QFormLayout;

  m_writerRequestsLabel = new QLabel("0");
  m_writerSuccessfulLabel = new QLabel("0");
  m_writerFailedLabel = new QLabel("0");
  m_writerAvgLatencyLabel = new QLabel("0 ms");

  writerStatsLayout->addRow("Total:", m_writerRequestsLabel);
  writerStatsLayout->addRow("Successful:", m_writerSuccessfulLabel);
  writerStatsLayout->addRow("Failed:", m_writerFailedLabel);
  writerStatsLayout->addRow("Avg Latency:", m_writerAvgLatencyLabel);

  m_writerStatsGroup->setLayout(writerStatsLayout);

  // Reader stats group
  m_readerStatsGroup = new QGroupBox("Reader Statistics");
  auto* readerStatsLayout = new QFormLayout;

  m_readerRequestsLabel = new QLabel("0");
  m_readerSuccessfulLabel = new QLabel("0");
  m_readerFailedLabel = new QLabel("0");
  m_readerAvgLatencyLabel = new QLabel("0 ms");

  readerStatsLayout->addRow("Total:", m_readerRequestsLabel);
  readerStatsLayout->addRow("Successful:", m_readerSuccessfulLabel);
  readerStatsLayout->addRow("Failed:", m_readerFailedLabel);
  readerStatsLayout->addRow("Avg Latency:", m_readerAvgLatencyLabel);

  m_readerStatsGroup->setLayout(readerStatsLayout);
}

void MainWindow::onStartClicked() {
  BenchmarkRunner::Config config;
  config.host = m_hostEdit->text();
  config.port = static_cast<quint16>(m_portSpinBox->value());
  config.topicName = m_topicEdit->text();
  config.partitionCount = static_cast<uint32_t>(m_partitionCountSpinBox->value());
  config.numWriters = m_numWritersSpinBox->value();
  config.numReaders = m_numReadersSpinBox->value();
  config.writerQps = m_writerQpsSpinBox->value();
  config.readerQps = m_readerQpsSpinBox->value();
  config.messageSize = m_messageSizeSpinBox->value();

  m_runner->setConfig(config);
  m_runner->start();

  m_startButton->setEnabled(false);
  m_stopButton->setEnabled(true);
  m_pauseButton->setEnabled(true);
  m_resumeButton->setEnabled(false);

  // Disable config inputs
  m_hostEdit->setEnabled(false);
  m_portSpinBox->setEnabled(false);
  m_topicEdit->setEnabled(false);
  m_partitionCountSpinBox->setEnabled(false);
  m_numWritersSpinBox->setEnabled(false);
  m_numReadersSpinBox->setEnabled(false);
  m_writerQpsSpinBox->setEnabled(false);
  m_readerQpsSpinBox->setEnabled(false);
  m_messageSizeSpinBox->setEnabled(false);
}

void MainWindow::onStopClicked() {
  m_runner->stop();

  m_startButton->setEnabled(true);
  m_stopButton->setEnabled(false);
  m_pauseButton->setEnabled(false);
  m_resumeButton->setEnabled(false);

  // Enable config inputs
  m_hostEdit->setEnabled(true);
  m_portSpinBox->setEnabled(true);
  m_topicEdit->setEnabled(true);
  m_partitionCountSpinBox->setEnabled(true);
  m_numWritersSpinBox->setEnabled(true);
  m_numReadersSpinBox->setEnabled(true);
  m_writerQpsSpinBox->setEnabled(true);
  m_readerQpsSpinBox->setEnabled(true);
  m_messageSizeSpinBox->setEnabled(true);
}

void MainWindow::onPauseClicked() {
  m_runner->pause();

  m_pauseButton->setEnabled(false);
  m_resumeButton->setEnabled(true);

  m_statusLabel->setText("Paused");
  m_statusLabel->setStyleSheet("font-weight: bold; color: #FF9800;");
}

void MainWindow::onResumeClicked() {
  m_runner->resume();

  m_pauseButton->setEnabled(true);
  m_resumeButton->setEnabled(false);

  m_statusLabel->setText("Running");
  m_statusLabel->setStyleSheet("font-weight: bold; color: #4CAF50;");
}

void MainWindow::onStatsUpdated(const BenchmarkRunner::AggregatedStats& stats) {
  updateStatsDisplay(stats);
}

void MainWindow::onBenchmarkStarted() {
  m_statusLabel->setText("Running");
  m_statusLabel->setStyleSheet("font-weight: bold; color: #4CAF50;");
}

void MainWindow::onBenchmarkStopped() {
  m_statusLabel->setText("Stopped");
  m_statusLabel->setStyleSheet("font-weight: bold; color: #666;");
  m_durationLabel->setText("0 RPS");
}

void MainWindow::onError(const QString& error) {
  qWarning() << "Benchmark error:" << error;
}

void MainWindow::updateStatsDisplay(const BenchmarkRunner::AggregatedStats& stats) {
  m_totalRequestsLabel->setText(QString::number(stats.totalRequests));
  m_successfulRequestsLabel->setText(QString::number(stats.successfulRequests));
  m_failedRequestsLabel->setText(QString::number(stats.failedRequests));
  m_rpsLabel->setText(QString::number(stats.requestsPerSecond, 'f', 2));
  m_avgLatencyLabel->setText(QString::number(stats.avgLatencyMs, 'f', 3) + " ms");
  m_minLatencyLabel->setText(QString::number(stats.minLatencyMs, 'f', 3) + " ms");
  m_maxLatencyLabel->setText(QString::number(stats.maxLatencyMs, 'f', 3) + " ms");
  m_p95LatencyLabel->setText(QString::number(stats.p95LatencyMs, 'f', 3) + " ms");
  m_p99LatencyLabel->setText(QString::number(stats.p99LatencyMs, 'f', 3) + " ms");

  m_writerRequestsLabel->setText(QString::number(stats.writerTotalRequests));
  m_writerSuccessfulLabel->setText(QString::number(stats.writerSuccessfulRequests));
  m_writerFailedLabel->setText(QString::number(stats.writerFailedRequests));
  m_writerAvgLatencyLabel->setText(QString::number(stats.writerAvgLatencyMs, 'f', 3) + " ms");

  m_readerRequestsLabel->setText(QString::number(stats.readerTotalRequests));
  m_readerSuccessfulLabel->setText(QString::number(stats.readerSuccessfulRequests));
  m_readerFailedLabel->setText(QString::number(stats.readerFailedRequests));
  m_readerAvgLatencyLabel->setText(QString::number(stats.readerAvgLatencyMs, 'f', 3) + " ms");
}
