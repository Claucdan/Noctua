#include "writer-worker.h"
#include "protocol-utils.h"

#include <cstring>

WriterWorker::WriterWorker(const QString& host,
                           quint16 port,
                           const QString& topicName,
                           uint32_t partitionId,
                           int qps,
                           int messageSize,
                           QObject* parent)
    : Worker(host, port, topicName, partitionId, qps, parent), m_messageSize(messageSize) {}

void WriterWorker::performRequest() {
  QByteArray payload;
  payload.resize(m_messageSize);

  QByteArray seqData = QByteArray::number(m_sequenceNumber++);
  int seqLen = seqData.size();

  for (int i = 0; i < m_messageSize; ++i) {
    payload[i] = static_cast<char>('A' + (i % 26));
  }

  if (seqLen < m_messageSize) {
    memcpy(payload.data(), seqData.constData(), seqLen);
  }

  QByteArray request = createPushMessage(payload);
  sendRequest(request);
}

QByteArray WriterWorker::createPushMessage(const QByteArray& payload) {
  return ProtocolUtils::createPushRequest(m_topicName.toUtf8(), m_partitionId, payload);
}
