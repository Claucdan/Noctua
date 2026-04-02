#include "worker.h"
#include "protocolutils.h"

#include "rpc/rpc-protocol.h"

#include <algorithm>
#include <cmath>
#include <cstring>

Worker::Worker(const QString& host, quint16 port,
               const QString& topicName, uint32_t partitionId,
               int qps, QObject* parent)
    : QObject(parent)
    , m_host(host)
    , m_port(port)
    , m_topicName(topicName)
    , m_partitionId(partitionId)
    , m_qps(qps)
    , m_socket(std::make_unique<QTcpSocket>(this))
    , m_requestTimer(new QTimer(this))
{
    m_socket->setSocketOption(QAbstractSocket::LowDelayOption, 1);

    connect(m_socket.get(), &QTcpSocket::connected, this, &Worker::onConnected);
    connect(m_socket.get(), &QTcpSocket::disconnected, this, &Worker::onDisconnected);
    connect(m_socket.get(), &QTcpSocket::errorOccurred, this, &Worker::onErrorOccurred);
    connect(m_socket.get(), &QTcpSocket::readyRead, this, &Worker::onReadyRead);

    connect(m_requestTimer, &QTimer::timeout, this, &Worker::onRequestTimer);
    m_requestTimer->setInterval(1000 / qps);
}

Worker::~Worker() {
    stop();
}

void Worker::start() {
    if (m_running.load()) {
        return;
    }

    m_running.store(true);
    m_paused.store(false);

    connectToHost();
}

void Worker::stop() {
    if (!m_running.load()) {
        return;
    }

    m_running.store(false);
    m_paused.store(false);
    m_requestTimer->stop();

    if (m_socket->isOpen()) {
        m_socket->disconnectFromHost();
    }

    {
        QMutexLocker locker(&m_pendingMutex);
        m_pendingSendTimes.clear();
    }
    m_buffer.clear();
}

void Worker::pause() {
    if (!m_running.load()) {
        return;
    }

    m_paused.store(true);
    m_requestTimer->stop();
}

void Worker::resume() {
    if (!m_running.load() || !m_paused.load()) {
        return;
    }

    m_paused.store(false);
    m_requestTimer->start();
}

Worker::Stats Worker::getStats() const {
    QMutexLocker locker(&m_statsMutex);
    return m_stats;
}

void Worker::connectToHost() {
    m_socket->connectToHost(m_host, m_port);
}

void Worker::updateStats(uint64_t latencyMicros, bool success) {
    QMutexLocker locker(&m_statsMutex);

    m_stats.totalRequests++;
    m_stats.hasLatencySample = false;

    if (success) {
        m_stats.successfulRequests++;
    } else {
        m_stats.failedRequests++;
    }

    if (latencyMicros > 0) {
        m_stats.latencySampleCount++;
        m_stats.latencyTotalMicros += latencyMicros;
        m_stats.lastLatencyMicros = latencyMicros;
        m_stats.hasLatencySample = true;

        if (m_stats.latencySampleCount == 1) {
            m_stats.minLatencyMs = static_cast<double>(latencyMicros) / 1000.0;
            m_stats.maxLatencyMs = static_cast<double>(latencyMicros) / 1000.0;
        } else {
            m_stats.minLatencyMs =
                std::min(m_stats.minLatencyMs, static_cast<double>(latencyMicros) / 1000.0);
            m_stats.maxLatencyMs =
                std::max(m_stats.maxLatencyMs, static_cast<double>(latencyMicros) / 1000.0);
        }

        m_stats.avgLatencyMs =
            static_cast<double>(m_stats.latencyTotalMicros) /
            static_cast<double>(m_stats.latencySampleCount) /
            1000.0;
        m_stats.p95LatencyMs = m_stats.maxLatencyMs;
        m_stats.p99LatencyMs = m_stats.maxLatencyMs;
    }

    emit statsUpdated(m_stats);
}

void Worker::sendRequest(const QByteArray& data) {
    if (m_socket->state() == QAbstractSocket::ConnectedState) {
        const auto sendTime = currentTimeMicros();
        const auto bytesQueued = m_socket->write(data);
        if (bytesQueued == data.size()) {
            QMutexLocker locker(&m_pendingMutex);
            m_pendingSendTimes.push_back(sendTime);
        }
    }
}

void Worker::processResponse(const QByteArray& data) {
    auto response = ProtocolUtils::parseResponse(data);

    uint64_t sendTime = 0;
    {
        QMutexLocker locker(&m_pendingMutex);
        if (!m_pendingSendTimes.empty()) {
            sendTime = m_pendingSendTimes.front();
            m_pendingSendTimes.pop_front();
        }
    }

    uint64_t latencyMicros = 0;
    if (sendTime != 0) {
        latencyMicros = currentTimeMicros() - sendTime;
    }

    const bool success =
        response.valid &&
        response.errorCode == noctua::rpc::error_code_t::OK &&
        response.opcode != noctua::rpc::opcode_t::ERROR;
    updateStats(latencyMicros, success);
}

void Worker::reconnect() {
    if (m_socket->state() != QAbstractSocket::UnconnectedState) {
        m_socket->disconnectFromHost();
    }
    m_socket->connectToHost(m_host, m_port);
}

void Worker::onConnected() {
    // Start sending requests
    if (m_running.load() && !m_paused.load()) {
        m_requestTimer->start();
    }
}

void Worker::onDisconnected() {
    m_requestTimer->stop();
    recordFailedPendingRequests();
    if (m_running.load()) {
        reconnect();
    }
}

void Worker::onErrorOccurred(QAbstractSocket::SocketError socketError) {
    Q_UNUSED(socketError);
    emit errorOccurred(m_socket->errorString());
    recordFailedPendingRequests();
}

void Worker::onReadyRead() {
    m_buffer.append(m_socket->readAll());

    while (m_buffer.size() >= static_cast<int>(sizeof(noctua::rpc::response_header_t))) {
        noctua::rpc::response_header_t header{};
        std::memcpy(&header, m_buffer.constData(), sizeof(header));
        const size_t totalLen = sizeof(header) + static_cast<size_t>(header.message_len);

        if (m_buffer.size() >= static_cast<int>(totalLen)) {
            QByteArray response = m_buffer.left(static_cast<int>(totalLen));
            m_buffer.remove(0, static_cast<int>(totalLen));
            processResponse(response);
        } else {
            break;
        }
    }
}

void Worker::onRequestTimer() {
    if (m_running.load() && !m_paused.load()) {
        performRequest();
    }
}

void Worker::recordFailedPendingRequests() {
    size_t failedCount = 0;
    {
        QMutexLocker locker(&m_pendingMutex);
        failedCount = m_pendingSendTimes.size();
        m_pendingSendTimes.clear();
    }

    for (size_t i = 0; i < failedCount; ++i) {
        updateStats(0, false);
    }
}

uint64_t Worker::currentTimeMicros() const {
    return std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}
