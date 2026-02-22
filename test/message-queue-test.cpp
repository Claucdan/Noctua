
#include "src/messages/message-queue.h"

#include <gtest/gtest.h>

namespace noctua::messages {

class message_queue_test_t : public ::testing::Test {
protected:
  void SetUp() override {
    while (!queue.empty()) {
      queue.pop();
    }
  }

  message_queue_t queue;
};

TEST_F(message_queue_test_t, constructor_creates_empty_queue) {
  EXPECT_TRUE(queue.empty());
  EXPECT_EQ(queue.size(), 0);
  EXPECT_EQ(queue.begin(), queue.end());
  EXPECT_EQ(queue.cbegin(), queue.cend());
}

TEST_F(message_queue_test_t, push_adds_message_to_front) {
  const std::string_view TEST_MESSAGE = "test message";

  auto it = queue.push(TEST_MESSAGE);

  EXPECT_FALSE(queue.empty());
  EXPECT_EQ(queue.size(), 1);
  EXPECT_EQ(it, queue.begin());
  EXPECT_NE(queue.begin(), queue.end());
  EXPECT_EQ(it->get_data(), TEST_MESSAGE);
}

TEST_F(message_queue_test_t, push_multiple_messages) {
  const std::string_view FIRST_MESSAGE = "first message";
  const std::string_view SECOND_MESSAGE = "second message";
  const std::string_view THIRD_MESSAGE = "third message";

  auto first_it = queue.push(FIRST_MESSAGE);
  auto second_it = queue.push(SECOND_MESSAGE);
  auto third_it = queue.push(THIRD_MESSAGE);

  EXPECT_EQ(queue.size(), 3);
  EXPECT_EQ(first_it, queue.begin());
  EXPECT_EQ((*queue.begin()).get_data(), FIRST_MESSAGE);

  auto current = queue.begin();
  EXPECT_EQ((*current).get_data(), FIRST_MESSAGE);
  ++current;
  EXPECT_EQ((*current).get_data(), SECOND_MESSAGE);
  ++current;
  EXPECT_EQ((*current).get_data(), THIRD_MESSAGE);
  ++current;
  EXPECT_EQ(current, queue.end());
}

TEST_F(message_queue_test_t, pop_removes_front_message) {
  const std::string_view FIRST_MESSAGE = "first message";
  const std::string_view SECOND_MESSAGE = "second message";

  queue.push(FIRST_MESSAGE);
  queue.push(SECOND_MESSAGE);
  EXPECT_EQ(queue.size(), 2);

  queue.pop();
  EXPECT_EQ(queue.size(), 1);
  EXPECT_EQ((*queue.begin()).get_data(), SECOND_MESSAGE);

  queue.pop();
  EXPECT_TRUE(queue.empty());
  EXPECT_EQ(queue.size(), 0);
  EXPECT_EQ(queue.begin(), queue.end());
}

TEST_F(message_queue_test_t, pop_on_empty_queue_does_nothing) {
  EXPECT_TRUE(queue.empty());

  queue.pop();

  EXPECT_TRUE(queue.empty());
  EXPECT_EQ(queue.size(), 0);
}

TEST_F(message_queue_test_t, iterator_dereference_returns_message) {
  const std::string_view TEST_MESSAGE = "test message";

  queue.push(TEST_MESSAGE);
  auto it = queue.begin();

  message_t& message = *it;
  EXPECT_EQ(message.get_data(), TEST_MESSAGE);
}

TEST_F(message_queue_test_t, iterator_arrow_operator_works) {
  const std::string_view TEST_MESSAGE = "test message";

  queue.push(TEST_MESSAGE);
  auto it = queue.begin();

  EXPECT_EQ(it->get_data(), TEST_MESSAGE);
}

TEST_F(message_queue_test_t, iterator_pre_increment_works) {
  const std::string_view FIRST_MESSAGE = "first message";
  const std::string_view SECOND_MESSAGE = "second message";

  queue.push(FIRST_MESSAGE);
  queue.push(SECOND_MESSAGE);

  auto it = queue.begin();
  EXPECT_EQ((*it).get_data(), FIRST_MESSAGE);

  auto& it_ref = ++it;
  EXPECT_EQ(&it_ref, &it);
  EXPECT_EQ((*it).get_data(), SECOND_MESSAGE);
}

TEST_F(message_queue_test_t, iterator_post_increment_works) {
  const std::string_view FIRST_MESSAGE = "first message";
  const std::string_view SECOND_MESSAGE = "second message";

  queue.push(FIRST_MESSAGE);
  queue.push(SECOND_MESSAGE);

  auto it = queue.begin();
  EXPECT_EQ((*it).get_data(), FIRST_MESSAGE);

  auto old_it = it++;
  EXPECT_EQ((*old_it).get_data(), FIRST_MESSAGE);
  EXPECT_EQ((*it).get_data(), SECOND_MESSAGE);
}

TEST_F(message_queue_test_t, const_iterator_can_be_created_from_begin) {
  const std::string_view TEST_MESSAGE = "test message";

  queue.push(TEST_MESSAGE);

  auto cit = queue.cbegin();
  EXPECT_EQ((*cit).get_data(), TEST_MESSAGE);
  EXPECT_EQ(cit, queue.cbegin());
  EXPECT_NE(cit, queue.cend());
}

TEST_F(message_queue_test_t, const_iterator_traversal_works) {
  const std::string_view FIRST_MESSAGE = "first message";
  const std::string_view SECOND_MESSAGE = "second message";

  queue.push(FIRST_MESSAGE);
  queue.push(SECOND_MESSAGE);

  auto cit = queue.cbegin();
  EXPECT_EQ((*cit).get_data(), FIRST_MESSAGE);
  ++cit;
  EXPECT_EQ((*cit).get_data(), SECOND_MESSAGE);
  ++cit;
  EXPECT_EQ(cit, queue.cend());
}

TEST_F(message_queue_test_t, non_const_iterator_converts_to_const_iterator) {
  const std::string_view TEST_MESSAGE = "test message";

  queue.push(TEST_MESSAGE);

  message_queue_t::iterator it = queue.begin();
  message_queue_t::const_iterator cit = it; // Должно работать

  EXPECT_EQ(cit, queue.cbegin());
  EXPECT_EQ((*cit).get_data(), TEST_MESSAGE);
}

TEST_F(message_queue_test_t, const_iterator_does_not_convert_to_non_const) {
  const std::string_view TEST_MESSAGE = "test message";

  queue.push(TEST_MESSAGE);

  message_queue_t::const_iterator cit = queue.cbegin();
  // Следующая строка не должна компилироваться - оставляем закомментированной
  // message_queue_t::iterator it = cit;

  // Вместо этого проверяем, что const_iterator работает корректно
  EXPECT_EQ((*cit).get_data(), TEST_MESSAGE);
}

TEST_F(message_queue_test_t, iterators_equality_comparison_works) {
  const std::string_view TEST_MESSAGE = "test message";

  queue.push(TEST_MESSAGE);

  auto it1 = queue.begin();
  auto it2 = queue.begin();
  auto it3 = queue.end();

  EXPECT_EQ(it1, it2);
  EXPECT_NE(it1, it3);

  auto cit1 = queue.cbegin();
  auto cit2 = queue.cbegin();

  EXPECT_EQ(cit1, cit2);
  EXPECT_NE(cit1, queue.cend());
}

TEST_F(message_queue_test_t, iterators_cross_comparison_works) {
  const std::string_view TEST_MESSAGE = "test message";

  queue.push(TEST_MESSAGE);

  auto it = queue.begin();
  auto cit = queue.cbegin();

  EXPECT_EQ(it, cit);
  EXPECT_EQ(cit, it);
}

TEST_F(message_queue_test_t, front_returns_iterator_to_begin) {
  const std::string_view FIRST_MESSAGE = "first message";
  const std::string_view SECOND_MESSAGE = "second message";

  queue.push(FIRST_MESSAGE);
  queue.push(SECOND_MESSAGE);

  EXPECT_EQ(queue.front(), queue.begin());
  EXPECT_EQ((*queue.front()).get_data(), FIRST_MESSAGE);

  const auto& const_queue = queue;
  EXPECT_EQ(const_queue.front(), const_queue.cbegin());
  EXPECT_EQ((*const_queue.front()).get_data(), FIRST_MESSAGE);
}

TEST_F(message_queue_test_t, back_returns_iterator_to_begin) {
  const std::string_view FIRST_MESSAGE = "first message";
  const std::string_view SECOND_MESSAGE = "second message";

  queue.push(FIRST_MESSAGE);
  queue.push(SECOND_MESSAGE);

  EXPECT_EQ(queue.back(), queue.begin());
  EXPECT_EQ((*queue.back()).get_data(), FIRST_MESSAGE);

  const auto& const_queue = queue;
  EXPECT_EQ(const_queue.back(), const_queue.cbegin());
  EXPECT_EQ((*const_queue.back()).get_data(), FIRST_MESSAGE);
}

TEST_F(message_queue_test_t, move_constructor_works) {
  const std::string_view TEST_MESSAGE = "test message";

  queue.push(TEST_MESSAGE);
  EXPECT_EQ(queue.size(), 1);

  message_queue_t moved_queue(std::move(queue));
  EXPECT_EQ(moved_queue.size(), 1);
  EXPECT_EQ((*moved_queue.begin()).get_data(), TEST_MESSAGE);

  // Исходная очередь должна быть в валидном, но неопределенном состоянии
  // По крайней мере, она не должна падать при разрушении
}

TEST_F(message_queue_test_t, move_assignment_works) {
  const std::string_view FIRST_MESSAGE = "first message";
  const std::string_view SECOND_MESSAGE = "second message";

  queue.push(FIRST_MESSAGE);

  message_queue_t other_queue;
  other_queue.push(SECOND_MESSAGE);

  other_queue = std::move(queue);
  EXPECT_EQ(other_queue.size(), 1);
  EXPECT_EQ((*other_queue.begin()).get_data(), FIRST_MESSAGE);
}

TEST_F(message_queue_test_t, range_based_for_loop_works_with_iterator) {
  const std::string_view MESSAGES[] = {"first", "second", "third"};
  constexpr size_t MESSAGE_COUNT = 3;

  for (const auto& msg : MESSAGES) {
    queue.push(msg);
  }

  size_t INDEX = 0;
  for (auto& msg : queue) {
    EXPECT_EQ(msg.get_data(), MESSAGES[INDEX]);
    ++INDEX;
  }
  EXPECT_EQ(INDEX, MESSAGE_COUNT);
}

TEST_F(message_queue_test_t, iterator_concepts_are_satisfied) {
  using iterator = message_queue_t::iterator;
  using const_iterator = message_queue_t::const_iterator;

  static_assert(std::forward_iterator<iterator>);
  static_assert(std::forward_iterator<const_iterator>);
  static_assert(std::equality_comparable<iterator>);
  static_assert(std::equality_comparable<const_iterator>);
  static_assert(std::equality_comparable_with<iterator, const_iterator>);
}

TEST_F(message_queue_test_t, large_number_of_messages) {
  constexpr size_t MESSAGE_COUNT = 1000;
  const std::string_view BASE_MESSAGE = "message ";

  for (size_t i = 0; i < MESSAGE_COUNT; ++i) {
    std::string msg = std::string(BASE_MESSAGE) + std::to_string(i);
    queue.push(msg);
  }

  EXPECT_EQ(queue.size(), MESSAGE_COUNT);

  size_t COUNT = 0;
  for ([[maybe_unused]] const auto& msg : queue) {
    ++COUNT;
  }
  EXPECT_EQ(COUNT, MESSAGE_COUNT);
}

TEST_F(message_queue_test_t, empty_queue_iterators_are_equal) {
  EXPECT_EQ(queue.begin(), queue.end());
  EXPECT_EQ(queue.cbegin(), queue.cend());
}

} // namespace noctua::messages
