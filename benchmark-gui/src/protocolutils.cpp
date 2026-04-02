#include "protocolutils.h"

#include <cstddef>
#include <cstring>

namespace {

QByteArray createRequest(noctua::rpc::opcode_t opcode,
                         const QByteArray& topicName,
                         uint16_t partitionId,
                         const QByteArray& message) {
    noctua::rpc::request_header_t header{};
    header.magic = noctua::rpc::RPC_MAGIC;
    header.opcode = opcode;
    header.topic_name_len = static_cast<uint16_t>(topicName.size());
    header.partition_id = partitionId;
    header.message_len = static_cast<uint16_t>(message.size());

    QByteArray packet;
    packet.resize(static_cast<int>(sizeof(header) + topicName.size() + message.size()));

    auto* out = reinterpret_cast<std::byte*>(packet.data());
    std::memcpy(out, &header, sizeof(header));
    std::memcpy(out + sizeof(header), topicName.constData(), static_cast<size_t>(topicName.size()));
    if (!message.isEmpty()) {
        std::memcpy(out + sizeof(header) + topicName.size(), message.constData(), static_cast<size_t>(message.size()));
    }

    return packet;
}

} // namespace

QByteArray ProtocolUtils::createPushRequest(const QByteArray& topicName, uint16_t partitionId, const QByteArray& message) {
    return createRequest(Opcode::PUSH, topicName, partitionId, message);
}

QByteArray ProtocolUtils::createPullRequest(const QByteArray& topicName, uint16_t partitionId) {
    return createRequest(Opcode::PULL, topicName, partitionId, {});
}

QByteArray ProtocolUtils::createDeleteRequest(const QByteArray& topicName, uint16_t partitionId, const QByteArray& message) {
    return createRequest(Opcode::DELETE, topicName, partitionId, message);
}

ProtocolUtils::Response ProtocolUtils::parseResponse(const QByteArray& data) {
    Response response;

    if (data.size() < static_cast<int>(sizeof(noctua::rpc::response_header_t))) {
        return response;
    }

    noctua::rpc::response_header_t header{};
    std::memcpy(&header, data.constData(), sizeof(header));

    if (header.magic != noctua::rpc::RPC_MAGIC) {
        return response;
    }

    const auto totalSize = sizeof(header) + static_cast<size_t>(header.message_len);
    if (data.size() < static_cast<int>(totalSize)) {
        return response;
    }

    response.valid = true;
    response.opcode = header.opcode;
    response.errorCode = header.error_code;
    if (header.message_len > 0) {
        response.message = data.mid(static_cast<int>(sizeof(header)), header.message_len);
    }

    return response;
}
