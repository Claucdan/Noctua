#include "benchmarkrunner.h"

#include "protocolutils.h"

#include <QMetaObject>
#include <QRandomGenerator>

#include <algorithm>
#include <cstring>
#include <limits>
#include <utility>

namespace {

template <typename Fn>
void invoke_on_worker_thread(Worker* worker, Fn&& fn, Qt::ConnectionType type) {
    if (worker == nullptr) {
        return;
    }

    auto* targetThread = worker->thread();
    if (targetThread == nullptr || targetThread == QThread::currentThread() || !targetThread->isRunning()) {
        fn();
        return;
    }

    QMetaObject::invokeMethod(
        worker,
        std::forward<Fn>(fn),
        type);
}

} // namespace

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

    QString initializationError;
    if (!initializeTopic(&initializationError)) {
        m_running.store(false);
        emit errorOccurred(initializationError);
        return;
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
        invoke_on_worker_thread(writer, [writer]() { writer->pause(); }, Qt::QueuedConnection);
    }
    for (auto* reader : m_readers) {
        invoke_on_worker_thread(reader, [reader]() { reader->pause(); }, Qt::QueuedConnection);
    }
}

void BenchmarkRunner::resume() {
    if (!m_running.load() || !m_paused.load()) {
        return;
    }

    m_paused.store(false);

    for (auto* writer : m_writers) {
        invoke_on_worker_thread(writer, [writer]() { writer->resume(); }, Qt::QueuedConnection);
    }
    for (auto* reader : m_readers) {
        invoke_on_worker_thread(reader, [reader]() { reader->resume(); }, Qt::QueuedConnection);
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
        const auto partitionId = randomPartitionForWorker();
        auto* worker = new WriterWorker(
            m_config.host, m_config.port,
            m_config.topicName, partitionId,
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
        const auto partitionId = randomPartitionForWorker();
        auto* worker = new ReaderWorker(
            m_config.host, m_config.port,
            m_config.topicName, partitionId,
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
    for (auto* worker : m_writers) {
        invoke_on_worker_thread(worker, [worker]() { worker->stop(); }, Qt::BlockingQueuedConnection);
    }
    for (auto* worker : m_readers) {
        invoke_on_worker_thread(worker, [worker]() { worker->stop(); }, Qt::BlockingQueuedConnection);
    }

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

bool BenchmarkRunner::initializeTopic(QString* errorMessage) const {
    if (m_config.partitionCount == 0) {
        if (errorMessage != nullptr) {
            *errorMessage = "Partition count must be greater than zero";
        }
        return false;
    }

    QTcpSocket socket;
    socket.connectToHost(m_config.host, m_config.port);
    if (!socket.waitForConnected(5000)) {
        if (errorMessage != nullptr) {
            *errorMessage = QString("Failed to connect for topic initialization: %1")
                .arg(socket.errorString());
        }
        return false;
    }

    const auto initPartition = static_cast<uint16_t>(m_config.partitionCount - 1);
    const QByteArray initMessage = "__benchmark_topic_init__";

    const auto pushRequest =
        ProtocolUtils::createPushRequest(m_config.topicName.toUtf8(), initPartition, initMessage);
    if (socket.write(pushRequest) != pushRequest.size() || !socket.waitForBytesWritten(5000)) {
        if (errorMessage != nullptr) {
            *errorMessage = QString("Failed to write initialization PUSH: %1")
                .arg(socket.errorString());
        }
        return false;
    }

    ProtocolUtils::Response pushResponse;
    if (!readResponse(socket, &pushResponse, errorMessage)) {
        return false;
    }
    if (!pushResponse.valid ||
        pushResponse.errorCode != ProtocolUtils::ErrorCode::OK ||
        pushResponse.opcode != ProtocolUtils::Opcode::PUSH) {
        if (errorMessage != nullptr) {
            *errorMessage = QString("Initialization PUSH failed: %1")
                .arg(QString::fromUtf8(pushResponse.message));
        }
        return false;
    }

    const auto deleteRequest =
        ProtocolUtils::createDeleteRequest(m_config.topicName.toUtf8(), initPartition, initMessage);
    if (socket.write(deleteRequest) != deleteRequest.size() || !socket.waitForBytesWritten(5000)) {
        if (errorMessage != nullptr) {
            *errorMessage = QString("Failed to write initialization DELETE: %1")
                .arg(socket.errorString());
        }
        return false;
    }

    ProtocolUtils::Response deleteResponse;
    if (!readResponse(socket, &deleteResponse, errorMessage)) {
        return false;
    }
    if (!deleteResponse.valid ||
        deleteResponse.errorCode != ProtocolUtils::ErrorCode::OK ||
        deleteResponse.opcode != ProtocolUtils::Opcode::DELETE) {
        if (errorMessage != nullptr) {
            *errorMessage = QString("Initialization DELETE failed: %1")
                .arg(QString::fromUtf8(deleteResponse.message));
        }
        return false;
    }

    socket.disconnectFromHost();
    return true;
}

bool BenchmarkRunner::readResponse(
    QTcpSocket& socket,
    ProtocolUtils::Response* response,
    QString* errorMessage) {
    QByteArray buffer;

    while (buffer.size() < static_cast<int>(sizeof(noctua::rpc::response_header_t))) {
        if (!socket.waitForReadyRead(5000)) {
            if (errorMessage != nullptr) {
                *errorMessage = QString("Timed out waiting for response header: %1")
                    .arg(socket.errorString());
            }
            return false;
        }
        buffer.append(socket.readAll());
    }

    noctua::rpc::response_header_t header{};
    std::memcpy(&header, buffer.constData(), sizeof(header));
    const auto totalLength = sizeof(header) + static_cast<size_t>(header.message_len);

    while (buffer.size() < static_cast<int>(totalLength)) {
        if (!socket.waitForReadyRead(5000)) {
            if (errorMessage != nullptr) {
                *errorMessage = QString("Timed out waiting for full response: %1")
                    .arg(socket.errorString());
            }
            return false;
        }
        buffer.append(socket.readAll());
    }

    if (response != nullptr) {
        *response = ProtocolUtils::parseResponse(buffer.left(static_cast<int>(totalLength)));
    }
    return true;
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

uint32_t BenchmarkRunner::randomPartitionForWorker() const noexcept {
    return QRandomGenerator::global()->bounded(m_config.partitionCount);
}
