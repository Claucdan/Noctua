#pragma once

#include <cstdint>
#include <string_view>
#include <span>

namespace noctua::rpc {

inline constexpr uint32_t RPC_MAGIC = 0x4E4F4355; // "NOCU"

enum class opcode_t : uint8_t {
  PUSH = 0x01,
  PULL = 0x02,
  DELETE = 0x03,
  ERROR = 0xFF,
};

enum class error_code_t : uint16_t {
  OK = 0x0000,
  INVALID_TOPIC_NAME = 0x0001,
  INVALID_PARTITION_ID = 0x0002,
  TOPIC_NOT_FOUND = 0x0003,
  PARTITION_NOT_FOUND = 0x0004,
  EMPTY_PARTITION = 0x0005,
  MESSAGE_NOT_FOUND = 0x0006,
  INTERNAL_ERROR = 0xFFFF,
};

#pragma pack(push, 4)

struct request_header_t {
  uint32_t magic{RPC_MAGIC};
  opcode_t opcode{};
  uint16_t topic_name_len{};
  uint16_t partition_id{};
  uint16_t message_len{};
};

struct response_header_t {
  uint32_t magic{RPC_MAGIC};
  opcode_t opcode{};
  error_code_t error_code{};
  uint16_t message_len{};
};

#pragma pack(pop)

[[nodiscard]] inline constexpr std::string_view opcode_to_string(opcode_t opcode) noexcept {
  switch (opcode) {
    case opcode_t::PUSH:
      return "PUSH";
    case opcode_t::PULL:
      return "PULL";
    case opcode_t::DELETE:
      return "DELETE";
    case opcode_t::ERROR:
      return "ERROR";
    default:
      return "UNKNOWN";
  }
}

[[nodiscard]] inline constexpr std::string_view error_code_to_string(error_code_t code) noexcept {
  switch (code) {
    case error_code_t::OK:
      return "OK";
    case error_code_t::INVALID_TOPIC_NAME:
      return "INVALID_TOPIC_NAME";
    case error_code_t::INVALID_PARTITION_ID:
      return "INVALID_PARTITION_ID";
    case error_code_t::TOPIC_NOT_FOUND:
      return "TOPIC_NOT_FOUND";
    case error_code_t::PARTITION_NOT_FOUND:
      return "PARTITION_NOT_FOUND";
    case error_code_t::EMPTY_PARTITION:
      return "EMPTY_PARTITION";
    case error_code_t::MESSAGE_NOT_FOUND:
      return "MESSAGE_NOT_FOUND";
    case error_code_t::INTERNAL_ERROR:
      return "INTERNAL_ERROR";
    default:
      return "UNKNOWN";
  }
}

} // namespace noctua::rpc
