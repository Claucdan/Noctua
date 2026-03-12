#include "src/rpc/rpc-client.h"

#include <fmt/core.h>

#include <cstdlib>
#include <iostream>

using namespace noctua::rpc;

void print_help() {
  fmt::println("Usage: ./noctua-client <command> [options]");
  fmt::println("");
  fmt::println("Commands:");
  fmt::println("  push <topic> <partition> <message>    - Push message to topic");
  fmt::println("  pull <topic> <partition>               - Pull message from topic partition");
  fmt::println("  delete <topic> <partition> <message>   - Delete message from topic");
  fmt::println("");
  fmt::println("Options:");
  fmt::println("  --host <host>   - Server host (default: 127.0.0.1)");
  fmt::println("  --port <port>   - Server port (default: 8080)");
  fmt::println("");
  fmt::println("Examples:");
  fmt::println("  ./noctua-client push my-topic 0 \"Hello World\"");
  fmt::println("  ./noctua-client pull my-topic 0");
  fmt::println("  ./noctua-client --port 9090 push test 1 \"data\"");
}

int main(int argc, char* argv[]) {
  if (argc < 3) {
    print_help();
    return 1;
  }

  std::string host = "127.0.0.1";
  uint16_t port = 8080;

  std::string command = argv[1];

  for (int i = 2; i < argc; ++i) {
    std::string arg = argv[i];

    if (arg == "--host" && i + 1 < argc) {
      host = argv[++i];
    } else if (arg == "--port" && i + 1 < argc) {
      port = static_cast<uint16_t>(std::atoi(argv[++i]));
    }
  }

  rpc_client_t client;

  try {
    fmt::println("[CLIENT] Connecting to {}:{}...", host, port);
    client.connect(host, port);
    fmt::println("[CLIENT] Connected!");

    if (command == "push") {
      if (argc < 6) {
        fmt::println(stderr, "Error: push requires <topic> <partition> <message>");
        print_help();
        return 1;
      }

      std::string topic = argv[2];
      uint16_t partition = static_cast<uint16_t>(std::atoi(argv[3]));
      std::string message = argv[4];

      fmt::println("[CLIENT] Sending PUSH: topic={}, partition={}, message={}", topic, partition, message);
      client.send_push(topic, partition, message);

    } else if (command == "pull") {
      if (argc < 5) {
        fmt::println(stderr, "Error: pull requires <topic> <partition>");
        print_help();
        return 1;
      }

      std::string topic = argv[2];
      uint16_t partition = static_cast<uint16_t>(std::atoi(argv[3]));

      fmt::println("[CLIENT] Sending PULL: topic={}, partition={}", topic, partition);
      client.send_pull(topic, partition);

    } else if (command == "delete") {
      if (argc < 6) {
        fmt::println(stderr, "Error: delete requires <topic> <partition> <message>");
        print_help();
        return 1;
      }

      std::string topic = argv[2];
      uint16_t partition = static_cast<uint16_t>(std::atoi(argv[3]));
      std::string message = argv[4];

      fmt::println("[CLIENT] Sending DELETE: topic={}, partition={}, message={}", topic, partition, message);
      client.send_delete(topic, partition, message);

    } else {
      fmt::println(stderr, "Error: Unknown command '{}'", command);
      print_help();
      return 1;
    }

    auto response = client.receive_response();
    fmt::println("[CLIENT] Response: opcode={}, error_code={}, message_len={}",
                  opcode_to_string(response.opcode),
                  error_code_to_string(response.error_code),
                  response.message_len);

    if (response.error_code != error_code_t::OK) {
      auto error_msg = client.receive_response_data(response);
      if (error_msg) {
        fmt::println("[CLIENT] Error: {}", *error_msg);
      }
    } else if (response.message_len > 0) {
      auto data = client.receive_response_data(response);
      if (data) {
        fmt::println("[CLIENT] Data: {}", *data);
      }
    }

    client.disconnect();
    fmt::println("[CLIENT] Disconnected");

  } catch (const std::exception& e) {
    fmt::println(stderr, "[CLIENT] Error: {}", e.what());
    return 1;
  }

  return 0;
}
