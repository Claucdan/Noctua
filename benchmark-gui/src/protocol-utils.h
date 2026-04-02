#pragma once

#include <QByteArray>

#include "rpc/rpc-protocol.h"

class ProtocolUtils {
public:
  using Opcode = noctua::rpc::opcode_t;
  using ErrorCode = noctua::rpc::error_code_t;

  static QByteArray createPushRequest(const QByteArray& topicName, uint16_t partitionId, const QByteArray& message);
  static QByteArray createPullRequest(const QByteArray& topicName, uint16_t partitionId);
  static QByteArray createDeleteRequest(const QByteArray& topicName, uint16_t partitionId, const QByteArray& message);

  struct Response {
    bool valid = false;
    Opcode opcode = Opcode::ERROR;
    ErrorCode errorCode = ErrorCode::INTERNAL_ERROR;
    QByteArray message;
  };

  static Response parseResponse(const QByteArray& data);
};
