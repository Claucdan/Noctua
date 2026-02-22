#pragma once

#include "src/messages/message.h"

#include <concepts>
#include <string_view>
#include <type_traits>

namespace noctua::messages {

class message_queue_t {
public:
  template<bool IS_CONST>
  class iterator_t {
  public:
    using iterator_category = std::forward_iterator_tag;
    using value_type = message_t;
    using pointer = std::conditional_t<IS_CONST, const value_type*, value_type*>;
    using reference = std::conditional_t<IS_CONST, const value_type&, value_type&>;
    using difference_type = std::ptrdiff_t;

    constexpr iterator_t() = default;
    constexpr iterator_t(const iterator_t& other) = default;
    constexpr iterator_t(iterator_t&& other) noexcept = default;

    constexpr explicit iterator_t(pointer msg)
        : message_(msg) {}

    constexpr iterator_t(const iterator_t<!IS_CONST>& other)
      requires(IS_CONST)
        : message_(other.message_) {}

    constexpr iterator_t& operator=(const iterator_t& other) = default;
    constexpr iterator_t& operator=(iterator_t&& other) noexcept = default;

    ~iterator_t() = default;

    template<bool OTHER_CONST>
    bool operator==(const iterator_t<OTHER_CONST>& other) const noexcept {
      return message_ == other.message_;
    }

    template<bool OTHER_CONST>
    bool operator!=(const iterator_t<OTHER_CONST>& other) const noexcept {
      return message_ != other.message_;
    }

    reference operator*() const noexcept {
      return *message_;
    }

    iterator_t& operator++() noexcept {
      message_ = message_->get_next();
      return *this;
    }

    iterator_t operator++(int) noexcept {
      auto old = *this;
      ++(*this);
      return old;
    }

    pointer operator->() const noexcept {
      return message_;
    }

  private:
    pointer message_{nullptr};

    friend iterator_t<!IS_CONST>;
  };

  using message_ptr_t = message_t::message_ptr_t;
  using iterator = iterator_t<false>;
  using const_iterator = iterator_t<true>;

  message_queue_t() = default;
  message_queue_t(const message_queue_t& other) = delete;
  message_queue_t(message_queue_t&& other) noexcept = default;

  message_queue_t& operator=(const message_queue_t& other) = delete;
  message_queue_t& operator=(message_queue_t&& other) noexcept = default;

  ~message_queue_t() = default;

  [[nodiscard]] const_iterator cbegin() const {
    return const_iterator{head_};
  }

  [[nodiscard]] const_iterator cend() const {
    return const_iterator{nullptr};
  }

  [[nodiscard]] iterator begin() {
    return iterator{head_};
  }

  [[nodiscard]] iterator end() {
    return iterator{nullptr};
  }

  [[nodiscard]] const_iterator front() const noexcept {
    return const_iterator{head_};
  }

  [[nodiscard]] const_iterator back() const noexcept {
    return const_iterator{head_};
  }

  [[nodiscard]] iterator front() noexcept {
    return iterator{head_};
  }

  [[nodiscard]] iterator back() noexcept {
    return iterator{head_};
  }

  iterator push(std::string_view msg) noexcept {
    if (head_ == nullptr) {
      head_ = message_t::create_message(msg);
      tail_ = head_;
    } else {
      auto new_head = message_t::create_message(msg);
      new_head->set_next(head_);
      head_ = new_head;
    }

    ++size_;
    return iterator{head_};
  }

  void pop() noexcept {
    if (head_ == nullptr) {
      return;
    }

    auto old_head = head_;
    head_ = head_->get_next();
    if (head_ == nullptr) {
      tail_ = nullptr;
    }

    --size_;
    message_t::destroy_message(old_head);
  }

  [[nodiscard]] size_t size() const noexcept {
    return size_;
  }

  [[nodiscard]] bool empty() const noexcept {
    return size_ == 0;
  }

private:
  size_t size_{0};
  message_ptr_t head_{nullptr};
  message_ptr_t tail_{nullptr};
};

static_assert(std::forward_iterator<message_queue_t::iterator_t<true>>);
static_assert(std::forward_iterator<message_queue_t::iterator_t<false>>);
static_assert(std::equality_comparable<message_queue_t::iterator_t<true>>);
static_assert(std::equality_comparable<message_queue_t::iterator_t<false>>);
static_assert(std::convertible_to<message_queue_t::iterator_t<false>, message_queue_t::iterator_t<true>>);
static_assert(!std::convertible_to<message_queue_t::iterator_t<true>, message_queue_t::iterator_t<false>>);
static_assert(std::equality_comparable_with<message_queue_t::iterator_t<true>, message_queue_t::iterator_t<false>>);

} // namespace noctua::messages
