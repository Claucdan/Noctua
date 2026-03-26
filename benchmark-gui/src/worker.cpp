#include "worker.h"
#include "protocolutils.h"
#include <QCoreApplication>
#include <algorithm>
#include <cmath>

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

    if (success) {
        m_stats.successfulRequests++;
    } else {
        m_stats.failedRequests++;
    }

    // Store latency (keep only last 1000 samples)
    m_stats.latencyMicros.push_back(latencyMicros);
    if (m_stats.latencyMicros.size() > 1000) {
        m_stats.latencyMicros.pop_front();
    }

    // Calculate statistics
    if (!m_stats.latencyMicros.empty()) {
        uint64_t sum = 0;
        uint64_t minLatency = std::numeric_limits<uint64_t>::max();
        uint64_t maxLatency = 0;

        for (uint64_t lat : m_stats.latencyMicros) {
            sum += lat;
            if (lat < minLatency) minLatency = lat;
            if (lat > maxLatency) maxLatency = lat;
        }

        m_stats.avgLatencyMs = static_cast<double>(sum) / m_stats.latencyMicros.size() / 1000.0;
        m_stats.minLatencyMs = static_cast<double>(minLatency) / 1000.0;
        m_stats.maxLatencyMs = static_cast<double>(maxLatency) / 1000.0;

        // Calculate percentiles
        std::vector<uint64_t> sorted(m_stats.latencyMicros.begin(), m_stats.latencyMicros.end());
        std::sort(sorted.begin(), sorted.end());

        size_t p95Index = static_cast<size_t>(sorted.size() * 0.95);
        size_t p99Index = static_cast<size_t>(sorted.size() * 0.99);

        if (p95Index < sorted.size()) {
            m_stats.p95LatencyMs = static_cast<double>(sorted[p95Index]) / 1000.0;
        }
        if (p99Index < sorted.size()) {
            m_stats.p99LatencyMs = static_cast<double>(sorted[p99Index]) / 1000.0;
        }
    }

    emit statsUpdated(m_stats);
}

void Worker::sendRequest(const QByteArray& data) {
    if (m_socket->state() == QAbstractSocket::ConnectedState) {
        m_lastSendTime.store(std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count());
        m_socket->write(data);
    }
}

void Worker::processResponse(const QByteArray& data) {
    auto response = ProtocolUtils::parseResponse(data);

    uint64_t currentTime = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    uint64_t latencyMicros = currentTime - m_lastSendTime.load();

    bool success = (response.errorCode == 0 && response.opcode != ProtocolUtils::Opcode::ERROR);
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
    if (m_running.load()) {
        reconnect();
    }
}

void Worker::onErrorOccurred(QAbstractSocket::SocketError socketError) {
    Q_UNUSED(socketError);
    emit errorOccurred(m_socket->errorString());
    if (m_running.load()) {
        updateStats(0, false);  // Count as failed request
    }
}

void Worker::onReadyRead() {
    m_buffer.append(m_socket->readAll());

    // Process complete responses
    while (m_buffer.size() >= 14) {
        // Read message length (bytes 6-13)
        QDataStream stream(m_buffer);
        stream.setByteOrder(QDataStream::LittleEndian);
        stream.skipRawData(6);  // Skip magic and opcode

        uint64_t messageLen;
        stream >> messageLen;

        size_t totalLen = 14 + static_cast<size_t>(messageLen);

        if (m_buffer.size() >= static_cast<int>(totalLen)) {
            QByteArray response = m_buffer.left(static_cast<int>(totalLen));
            m_buffer.remove(0, static_cast<int>(totalLen));
            processResponse(response);
        } else {
            // Wait for more data
            break;
        }
    }
}

void Worker::onRequestTimer() {
    if (m_running.load() && !m_paused.load()) {
        performRequest();
    }
}
