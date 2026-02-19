#pragma once

#include <boost/cobalt.hpp>

namespace common::fibers {

template<typename T>
using task_t = boost::cobalt::task<T>;

template<typename T>
using promise_t = boost::cobalt::promise<T>;

} // namespace common::fibers
