#include "benchmarkrunner.h"

#include <algorithm>
#include <limits>

BenchmarkRunner::BenchmarkRunner(QObject* parent)
    : QObject(parent)
{
    qRegisterMetaType<Worker::Stats>("Worker::Stats");
}

BenchmarkRunner::~BenchmarkRunner() {
    stop();
}

void BenchmarkRunner::setConfig(const Config& config) {
    m_config = config;
}

void BenchmarkRunner::start() {
    if (m_running.load()) {
        return;
    }

    stop();  // Ensure clean state

    m_running.store(true);
    m_paused.store(false);
    m_startTime = std::chrono::steady_clock::now();

    // Reset stats
    {
        QMutexLocker locker(&m_statsMutex);
        m_aggregatedStats = AggregatedStats{};
        m_writerStatsByWorker.clear();
        m_readerStatsByWorker.clear();
        m_totalLatency = LatencyAccumulator{};
        m_writerLatency = LatencyAccumulator{};
        m_readerLatency = LatencyAccumulator{};
    }

    createWorkers();

    emit benchmarkStarted();
}

void BenchmarkRunner::stop() {
    if (!m_running.load()) {
        return;
    }

    m_running.store(false);
    m_paused.store(false);

    destroyWorkers();

    emit benchmarkStopped();
}

void BenchmarkRunner::pause() {
    if (!m_running.load() || m_paused.load()) {
        return;
    }

    m_paused.store(true);

    for (auto* writer : m_writers) {
        writer->pause();
    }
    for (auto* reader : m_readers) {
        reader->pause();
    }
}

void BenchmarkRunner::resume() {
    if (!m_running.load() || !m_paused.load()) {
        return;
    }

    m_paused.store(false);

    for (auto* writer : m_writers) {
        writer->resume();
    }
    for (auto* reader : m_readers) {
        reader->resume();
    }
}

BenchmarkRunner::AggregatedStats BenchmarkRunner::getAggregatedStats() const {
    QMutexLocker locker(&m_statsMutex);
    return buildAggregatedStatsLocked();
}

void BenchmarkRunner::onWorkerStatsUpdated(const Worker::Stats& stats) {
    QMutexLocker locker(&m_statsMutex);
    auto* worker = qobject_cast<Worker*>(sender());
    if (worker == nullptr) {
        return;
    }

    if (qobject_cast<WriterWorker*>(worker) != nullptr) {
        recordLatencySample(m_writerStatsByWorker.value(worker), stats, m_writerLatency, m_totalLatency);
        m_writerStatsByWorker.insert(worker, stats);
    } else if (qobject_cast<ReaderWorker*>(worker) != nullptr) {
        recordLatencySample(m_readerStatsByWorker.value(worker), stats, m_readerLatency, m_totalLatency);
        m_readerStatsByWorker.insert(worker, stats);
    } else {
        return;
    }

    m_aggregatedStats = buildAggregatedStatsLocked();
    emit statsUpdated(m_aggregatedStats);
}

void BenchmarkRunner::createWorkers() {
    destroyWorkers();

    // Create writer workers
    m_writers.reserve(m_config.numWriters);
    m_writerThreads.reserve(m_config.numWriters);

    for (int i = 0; i < m_config.numWriters; ++i) {
        auto* thread = new QThread(this);
        auto* worker = new WriterWorker(
            m_config.host, m_config.port,
            m_config.topicName, m_config.partitionId,
            m_config.writerQps, m_config.messageSize);

        connect(worker, &Worker::statsUpdated,
                this, &BenchmarkRunner::onWorkerStatsUpdated);
        connect(worker, &Worker::errorOccurred,
                this, &BenchmarkRunner::errorOccurred);
        connect(worker, &QObject::destroyed, this, [this, worker]() {
            QMutexLocker locker(&m_statsMutex);
            m_writerStatsByWorker.remove(worker);
        });

        worker->moveToThread(thread);

        connect(thread, &QThread::started, worker, &Worker::start);
        connect(thread, &QThread::finished, worker, &QObject::deleteLater);

        m_writerThreads.append(thread);
        m_writers.append(worker);
        thread->start();
    }

    // Create reader workers
    m_readers.reserve(m_config.numReaders);
    m_readerThreads.reserve(m_config.numReaders);

    for (int i = 0; i < m_config.numReaders; ++i) {
        auto* thread = new QThread(this);
        auto* worker = new ReaderWorker(
            m_config.host, m_config.port,
            m_config.topicName, m_config.partitionId,
            m_config.readerQps);

        connect(worker, &Worker::statsUpdated,
                this, &BenchmarkRunner::onWorkerStatsUpdated);
        connect(worker, &Worker::errorOccurred,
                this, &BenchmarkRunner::errorOccurred);
        connect(worker, &QObject::destroyed, this, [this, worker]() {
            QMutexLocker locker(&m_statsMutex);
            m_readerStatsByWorker.remove(worker);
        });

        worker->moveToThread(thread);

        connect(thread, &QThread::started, worker, &Worker::start);
        connect(thread, &QThread::finished, worker, &QObject::deleteLater);

        m_readerThreads.append(thread);
        m_readers.append(worker);
        thread->start();
    }
}

