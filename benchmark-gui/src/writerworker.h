#pragma once

#include "worker.h"

class WriterWorker : public Worker {
    Q_OBJECT

public:
    explicit WriterWorker(const QString& host, quint16 port,
                         const QString& topicName, uint32_t partitionId,
                         int qps, int messageSize,
                         QObject* parent = nullptr);
    ~WriterWorker() override = default;

protected:
    void performRequest() override;

private:
    int m_messageSize;
    uint64_t m_sequenceNumber = 0;

    QByteArray createPushMessage(const QByteArray& payload);
};
