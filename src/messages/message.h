#pragma once

#include <algorithm>
#include <bit>
#include <cstdint>
#include <memory>
#include <string_view>

namespace noctua::messages {

#pragma pack(push, 4)

class message_t final {
private:
  struct deleter_t {
    void operator()(message_t* data) {
      std::destroy_at(data);
      free(data);
    }
  };

public:
  using message_ptr_t = message_t*;

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

  [[nodiscard]] const message_ptr_t get_next() const noexcept {
    return std::launder(std::bit_cast<message_t*>(next_));
  }

  [[nodiscard]] message_ptr_t get_next() noexcept {
    return std::launder(std::bit_cast<message_t*>(next_));
  }

  void set_next(message_ptr_t next) noexcept {
    next_ = std::bit_cast<uint64_t>(next);
  }

  static message_ptr_t create_message(std::string_view msg) {
    auto raw_data = static_cast<message_t*>(malloc(sizeof(message_t) + msg.size()));
    std::construct_at(raw_data, msg);
    return message_ptr_t{raw_data};
  }

  static void destroy_message(message_t* msg) {
    if (msg) {
      std::destroy_at(msg);
      free(msg);
    }
  }

private:
  uint64_t next_     : 47;
  uint16_t data_len_ : 15;
  char data_[];
};

#pragma pack(pop)

} // namespace noctua::messages
