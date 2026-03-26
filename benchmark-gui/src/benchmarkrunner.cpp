#include "benchmarkrunner.h"
#include <algorithm>

BenchmarkRunner::BenchmarkRunner(QObject* parent)
    : QObject(parent)
{
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

    // Calculate RPS based on elapsed time
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
        now - m_startTime).count();

    AggregatedStats stats = m_aggregatedStats;
    if (elapsed > 0) {
        stats.requestsPerSecond = static_cast<double>(stats.totalRequests) / elapsed;
    }

    return stats;
}

void BenchmarkRunner::onWorkerStatsUpdated(const Worker::Stats& stats) {
    QMutexLocker locker(&m_statsMutex);

    m_aggregatedStats.totalRequests += 1;  // Increment on each update

    if (stats.successfulRequests > 0 || stats.failedRequests > 0) {
        m_aggregatedStats.successfulRequests = stats.successfulRequests;
        m_aggregatedStats.failedRequests = stats.failedRequests;
        m_aggregatedStats.avgLatencyMs = stats.avgLatencyMs;
        m_aggregatedStats.minLatencyMs = stats.minLatencyMs;
        m_aggregatedStats.maxLatencyMs = stats.maxLatencyMs;
        m_aggregatedStats.p95LatencyMs = stats.p95LatencyMs;
        m_aggregatedStats.p99LatencyMs = stats.p99LatencyMs;
    }

    // Update individual worker stats
    auto writer = qobject_cast<WriterWorker*>(sender());
    if (writer) {
        m_aggregatedStats.writerTotalRequests = stats.totalRequests;
        m_aggregatedStats.writerSuccessfulRequests = stats.successfulRequests;
        m_aggregatedStats.writerFailedRequests = stats.failedRequests;
        m_aggregatedStats.writerAvgLatencyMs = stats.avgLatencyMs;
    }

    auto reader = qobject_cast<ReaderWorker*>(sender());
    if (reader) {
        m_aggregatedStats.readerTotalRequests = stats.totalRequests;
        m_aggregatedStats.readerSuccessfulRequests = stats.successfulRequests;
        m_aggregatedStats.readerFailedRequests = stats.failedRequests;
        m_aggregatedStats.readerAvgLatencyMs = stats.avgLatencyMs;
    }

    emit statsUpdated(getAggregatedStats());
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
}
