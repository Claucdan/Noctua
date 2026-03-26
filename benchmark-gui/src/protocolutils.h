#pragma once

#include <QByteArray>
#include <cstdint>

class ProtocolUtils {
public:
    enum class Opcode : uint8_t {
        PUSH = 0x01,
        PULL = 0x02,
        DELETE = 0x03,
        ERROR = 0x04
    };

    enum class ErrorCode : uint8_t {
        INVALID_TOPIC_NAME = 0x01,
        INVALID_PARTITION_ID = 0x02,
        TOPIC_NOT_FOUND = 0x03,
        PARTITION_NOT_FOUND = 0x04,
        EMPTY_PARTITION = 0x05,
        MESSAGE_NOT_FOUND = 0x06,
        INTERNAL_ERROR = 0x07
    };

    static QByteArray createPushRequest(const QByteArray& topicName, uint32_t partitionId, const QByteArray& message);
    static QByteArray createPullRequest(const QByteArray& topicName, uint32_t partitionId);
    static QByteArray createDeleteRequest(const QByteArray& topicName, uint32_t partitionId);

    struct Response {
        Opcode opcode;
        uint8_t errorCode;
        QByteArray message;
    };
    static Response parseResponse(const QByteArray& data);
};
