#include "benchmark-runner.h"

#include "benchmark-gui-qt-test.h"
#include "protocol-utils.h"

#include "rpc/rpc-protocol.h"
#include "rpc/rpc-server.h"

#include <boost/asio.hpp>

#include <QCoreApplication>
#include <QTest>

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <thread>

namespace {

using namespace std::chrono_literals;

uint16_t find_free_port() {
  boost::asio::io_context io_ctx;
  boost::asio::ip::tcp::acceptor acceptor(io_ctx, boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), 0));
  return acceptor.local_endpoint().port();
}

class test_rpc_server_t {
public:
  explicit test_rpc_server_t(uint16_t port)
      : server_(io_ctx_, port) {
    server_.start();
    thread_ = std::thread([this]() { io_ctx_.run(); });
    std::this_thread::sleep_for(50ms);
  }

  ~test_rpc_server_t() {
    server_.stop();
    io_ctx_.stop();
    if (thread_.joinable()) {
      thread_.join();
    }
  }

private:
  boost::asio::io_context io_ctx_;
  noctua::rpc::rpc_server_t server_;
  std::thread thread_;
};

} // namespace

void benchmark_gui_qt_test_t::create_push_request_matches_rpc_wire_format() {
  const QByteArray topic{"bench-topic"};
  const QByteArray message{"payload"};

  const QByteArray packet = protocol_utils_t::create_push_request(topic, 7, message);

  QCOMPARE(packet.size(), static_cast<int>(sizeof(noctua::rpc::request_header_t) + topic.size() + message.size()));

  noctua::rpc::request_header_t header{};
  std::memcpy(&header, packet.constData(), sizeof(header));

  QCOMPARE(header.magic, noctua::rpc::RPC_MAGIC);
  QCOMPARE(header.opcode, noctua::rpc::opcode_t::PUSH);
  QCOMPARE(header.topic_name_len, static_cast<uint16_t>(topic.size()));
  QCOMPARE(header.partition_id, static_cast<uint16_t>(7));
  QCOMPARE(header.message_len, static_cast<uint16_t>(message.size()));
  QCOMPARE(packet.mid(sizeof(header), topic.size()), topic);
  QCOMPARE(packet.mid(sizeof(header) + topic.size(), message.size()), message);
}

void benchmark_gui_qt_test_t::create_pull_request_has_zero_message_length() {
  const QByteArray topic{"bench-topic"};

  const QByteArray packet = protocol_utils_t::create_pull_request(topic, 2);

  noctua::rpc::request_header_t header{};
  std::memcpy(&header, packet.constData(), sizeof(header));

  QCOMPARE(header.magic, noctua::rpc::RPC_MAGIC);
  QCOMPARE(header.opcode, noctua::rpc::opcode_t::PULL);
  QCOMPARE(header.partition_id, static_cast<uint16_t>(2));
  QCOMPARE(header.message_len, static_cast<uint16_t>(0));
  QCOMPARE(packet.mid(sizeof(header), topic.size()), topic);
}

void benchmark_gui_qt_test_t::parse_response_reads_server_header_and_payload() {
  const QByteArray message{"ok-response"};

  noctua::rpc::response_header_t header{};
  header.magic = noctua::rpc::RPC_MAGIC;
  header.opcode = noctua::rpc::opcode_t::PULL;
  header.error_code = noctua::rpc::error_code_t::OK;
  header.message_len = static_cast<uint16_t>(message.size());

  QByteArray packet;
  packet.resize(static_cast<int>(sizeof(header) + message.size()));
  std::memcpy(packet.data(), &header, sizeof(header));
  std::memcpy(packet.data() + sizeof(header), message.constData(), static_cast<size_t>(message.size()));

  const auto response = protocol_utils_t::parse_response(packet);

  QVERIFY(response.valid);
  QCOMPARE(response.opcode, noctua::rpc::opcode_t::PULL);
  QCOMPARE(response.error_code, noctua::rpc::error_code_t::OK);
  QCOMPARE(response.message, message);
}

void benchmark_gui_qt_test_t::parse_response_rejects_invalid_magic() {
  noctua::rpc::response_header_t header{};
  header.magic = 0x12345678;
  header.opcode = noctua::rpc::opcode_t::PUSH;
  header.error_code = noctua::rpc::error_code_t::OK;
  header.message_len = 0;

  QByteArray packet;
  packet.resize(static_cast<int>(sizeof(header)));
  std::memcpy(packet.data(), &header, sizeof(header));

  const auto response = protocol_utils_t::parse_response(packet);

  QVERIFY(!response.valid);
  QCOMPARE(response.opcode, noctua::rpc::opcode_t::ERROR);
  QCOMPARE(response.error_code, noctua::rpc::error_code_t::INTERNAL_ERROR);
}

