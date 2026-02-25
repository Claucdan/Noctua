#pragma once

#include <string_view>

namespace noctua::common {

#pragma pack(push, 4)

class request_body_t {
public:
  request_body_t() = delete;
  request_body_t(const request_body_t&) = delete;
  request_body_t(request_body_t&&) = delete;

  request_body_t& operator=(const request_body_t&) = delete;
  request_body_t& operator=(request_body_t&&) = delete;

private:
  uint16_t topic_name_len;
  uint16_t partition_id;
  uint16_t message_len;
  std::byte data[];
};

#pragma pack(pop)

} // namespace noctua::common
