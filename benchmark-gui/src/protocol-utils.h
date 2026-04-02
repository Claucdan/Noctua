#pragma once

#include <QByteArray>

#include "rpc/rpc-protocol.h"

class protocol_utils_t {
public:
  using opcode_t = noctua::rpc::opcode_t;
  using error_code_t = noctua::rpc::error_code_t;

  static QByteArray create_push_request(const QByteArray& topic_name, uint16_t partition_id, const QByteArray& message);
  static QByteArray create_pull_request(const QByteArray& topic_name, uint16_t partition_id);
  static QByteArray create_delete_request(const QByteArray& topic_name,
                                          uint16_t partition_id,
                                          const QByteArray& message);

  struct response_t {
    bool valid = false;
    opcode_t opcode = opcode_t::ERROR;
    error_code_t error_code = error_code_t::INTERNAL_ERROR;
    QByteArray message;
  };

  static response_t parse_response(const QByteArray& data);
};
