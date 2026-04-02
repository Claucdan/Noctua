#pragma once

#include "worker.h"

class reader_worker_t : public worker_t {
  Q_OBJECT

public:
  explicit reader_worker_t(const QString& host,
                           quint16 port,
                           const QString& topic_name,
                           uint32_t partition_id,
                           int qps,
                           QObject* parent = nullptr);
  ~reader_worker_t() override = default;

protected:
  void perform_request() override;
  void on_connected() override;
  void on_ready_read() override;
};
