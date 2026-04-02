#pragma once

#include "worker.h"

class writer_worker_t : public worker_t {
  Q_OBJECT

public:
  explicit writer_worker_t(const QString& host,
                           quint16 port,
                           const QString& topic_name,
                           uint32_t partition_id,
                           int qps,
                           int message_size,
                           QObject* parent = nullptr);
  ~writer_worker_t() override = default;

protected:
  void perform_request() override;

private:
  int message_size_;
  uint64_t sequence_number_ = 0;

  QByteArray create_push_message(const QByteArray& payload);
};
