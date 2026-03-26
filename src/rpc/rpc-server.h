#pragma once

#include "rpc/rpc-protocol.h"
#include "rpc/rpc-session.h"

#include "common/fibers/task.h"
#include "common/kassert.h"

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>

#include <fmt/core.h>

#include <cstdint>
#include <memory>
#include <string_view>

namespace noctua::rpc {

class rpc_server_t {
public:
  rpc_server_t() = delete;
  rpc_server_t(const rpc_server_t&) = delete;
  rpc_server_t(rpc_server_t&&) = delete;

  rpc_server_t& operator=(const rpc_server_t&) = delete;
  rpc_server_t& operator=(rpc_server_t&&) = delete;

  explicit rpc_server_t(boost::asio::io_context& io_ctx, uint16_t port)
      : io_ctx_(io_ctx)
      , acceptor_(io_ctx, boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), port)) {

    fmt::println("[SERVER] RPC server listening on port {}", port);
  }

  ~rpc_server_t() {
    stop();
  }

  void start() {
    boost::asio::co_spawn(
        io_ctx_,
        [this]() -> fibers::task_t<void> {
          co_await accept_loop();
        },
        boost::asio::detached);
  }

  void stop() {
    boost::asio::post(io_ctx_, [this]() {
      boost::system::error_code ec;
      acceptor_.close(ec);
      if (ec) {
        fmt::println(stderr, "[SERVER] Error closing acceptor: {}", ec.message());
      }
    });
  }

  [[nodiscard]] topic_registry_t& registry() noexcept {
    return registry_;
  }

  [[nodiscard]] const topic_registry_t& registry() const noexcept {
    return registry_;
  }

private:
  fibers::task_t<void> accept_loop() {
    while (true) {
      try {
        auto socket = co_await acceptor_.async_accept(boost::asio::use_awaitable);
        auto endpoint = socket.remote_endpoint();
        fmt::println("[SERVER] New connection from {}:{}", endpoint.address().to_string(), endpoint.port());

        auto session = std::make_shared<rpc_session_t>(registry_, std::move(socket));
        boost::asio::co_spawn(
            io_ctx_,
            [session]() -> fibers::task_t<void> {
              co_await session->start();
            },
            boost::asio::detached);
      } catch (const boost::system::system_error& e) {
        if (e.code() != boost::asio::error::operation_aborted) {
          fmt::println(stderr, "[SERVER] Accept error: {}", e.what());
        }
        break;
      } catch (const std::exception& e) {
        fmt::println(stderr, "[SERVER] Accept error: {}", e.what());
        break;
      }
    }
  }

private:
  boost::asio::io_context& io_ctx_;
  boost::asio::ip::tcp::acceptor acceptor_;
  topic_registry_t registry_;
};

} // namespace noctua::rpc