void BenchmarkRunner::destroyWorkers() {
    // Stop all workers
    for (auto* worker : m_writers) {
        worker->stop();
    }
    for (auto* worker : m_readers) {
        worker->stop();
    }

    // Wait for threads to finish
    for (auto* thread : m_writerThreads) {
        thread->quit();
        thread->wait(1000);
    }
    for (auto* thread : m_readerThreads) {
        thread->quit();
        thread->wait(1000);
    }

    m_writers.clear();
    m_readers.clear();
    m_writerThreads.clear();
    m_readerThreads.clear();

    QMutexLocker locker(&m_statsMutex);
    m_writerStatsByWorker.clear();
    m_readerStatsByWorker.clear();
    m_totalLatency = LatencyAccumulator{};
    m_writerLatency = LatencyAccumulator{};
    m_readerLatency = LatencyAccumulator{};
}

BenchmarkRunner::AggregatedStats BenchmarkRunner::buildAggregatedStatsLocked() const {
    AggregatedStats stats{};
    for (const auto& worker : m_writerStatsByWorker) {
        stats.writerTotalRequests += worker.totalRequests;
        stats.writerSuccessfulRequests += worker.successfulRequests;
        stats.writerFailedRequests += worker.failedRequests;
    }
    for (const auto& worker : m_readerStatsByWorker) {
        stats.readerTotalRequests += worker.totalRequests;
        stats.readerSuccessfulRequests += worker.successfulRequests;
        stats.readerFailedRequests += worker.failedRequests;
    }

    stats.totalRequests = stats.writerTotalRequests + stats.readerTotalRequests;
    stats.successfulRequests = stats.writerSuccessfulRequests + stats.readerSuccessfulRequests;
    stats.failedRequests = stats.writerFailedRequests + stats.readerFailedRequests;

    stats.avgLatencyMs = averageLatencyMs(m_totalLatency);
    stats.minLatencyMs = minLatencyMs(m_totalLatency);
    stats.maxLatencyMs = maxLatencyMs(m_totalLatency);
    stats.p95LatencyMs = percentileMs(m_totalLatency.samples, 0.95);
    stats.p99LatencyMs = percentileMs(m_totalLatency.samples, 0.99);
    stats.writerAvgLatencyMs = averageLatencyMs(m_writerLatency);
    stats.readerAvgLatencyMs = averageLatencyMs(m_readerLatency);

    const auto now = std::chrono::steady_clock::now();
    const auto elapsedMicros =
        std::chrono::duration_cast<std::chrono::microseconds>(now - m_startTime).count();
    if (elapsedMicros > 0) {
        stats.requestsPerSecond =
            static_cast<double>(stats.totalRequests) * 1'000'000.0 / static_cast<double>(elapsedMicros);
    }

    return stats;
}

void BenchmarkRunner::recordLatencySample(const Worker::Stats& previousStats,
                                          const Worker::Stats& currentStats,
                                          LatencyAccumulator& bucket,
                                          LatencyAccumulator& totalBucket) {
    if (!currentStats.hasLatencySample || currentStats.latencySampleCount <= previousStats.latencySampleCount) {
        return;
    }

    bucket.sampleCount++;
    bucket.totalMicros += currentStats.lastLatencyMicros;
    bucket.samples.push_back(currentStats.lastLatencyMicros);

    totalBucket.sampleCount++;
    totalBucket.totalMicros += currentStats.lastLatencyMicros;
    totalBucket.samples.push_back(currentStats.lastLatencyMicros);
}

double BenchmarkRunner::percentileMs(const std::vector<uint64_t>& samples, double percentile) {
    if (samples.empty()) {
        return 0.0;
    }

    auto sorted = samples;
    std::sort(sorted.begin(), sorted.end());
    const auto index = static_cast<size_t>((sorted.size() - 1) * percentile);
    return static_cast<double>(sorted[index]) / 1000.0;
}

double BenchmarkRunner::averageLatencyMs(const LatencyAccumulator& accumulator) {
    if (accumulator.sampleCount == 0) {
        return 0.0;
    }
    return static_cast<double>(accumulator.totalMicros) /
           static_cast<double>(accumulator.sampleCount) /
           1000.0;
}

double BenchmarkRunner::minLatencyMs(const LatencyAccumulator& accumulator) {
    if (accumulator.samples.empty()) {
        return 0.0;
    }
    return static_cast<double>(*std::min_element(accumulator.samples.begin(), accumulator.samples.end())) / 1000.0;
}

double BenchmarkRunner::maxLatencyMs(const LatencyAccumulator& accumulator) {
    if (accumulator.samples.empty()) {
        return 0.0;
    }
    return static_cast<double>(*std::max_element(accumulator.samples.begin(), accumulator.samples.end())) / 1000.0;
}
