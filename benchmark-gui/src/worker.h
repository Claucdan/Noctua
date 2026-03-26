#pragma once

#include <QObject>
#include <QThread>
#include <QTcpSocket>
#include <QTimer>
#include <QByteArray>
#include <QMutex>
#include <atomic>
#include <memory>
#include <deque>
#include <vector>
#include <algorithm>
#include <chrono>
#include <limits>

class Worker : public QObject {
    Q_OBJECT

public:
    struct Stats {
        uint64_t totalRequests = 0;
        uint64_t successfulRequests = 0;
        uint64_t failedRequests = 0;
        std::deque<uint64_t> latencyMicros;  // Last 1000 latencies for statistics
        double avgLatencyMs = 0.0;
        double minLatencyMs = 0.0;
        double maxLatencyMs = 0.0;
        double p95LatencyMs = 0.0;
        double p99LatencyMs = 0.0;
    };

    explicit Worker(const QString& host, quint16 port,
                    const QString& topicName, uint32_t partitionId,
                    int qps, QObject* parent = nullptr);
    virtual ~Worker();

    void start();
    void stop();
    void pause();
    void resume();

    Stats getStats() const;

signals:
    void statsUpdated(const Worker::Stats& stats);
    void errorOccurred(const QString& error);

protected:
    virtual void performRequest() = 0;
    void updateStats(uint64_t latencyMicros, bool success);
    void sendRequest(const QByteArray& data);
    void processResponse(const QByteArray& data);
    void reconnect();
    void connectToHost();

protected slots:
    virtual void onConnected();
    virtual void onDisconnected();
    virtual void onErrorOccurred(QAbstractSocket::SocketError socketError);
    virtual void onReadyRead();

    void onRequestTimer();

protected:
    QString m_host;
    quint16 m_port;
    QString m_topicName;
    uint32_t m_partitionId;
    int m_qps;
    std::atomic<bool> m_running{false};
    std::atomic<bool> m_paused{false};

    std::unique_ptr<QTcpSocket> m_socket;
    QTimer* m_requestTimer;

    mutable QMutex m_statsMutex;
    Stats m_stats;
    std::atomic<uint64_t> m_lastSendTime{0};
    QByteArray m_buffer;  // Buffer for incoming data
};
