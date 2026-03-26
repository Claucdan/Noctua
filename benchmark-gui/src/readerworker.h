#pragma once

#include "worker.h"

class ReaderWorker : public Worker {
    Q_OBJECT

public:
    explicit ReaderWorker(const QString& host, quint16 port,
                         const QString& topicName, uint32_t partitionId,
                         int qps, QObject* parent = nullptr);
    ~ReaderWorker() override = default;

protected:
    void performRequest() override;
    void onConnected() override;
    void onReadyRead() override;
};
