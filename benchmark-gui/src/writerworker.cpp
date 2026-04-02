#include "writerworker.h"
#include "protocolutils.h"

#include <QDebug>

WriterWorker::WriterWorker(const QString& host, quint16 port,
                           const QString& topicName, uint32_t partitionId,
                           int qps, int messageSize,
                           QObject* parent)
    : Worker(host, port, topicName, partitionId, qps, parent)
    , m_messageSize(messageSize)
{
}

void WriterWorker::performRequest() {
    QByteArray payload;
    payload.resize(m_messageSize);

    // Fill with some data - include sequence number for tracking
    QByteArray seqData = QByteArray::number(m_sequenceNumber++);
    int seqLen = seqData.size();

    // Fill payload with pattern and sequence number
    for (int i = 0; i < m_messageSize; ++i) {
        payload[i] = static_cast<char>('A' + (i % 26));
    }

    // Write sequence number at beginning (if space permits)
    if (seqLen < m_messageSize) {
        memcpy(payload.data(), seqData.constData(), seqLen);
    }

    QByteArray request = createPushMessage(payload);
    qInfo().nospace()
        << "[BENCH][Writer " << this << "] Prepared PUSH payload_size="
        << payload.size() << " request_size=" << request.size()
        << " sequence=" << (m_sequenceNumber - 1);
    sendRequest(request);
}

QByteArray WriterWorker::createPushMessage(const QByteArray& payload) {
    return ProtocolUtils::createPushRequest(m_topicName.toUtf8(), m_partitionId, payload);
}
