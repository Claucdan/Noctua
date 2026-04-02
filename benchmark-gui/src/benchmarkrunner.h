#pragma once

#include <QObject>
#include <QVector>
#include <QMutex>
#include <QHash>
#include <QTcpSocket>
#include <memory>
#include <chrono>
#include <vector>
#include "protocolutils.h"
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
        uint32_t partitionCount = 1;
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
    struct LatencyAccumulator {
        uint64_t sampleCount = 0;
        uint64_t totalMicros = 0;
        std::vector<uint64_t> samples;
    };

public:
    static constexpr uint32_t kMaxPartitionCount = 1024;

private:
    void createWorkers();
    void destroyWorkers();
    [[nodiscard]] bool initializeTopic(QString* errorMessage) const;
    [[nodiscard]] static bool readResponse(QTcpSocket& socket, ProtocolUtils::Response* response, QString* errorMessage);
    [[nodiscard]] AggregatedStats buildAggregatedStatsLocked() const;
    static void recordLatencySample(const Worker::Stats& previousStats,
                                    const Worker::Stats& currentStats,
                                    LatencyAccumulator& bucket,
                                    LatencyAccumulator& totalBucket);
    [[nodiscard]] static double percentileMs(const std::vector<uint64_t>& samples, double percentile);
    [[nodiscard]] static double averageLatencyMs(const LatencyAccumulator& accumulator);
    [[nodiscard]] static double minLatencyMs(const LatencyAccumulator& accumulator);
    [[nodiscard]] static double maxLatencyMs(const LatencyAccumulator& accumulator);
    [[nodiscard]] uint32_t randomPartitionForWorker() const noexcept;

    Config m_config;
    std::atomic<bool> m_running{false};
    std::atomic<bool> m_paused{false};

    QVector<QThread*> m_writerThreads;
    QVector<QThread*> m_readerThreads;
    QVector<WriterWorker*> m_writers;
    QVector<ReaderWorker*> m_readers;

    mutable QMutex m_statsMutex;
    AggregatedStats m_aggregatedStats;
    QHash<const Worker*, Worker::Stats> m_writerStatsByWorker;
    QHash<const Worker*, Worker::Stats> m_readerStatsByWorker;
    LatencyAccumulator m_totalLatency;
    LatencyAccumulator m_writerLatency;
    LatencyAccumulator m_readerLatency;
    std::chrono::steady_clock::time_point m_startTime;
};
