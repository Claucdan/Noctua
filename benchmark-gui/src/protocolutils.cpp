#include "protocolutils.h"
#include <QDataStream>
#include <QIODevice>

QByteArray ProtocolUtils::createPushRequest(const QByteArray& topicName, uint32_t partitionId, const QByteArray& message) {
    QByteArray packet;
    QDataStream stream(&packet, QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::LittleEndian);

    // Magic number: 0x4E4F4355 ("NOCU")
    stream << static_cast<uint32_t>(0x4E4F4355);
    // Opcode: PUSH
    stream << static_cast<uint8_t>(Opcode::PUSH);
    // Topic name length
    stream << static_cast<uint32_t>(topicName.size());
    // Partition ID
    stream << partitionId;
    // Message length
    stream << static_cast<uint64_t>(message.size());

    // Topic name
    stream.writeRawData(topicName.constData(), topicName.size());

    // Message
    if (!message.isEmpty()) {
        stream.writeRawData(message.constData(), message.size());
    }

    return packet;
}

QByteArray ProtocolUtils::createPullRequest(const QByteArray& topicName, uint32_t partitionId) {
    QByteArray packet;
    QDataStream stream(&packet, QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::LittleEndian);

    // Magic number: 0x4E4F4355 ("NOCU")
    stream << static_cast<uint32_t>(0x4E4F4355);
    // Opcode: PULL
    stream << static_cast<uint8_t>(Opcode::PULL);
    // Topic name length
    stream << static_cast<uint32_t>(topicName.size());
    // Partition ID
    stream << partitionId;
    // Message length (0 for pull)
    stream << static_cast<uint64_t>(0);

    // Topic name
    stream.writeRawData(topicName.constData(), topicName.size());

    return packet;
}

QByteArray ProtocolUtils::createDeleteRequest(const QByteArray& topicName, uint32_t partitionId) {
    QByteArray packet;
    QDataStream stream(&packet, QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::LittleEndian);

    // Magic number: 0x4E4F4355 ("NOCU")
    stream << static_cast<uint32_t>(0x4E4F4355);
    // Opcode: DELETE
    stream << static_cast<uint8_t>(Opcode::DELETE);
    // Topic name length
    stream << static_cast<uint32_t>(topicName.size());
    // Partition ID
    stream << partitionId;
    // Message length (0 for delete)
    stream << static_cast<uint64_t>(0);

    // Topic name
    stream.writeRawData(topicName.constData(), topicName.size());

    return packet;
}

ProtocolUtils::Response ProtocolUtils::parseResponse(const QByteArray& data) {
    Response response;
    response.opcode = Opcode::ERROR;
    response.errorCode = 0;

    if (data.size() < 14) {
        return response;  // Invalid response
    }

    QDataStream stream(data);
    stream.setByteOrder(QDataStream::LittleEndian);

    uint32_t magic;
    uint8_t opcode;
    uint8_t errorCode;
    uint64_t messageLen;

    stream >> magic;

    if (magic != 0x4E4F4355) {
        return response;  // Invalid magic number
    }

    stream >> opcode;
    stream >> errorCode;
    stream >> messageLen;

    response.opcode = static_cast<Opcode>(opcode);
    response.errorCode = errorCode;

    if (messageLen > 0 && stream.device()->bytesAvailable() >= static_cast<qint64>(messageLen)) {
        response.message.resize(static_cast<int>(messageLen));
        stream.readRawData(response.message.data(), static_cast<int>(messageLen));
    }

    return response;
}
