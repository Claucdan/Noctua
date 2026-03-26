#include "src/rpc/rpc-server.h"

#include <boost/asio/signal_set.hpp>
#include <fmt/core.h>

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <thread>
#include <vector>

namespace {

std::atomic<bool> running{true};

void signal_handler(int signal) {
  fmt::println("\n[MAIN] Received signal {}, shutting down...", signal);
  running.store(false);
}

} // namespace

int main(int argc, char* argv[]) {
  uint16_t port = 8080;
  if (argc > 1) {
    port = static_cast<uint16_t>(std::atoi(argv[1]));
  }

  std::signal(SIGINT, signal_handler);
  std::signal(SIGTERM, signal_handler);

  boost::asio::io_context io_ctx;
  noctua::rpc::rpc_server_t server(io_ctx, port);

  server.start();

  std::vector<std::thread> threads;
  const size_t thread_count = std::max<size_t>(1, std::thread::hardware_concurrency());
  threads.reserve(thread_count);

  for (size_t i = 0; i < thread_count; ++i) {
    threads.emplace_back([&io_ctx]() {
      io_ctx.run();
    });
  }

  fmt::println("[MAIN] Server started on port {} with {} threads", port, thread_count);

  while (running.load()) {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }

  server.stop();
  io_ctx.stop();

  for (auto& thread : threads) {
    if (thread.joinable()) {
      thread.join();
    }
  }

  fmt::println("[MAIN] Server stopped");
  return 0;
}
