#include "reader-worker.h"
#include "protocol-utils.h"

reader_worker_t::reader_worker_t(
        const QString& host, quint16 port, const QString& topic_name, uint32_t partition_id, int qps, QObject* parent)
    : worker_t(host, port, topic_name, partition_id, qps, parent) {}

void reader_worker_t::perform_request() {
  QByteArray request =
          protocol_utils_t::create_pull_request(topic_name_.toUtf8(), static_cast<uint16_t>(partition_id_));
  send_request(request);
}

void reader_worker_t::on_connected() {
  worker_t::on_connected();
}

void reader_worker_t::on_ready_read() {
  worker_t::on_ready_read();
}
