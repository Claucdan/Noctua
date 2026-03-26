#include "readerworker.h"
#include "protocolutils.h"

ReaderWorker::ReaderWorker(const QString& host, quint16 port,
                           const QString& topicName, uint32_t partitionId,
                           int qps, QObject* parent)
    : Worker(host, port, topicName, partitionId, qps, parent)
{
}

void ReaderWorker::performRequest() {
    QByteArray request = ProtocolUtils::createPullRequest(
        m_topicName.toUtf8(), m_partitionId);
    sendRequest(request);
}

void ReaderWorker::onConnected() {
    Worker::onConnected();
}

void ReaderWorker::onReadyRead() {
    Worker::onReadyRead();
}