void benchmark_gui_qt_test_t::gui_request_round_trips_against_rpc_server() {
  const auto port = find_free_port();
  test_rpc_server_t server(port);

  boost::asio::io_context client_ctx;
  boost::asio::ip::tcp::socket socket(client_ctx);
  socket.connect({boost::asio::ip::address_v4::loopback(), port});

  const QByteArray push = protocol_utils_t::create_push_request("bench-topic", 1, "payload");
  boost::asio::write(socket, boost::asio::buffer(push.constData(), static_cast<size_t>(push.size())));

  noctua::rpc::response_header_t push_response{};
  boost::asio::read(socket, boost::asio::buffer(&push_response, sizeof(push_response)));
  QCOMPARE(push_response.magic, noctua::rpc::RPC_MAGIC);
  QCOMPARE(push_response.error_code, noctua::rpc::error_code_t::OK);
  QCOMPARE(push_response.opcode, noctua::rpc::opcode_t::PUSH);

  const QByteArray pull = protocol_utils_t::create_pull_request("bench-topic", 1);
  boost::asio::write(socket, boost::asio::buffer(pull.constData(), static_cast<size_t>(pull.size())));

  noctua::rpc::response_header_t pull_response{};
  boost::asio::read(socket, boost::asio::buffer(&pull_response, sizeof(pull_response)));
  QCOMPARE(pull_response.magic, noctua::rpc::RPC_MAGIC);
  QCOMPARE(pull_response.error_code, noctua::rpc::error_code_t::OK);
  QCOMPARE(pull_response.opcode, noctua::rpc::opcode_t::PULL);
  QCOMPARE(pull_response.message_len, static_cast<uint16_t>(7));

  std::array<char, 32> payload{};
  boost::asio::read(socket, boost::asio::buffer(payload.data(), pull_response.message_len));
  QCOMPARE(QByteArray(payload.data(), pull_response.message_len), QByteArray("payload"));
}

void benchmark_gui_qt_test_t::benchmark_runner_aggregates_stats_from_all_workers() {
  benchmark_runner_t runner;
  benchmark_runner_t::config_t config;
  config.num_writers = 0;
  config.num_readers = 0;
  runner.set_config(config);

  writer_worker_t writer1("127.0.0.1", 8080, "topic", 0, 1, 16);
  writer_worker_t writer2("127.0.0.1", 8080, "topic", 0, 1, 16);
  reader_worker_t reader1("127.0.0.1", 8080, "topic", 0, 1);

  QVERIFY(QObject::connect(&writer1,
                           SIGNAL(stats_updated(worker_t::stats_t)),
                           &runner,
                           SLOT(on_worker_stats_updated(worker_t::stats_t))));
  QVERIFY(QObject::connect(&writer2,
                           SIGNAL(stats_updated(worker_t::stats_t)),
                           &runner,
                           SLOT(on_worker_stats_updated(worker_t::stats_t))));
  QVERIFY(QObject::connect(&reader1,
                           SIGNAL(stats_updated(worker_t::stats_t)),
                           &runner,
                           SLOT(on_worker_stats_updated(worker_t::stats_t))));

  worker_t::stats_t writer_stats1;
  writer_stats1.total_requests = 4;
  writer_stats1.successful_requests = 4;
  writer_stats1.latency_sample_count = 1;
  writer_stats1.latency_total_micros = 1000;
  writer_stats1.last_latency_micros = 1000;
  writer_stats1.has_latency_sample = true;
  writer_stats1.avg_latency_ms = 1.0;
  writer_stats1.min_latency_ms = 1.0;
  writer_stats1.max_latency_ms = 1.0;
  writer_stats1.p95_latency_ms = 1.0;
  writer_stats1.p99_latency_ms = 1.0;

  worker_t::stats_t writer_stats1_final = writer_stats1;
  writer_stats1_final.total_requests = 5;
  writer_stats1_final.failed_requests = 1;
  writer_stats1_final.latency_sample_count = 2;
  writer_stats1_final.latency_total_micros = 3000;
  writer_stats1_final.last_latency_micros = 2000;
  writer_stats1_final.avg_latency_ms = 1.5;
  writer_stats1_final.max_latency_ms = 2.0;
  writer_stats1_final.p95_latency_ms = 2.0;
  writer_stats1_final.p99_latency_ms = 2.0;

  worker_t::stats_t writer_stats2;
  writer_stats2.total_requests = 7;
  writer_stats2.successful_requests = 7;
  writer_stats2.latency_sample_count = 1;
  writer_stats2.latency_total_micros = 3000;
  writer_stats2.last_latency_micros = 3000;
  writer_stats2.has_latency_sample = true;
  writer_stats2.avg_latency_ms = 3.0;
  writer_stats2.min_latency_ms = 3.0;
  writer_stats2.max_latency_ms = 3.0;
  writer_stats2.p95_latency_ms = 3.0;
  writer_stats2.p99_latency_ms = 3.0;

  worker_t::stats_t reader_stats;
  reader_stats.total_requests = 10;
  reader_stats.successful_requests = 10;
  reader_stats.latency_sample_count = 1;
  reader_stats.latency_total_micros = 4000;
  reader_stats.last_latency_micros = 4000;
  reader_stats.has_latency_sample = true;
  reader_stats.avg_latency_ms = 4.0;
  reader_stats.min_latency_ms = 4.0;
  reader_stats.max_latency_ms = 4.0;
  reader_stats.p95_latency_ms = 4.0;
  reader_stats.p99_latency_ms = 4.0;

  worker_t::stats_t reader_stats_final = reader_stats;
  reader_stats_final.total_requests = 11;
  reader_stats_final.failed_requests = 1;
  reader_stats_final.latency_sample_count = 2;
  reader_stats_final.latency_total_micros = 9000;
  reader_stats_final.last_latency_micros = 5000;
  reader_stats_final.avg_latency_ms = 4.5;
  reader_stats_final.max_latency_ms = 5.0;
  reader_stats_final.p95_latency_ms = 5.0;
  reader_stats_final.p99_latency_ms = 5.0;

  emit writer1.stats_updated(writer_stats1);
  emit writer1.stats_updated(writer_stats1_final);
  emit writer2.stats_updated(writer_stats2);
  emit reader1.stats_updated(reader_stats);
  emit reader1.stats_updated(reader_stats_final);

  const auto stats = runner.get_aggregated_stats();

  QCOMPARE(stats.total_requests, static_cast<uint64_t>(23));
  QCOMPARE(stats.successful_requests, static_cast<uint64_t>(21));
  QCOMPARE(stats.failed_requests, static_cast<uint64_t>(2));
  QCOMPARE(stats.writer_total_requests, static_cast<uint64_t>(12));
  QCOMPARE(stats.writer_successful_requests, static_cast<uint64_t>(11));
  QCOMPARE(stats.writer_failed_requests, static_cast<uint64_t>(1));
  QCOMPARE(stats.reader_total_requests, static_cast<uint64_t>(11));
  QCOMPARE(stats.reader_successful_requests, static_cast<uint64_t>(10));
  QCOMPARE(stats.reader_failed_requests, static_cast<uint64_t>(1));
  QVERIFY(qAbs(stats.avg_latency_ms - 3.0) < 0.001);
  QCOMPARE(stats.min_latency_ms, 1.0);
  QCOMPARE(stats.max_latency_ms, 5.0);
  QCOMPARE(stats.p95_latency_ms, 4.0);
  QCOMPARE(stats.p99_latency_ms, 4.0);
  QVERIFY(qAbs(stats.writer_avg_latency_ms - 2.0) < 0.001);
  QVERIFY(qAbs(stats.reader_avg_latency_ms - 4.5) < 0.001);
}

