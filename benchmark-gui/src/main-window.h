#pragma once

#include <QMainWindow>
#include <QLabel>
#include <QLineEdit>
#include <QSpinBox>
#include <QPushButton>
#include <QGroupBox>
#include <QTextEdit>
#include <QProgressBar>
#include <QTimer>
#include <memory>
#include "benchmark-runner.h"

class QGroupBox;

class MainWindow : public QMainWindow {
  Q_OBJECT

public:
  explicit MainWindow(QWidget* parent = nullptr);
  ~MainWindow() override;

private slots:
  void onStartClicked();
  void onStopClicked();
  void onPauseClicked();
  void onResumeClicked();
  void onStatsUpdated(const BenchmarkRunner::AggregatedStats& stats);
  void onBenchmarkStarted();
  void onBenchmarkStopped();
  void onError(const QString& error);

private:
  void setupUi();
  void createConnectionGroup();
  void createWorkerConfigGroup();
  void createControlButtons();
  void createStatsDisplay();
  void updateStatsDisplay(const BenchmarkRunner::AggregatedStats& stats);

  std::unique_ptr<BenchmarkRunner> m_runner;
  QTimer* m_updateTimer;

  // UI Groups
  QGroupBox* m_connectionGroup;
  QGroupBox* m_workerConfigGroup;
  QGroupBox* m_controlGroup;
  QGroupBox* m_statusGroup;
  QGroupBox* m_statsGroup;
  QGroupBox* m_writerStatsGroup;
  QGroupBox* m_readerStatsGroup;

  // Connection settings
  QLineEdit* m_hostEdit;
  QSpinBox* m_portSpinBox;
  QLineEdit* m_topicEdit;
  QSpinBox* m_partitionCountSpinBox;

  // Worker configuration
  QSpinBox* m_numWritersSpinBox;
  QSpinBox* m_numReadersSpinBox;
  QSpinBox* m_writerQpsSpinBox;
  QSpinBox* m_readerQpsSpinBox;
  QSpinBox* m_messageSizeSpinBox;

  // Control buttons
  QPushButton* m_startButton;
  QPushButton* m_stopButton;
  QPushButton* m_pauseButton;
  QPushButton* m_resumeButton;

  // Stats display
  QLabel* m_statusLabel;
  QLabel* m_durationLabel;
  QLabel* m_totalRequestsLabel;
  QLabel* m_successfulRequestsLabel;
  QLabel* m_failedRequestsLabel;
  QLabel* m_rpsLabel;
  QLabel* m_avgLatencyLabel;
  QLabel* m_minLatencyLabel;
  QLabel* m_maxLatencyLabel;
  QLabel* m_p95LatencyLabel;
  QLabel* m_p99LatencyLabel;

  // Writer stats
  QLabel* m_writerRequestsLabel;
  QLabel* m_writerSuccessfulLabel;
  QLabel* m_writerFailedLabel;
  QLabel* m_writerAvgLatencyLabel;

  // Reader stats
  QLabel* m_readerRequestsLabel;
  QLabel* m_readerSuccessfulLabel;
  QLabel* m_readerFailedLabel;
  QLabel* m_readerAvgLatencyLabel;
};
