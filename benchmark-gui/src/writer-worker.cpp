#include "writer-worker.h"
#include "protocol-utils.h"

#include <cstring>

writer_worker_t::writer_worker_t(const QString& host,
                                 quint16 port,
                                 const QString& topic_name,
                                 uint32_t partition_id,
                                 int qps,
                                 int message_size,
                                 QObject* parent)
    : worker_t(host, port, topic_name, partition_id, qps, parent), message_size_(message_size) {}

void writer_worker_t::perform_request() {
  QByteArray payload;
  payload.resize(message_size_);

  QByteArray seq_data = QByteArray::number(sequence_number_++);
  int seq_len = seq_data.size();

  for (int i = 0; i < message_size_; ++i) {
    payload[i] = static_cast<char>('A' + (i % 26));
  }

  if (seq_len < message_size_) {
    memcpy(payload.data(), seq_data.constData(), seq_len);
  }

  QByteArray request = create_push_message(payload);
  send_request(request);
}

QByteArray writer_worker_t::create_push_message(const QByteArray& payload) {
  return protocol_utils_t::create_push_request(topic_name_.toUtf8(), static_cast<uint16_t>(partition_id_), payload);
}