void benchmark_gui_qt_test_t::benchmark_runner_random_partition_stays_within_configured_range() {
  benchmark_runner_t runner;
  benchmark_runner_t::config_t config;
  config.partition_count = 5;
  runner.set_config(config);

  bool saw_zero = false;
  bool saw_upper_bound = false;

  for (int i = 0; i < 500; ++i) {
    const auto partition = runner.random_partition_for_worker();
    QVERIFY2(partition < config.partition_count, "Partition must stay within configured range");
    if (partition == 0) {
      saw_zero = true;
    }
    if (partition == config.partition_count - 1) {
      saw_upper_bound = true;
    }
  }

  QVERIFY(saw_zero);
  QVERIFY(saw_upper_bound);
}

void benchmark_gui_qt_test_t::benchmark_runner_initialize_topic_creates_requested_partition_count() {
  const auto port = find_free_port();
  test_rpc_server_t server(port);

  benchmark_runner_t runner;
  benchmark_runner_t::config_t config;
  config.host = "127.0.0.1";
  config.port = port;
  config.topic_name = "init-topic";
  config.partition_count = 4;
  config.num_writers = 0;
  config.num_readers = 0;
  runner.set_config(config);

  QString error_message;
  QVERIFY2(runner.initialize_topic(&error_message), qPrintable(error_message));

  boost::asio::io_context client_ctx;
  boost::asio::ip::tcp::socket socket(client_ctx);
  socket.connect({boost::asio::ip::address_v4::loopback(), port});

  const QByteArray request = protocol_utils_t::create_pull_request("init-topic", 3);
  boost::asio::write(socket, boost::asio::buffer(request.constData(), static_cast<size_t>(request.size())));

  noctua::rpc::response_header_t response{};
  boost::asio::read(socket, boost::asio::buffer(&response, sizeof(response)));

  QVERIFY(response.magic == noctua::rpc::RPC_MAGIC);
  QVERIFY(response.opcode == noctua::rpc::opcode_t::PULL);
  QVERIFY(response.error_code == noctua::rpc::error_code_t::EMPTY_PARTITION
          || response.error_code == noctua::rpc::error_code_t::OK);
}

QTEST_MAIN(benchmark_gui_qt_test_t)
