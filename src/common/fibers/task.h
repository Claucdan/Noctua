#pragma once

#include <boost/asio.hpp>

namespace noctua::fibers {

template<typename T>
using task_t = boost::asio::awaitable<T>;

}
