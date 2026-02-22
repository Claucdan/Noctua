#include "tcp_server.hpp"

#include <chrono>
#include <csignal>
#include <cstdlib>

std::unique_ptr<MultiThreadedTcpServer> server;

void signal_handler(int /*signal*/) {
  fmt::print("\nShutting down server...\n");
  server.reset();
  std::abort();
}

int main() {
  const unsigned short port = 12345;

  std::signal(SIGINT, signal_handler);
  std::signal(SIGTERM, signal_handler);

  try {
    server = std::make_unique<MultiThreadedTcpServer>(port);
    server->run();

    fmt::print("Server is running. Press Ctrl+C to stop.\n");

    // Главный поток ожидает завершения
    std::this_thread::sleep_for(std::chrono::hours(24));
  } catch (const std::exception& e) {
    fmt::print(stderr, "Error: {}\n", e.what());
    return 1;
  }

  return 0;
}
