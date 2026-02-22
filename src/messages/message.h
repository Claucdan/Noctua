#pragma once

#include <algorithm>
#include <cstdint>
#include <memory>
#include <string_view>

namespace noctua::common {

class message_t final {
private:
  struct deleter_t {
    void operator()(message_t* data) {
      std::destroy_at(data);
      free(data);
    }
  };

public:
  using message_ptr_t = std::unique_ptr<message_t, deleter_t>;

  message_t() = delete;

  message_t(message_t& other) = delete;
  message_t(message_t&& other) = delete;

  explicit message_t(std::string_view msg)
      : data_len_(msg.size()) {
    std::ranges::copy(msg, data_);
  }

  message_t& operator=(message_t& other) = delete;
  message_t& operator=(message_t&& other) = delete;

  ~message_t() = default;

  [[nodiscard]] std::string_view get_data() const noexcept {
    return {data_, data_len_};
  }

  [[nodiscard]] size_t size() const noexcept {
    return data_len_;
  }

  static message_ptr_t create_message(std::string_view msg) {
    auto raw_data = static_cast<message_t*>(malloc(sizeof(message_t) + msg.size()));
    std::construct_at(raw_data, msg);
    return message_ptr_t{raw_data};
  }

private:
  uint32_t data_len_;
  char data_[];
};

} // namespace noctua::common
