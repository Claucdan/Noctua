#pragma once

#include <string_view>

namespace noctua::common {

#pragma pack(push, 4)

class request_t {
public:
  request_t() = delete;
  request_t(const request_t&) = delete;
  request_t(request_t&&) = delete;

  request_t& operator=(const request_t&) = delete;
  request_t& operator=(request_t&&) = delete;

  ~request_t() = default;

  [[nodiscard]] uint16_t topic_name_len() const noexcept {
    return topic_name_len_;
  }

  [[nodiscard]] uint16_t partition_id() const noexcept {
    return partition_id_;
  }

  [[nodiscard]] std::string_view topic_name() const noexcept {
    return {reinterpret_cast<const char*>(data_), topic_name_len_};
  }

  [[nodiscard]] std::string_view message_view() const noexcept {
    return {reinterpret_cast<const char*>(data_) + topic_name_len_, message_len_};
  }

private:
  uint16_t topic_name_len_;
  uint16_t partition_id_;
  uint16_t message_len_;
  std::byte data_[];
};

#pragma pack(pop)

} // namespace noctua::common
