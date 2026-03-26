#pragma once

#include <QObject>
#include <QVector>
#include <QMutex>
#include <memory>
#include "worker.h"
#include "writerworker.h"
#include "readerworker.h"

class BenchmarkRunner : public QObject {
    Q_OBJECT

public:
    struct Config {
        QString host = "localhost";
        quint16 port = 8080;
        QString topicName = "test_topic";
        uint32_t partitionId = 0;
        int numWriters = 1;
        int numReaders = 1;
        int writerQps = 100;
        int readerQps = 100;
        int messageSize = 1024;
    };

    explicit BenchmarkRunner(QObject* parent = nullptr);
    ~BenchmarkRunner() override;

    void setConfig(const Config& config);

    void start();
    void stop();
    void pause();
    void resume();

    bool isRunning() const { return m_running.load(); }

    struct AggregatedStats {
        uint64_t totalRequests = 0;
        uint64_t successfulRequests = 0;
        uint64_t failedRequests = 0;
        double avgLatencyMs = 0.0;
        double minLatencyMs = 0.0;
        double maxLatencyMs = 0.0;
        double p95LatencyMs = 0.0;
        double p99LatencyMs = 0.0;
        double requestsPerSecond = 0.0;

        uint64_t writerTotalRequests = 0;
        uint64_t writerSuccessfulRequests = 0;
        uint64_t writerFailedRequests = 0;
        double writerAvgLatencyMs = 0.0;

        uint64_t readerTotalRequests = 0;
        uint64_t readerSuccessfulRequests = 0;
        uint64_t readerFailedRequests = 0;
        double readerAvgLatencyMs = 0.0;
    };

    AggregatedStats getAggregatedStats() const;

signals:
    void statsUpdated(const BenchmarkRunner::AggregatedStats& stats);
    void benchmarkStarted();
    void benchmarkStopped();
    void errorOccurred(const QString& error);

private slots:
    void onWorkerStatsUpdated(const Worker::Stats& stats);

private:
    void createWorkers();
    void destroyWorkers();

    Config m_config;
    std::atomic<bool> m_running{false};
    std::atomic<bool> m_paused{false};

    QVector<QThread*> m_writerThreads;
    QVector<QThread*> m_readerThreads;
    QVector<WriterWorker*> m_writers;
    QVector<ReaderWorker*> m_readers;

    mutable QMutex m_statsMutex;
    AggregatedStats m_aggregatedStats;
    std::chrono::steady_clock::time_point m_startTime;
};
