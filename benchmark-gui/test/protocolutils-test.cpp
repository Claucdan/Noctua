#include "benchmarkrunner.h"

#include "protocolutils.h"

#include "rpc/rpc-protocol.h"
#include "rpc/rpc-server.h"

#include <boost/asio.hpp>
#include <gtest/gtest.h>

#include <QCoreApplication>

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
  boost::asio::ip::tcp::acceptor acceptor(
      io_ctx, boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), 0));
  return acceptor.local_endpoint().port();
}

TEST(ProtocolUtilsTest, create_push_request_matches_rpc_wire_format) {
  const QByteArray topic{"bench-topic"};
  const QByteArray message{"payload"};

  const QByteArray packet = ProtocolUtils::createPushRequest(topic, 7, message);

  ASSERT_EQ(packet.size(), static_cast<int>(sizeof(noctua::rpc::request_header_t) + topic.size() + message.size()));

  noctua::rpc::request_header_t header{};
  std::memcpy(&header, packet.constData(), sizeof(header));

  EXPECT_EQ(header.magic, noctua::rpc::RPC_MAGIC);
  EXPECT_EQ(header.opcode, noctua::rpc::opcode_t::PUSH);
  EXPECT_EQ(header.topic_name_len, topic.size());
  EXPECT_EQ(header.partition_id, 7);
  EXPECT_EQ(header.message_len, message.size());
  EXPECT_EQ(packet.mid(sizeof(header), topic.size()), topic);
  EXPECT_EQ(packet.mid(sizeof(header) + topic.size(), message.size()), message);
}

TEST(ProtocolUtilsTest, create_pull_request_has_zero_message_length) {
  const QByteArray topic{"bench-topic"};

  const QByteArray packet = ProtocolUtils::createPullRequest(topic, 2);

  noctua::rpc::request_header_t header{};
  std::memcpy(&header, packet.constData(), sizeof(header));

  EXPECT_EQ(header.magic, noctua::rpc::RPC_MAGIC);
  EXPECT_EQ(header.opcode, noctua::rpc::opcode_t::PULL);
  EXPECT_EQ(header.partition_id, 2);
  EXPECT_EQ(header.message_len, 0);
  EXPECT_EQ(packet.mid(sizeof(header), topic.size()), topic);
}

TEST(ProtocolUtilsTest, parse_response_reads_server_header_and_payload) {
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

  const auto response = ProtocolUtils::parseResponse(packet);

  EXPECT_TRUE(response.valid);
  EXPECT_EQ(response.opcode, noctua::rpc::opcode_t::PULL);
  EXPECT_EQ(response.errorCode, noctua::rpc::error_code_t::OK);
  EXPECT_EQ(response.message, message);
}

TEST(ProtocolUtilsTest, parse_response_rejects_invalid_magic) {
  noctua::rpc::response_header_t header{};
  header.magic = 0x12345678;
  header.opcode = noctua::rpc::opcode_t::PUSH;
  header.error_code = noctua::rpc::error_code_t::OK;
  header.message_len = 0;

  QByteArray packet;
  packet.resize(static_cast<int>(sizeof(header)));
  std::memcpy(packet.data(), &header, sizeof(header));

  const auto response = ProtocolUtils::parseResponse(packet);

  EXPECT_FALSE(response.valid);
  EXPECT_EQ(response.opcode, noctua::rpc::opcode_t::ERROR);
  EXPECT_EQ(response.errorCode, noctua::rpc::error_code_t::INTERNAL_ERROR);
}

TEST(ProtocolUtilsIntegrationTest, gui_request_round_trips_against_rpc_server) {
  const auto port = find_free_port();

  boost::asio::io_context server_ctx;
  noctua::rpc::rpc_server_t server(server_ctx, port);
  server.start();

  std::thread server_thread([&server_ctx]() {
    server_ctx.run();
  });

  std::this_thread::sleep_for(50ms);

  boost::asio::io_context client_ctx;
  boost::asio::ip::tcp::socket socket(client_ctx);
  socket.connect({boost::asio::ip::address_v4::loopback(), port});

  const QByteArray push = ProtocolUtils::createPushRequest("bench-topic", 1, "payload");
  boost::asio::write(socket, boost::asio::buffer(push.constData(), static_cast<size_t>(push.size())));

  noctua::rpc::response_header_t push_response{};
  boost::asio::read(socket, boost::asio::buffer(&push_response, sizeof(push_response)));
  EXPECT_EQ(push_response.magic, noctua::rpc::RPC_MAGIC);
  EXPECT_EQ(push_response.error_code, noctua::rpc::error_code_t::OK);
  EXPECT_EQ(push_response.opcode, noctua::rpc::opcode_t::PUSH);

  const QByteArray pull = ProtocolUtils::createPullRequest("bench-topic", 1);
  boost::asio::write(socket, boost::asio::buffer(pull.constData(), static_cast<size_t>(pull.size())));

  noctua::rpc::response_header_t pull_response{};
  boost::asio::read(socket, boost::asio::buffer(&pull_response, sizeof(pull_response)));
  ASSERT_EQ(pull_response.magic, noctua::rpc::RPC_MAGIC);
  ASSERT_EQ(pull_response.error_code, noctua::rpc::error_code_t::OK);
  ASSERT_EQ(pull_response.opcode, noctua::rpc::opcode_t::PULL);
  ASSERT_EQ(pull_response.message_len, 7);

  std::array<char, 32> payload{};
  boost::asio::read(socket, boost::asio::buffer(payload.data(), pull_response.message_len));
  EXPECT_EQ(QByteArray(payload.data(), pull_response.message_len), QByteArray("payload"));

  boost::system::error_code ec;
  socket.close(ec);
  server.stop();
  server_ctx.stop();
  server_thread.join();
}

TEST(BenchmarkRunnerTest, aggregates_stats_from_all_workers_without_overwrite) {
  BenchmarkRunner runner;
  BenchmarkRunner::Config config;
  config.numWriters = 0;
  config.numReaders = 0;
  runner.setConfig(config);
  runner.start();

  std::this_thread::sleep_for(1100ms);

  WriterWorker writer1("127.0.0.1", 8080, "topic", 0, 1, 16);
  WriterWorker writer2("127.0.0.1", 8080, "topic", 0, 1, 16);
  ReaderWorker reader1("127.0.0.1", 8080, "topic", 0, 1);

  ASSERT_TRUE(QObject::connect(&writer1, SIGNAL(statsUpdated(Worker::Stats)),
                               &runner, SLOT(onWorkerStatsUpdated(Worker::Stats))));
  ASSERT_TRUE(QObject::connect(&writer2, SIGNAL(statsUpdated(Worker::Stats)),
                               &runner, SLOT(onWorkerStatsUpdated(Worker::Stats))));
  ASSERT_TRUE(QObject::connect(&reader1, SIGNAL(statsUpdated(Worker::Stats)),
                               &runner, SLOT(onWorkerStatsUpdated(Worker::Stats))));

  Worker::Stats writer_stats_1;
  writer_stats_1.totalRequests = 4;
  writer_stats_1.successfulRequests = 4;
  writer_stats_1.failedRequests = 0;
  writer_stats_1.latencySampleCount = 1;
  writer_stats_1.latencyTotalMicros = 1000;
  writer_stats_1.lastLatencyMicros = 1000;
  writer_stats_1.hasLatencySample = true;
  writer_stats_1.avgLatencyMs = 1.0;
  writer_stats_1.minLatencyMs = 1.0;
  writer_stats_1.maxLatencyMs = 1.0;
  writer_stats_1.p95LatencyMs = 1.0;
  writer_stats_1.p99LatencyMs = 1.0;

  Worker::Stats writer_stats_1_final = writer_stats_1;
  writer_stats_1_final.totalRequests = 5;
  writer_stats_1_final.failedRequests = 1;
  writer_stats_1_final.latencySampleCount = 2;
  writer_stats_1_final.latencyTotalMicros = 3000;
  writer_stats_1_final.lastLatencyMicros = 2000;
  writer_stats_1_final.avgLatencyMs = 1.5;
  writer_stats_1_final.maxLatencyMs = 2.0;
  writer_stats_1_final.p95LatencyMs = 2.0;
  writer_stats_1_final.p99LatencyMs = 2.0;

  Worker::Stats writer_stats_2;
  writer_stats_2.totalRequests = 7;
  writer_stats_2.successfulRequests = 7;
  writer_stats_2.failedRequests = 0;
  writer_stats_2.latencySampleCount = 1;
  writer_stats_2.latencyTotalMicros = 3000;
  writer_stats_2.lastLatencyMicros = 3000;
  writer_stats_2.hasLatencySample = true;
  writer_stats_2.avgLatencyMs = 3.0;
  writer_stats_2.minLatencyMs = 3.0;
  writer_stats_2.maxLatencyMs = 3.0;
  writer_stats_2.p95LatencyMs = 3.0;
  writer_stats_2.p99LatencyMs = 3.0;

  Worker::Stats reader_stats;
  reader_stats.totalRequests = 10;
  reader_stats.successfulRequests = 10;
  reader_stats.failedRequests = 0;
  reader_stats.latencySampleCount = 1;
  reader_stats.latencyTotalMicros = 4000;
  reader_stats.lastLatencyMicros = 4000;
  reader_stats.hasLatencySample = true;
  reader_stats.avgLatencyMs = 4.0;
  reader_stats.minLatencyMs = 4.0;
  reader_stats.maxLatencyMs = 4.0;
  reader_stats.p95LatencyMs = 4.0;
  reader_stats.p99LatencyMs = 4.0;

  Worker::Stats reader_stats_final = reader_stats;
  reader_stats_final.totalRequests = 11;
  reader_stats_final.failedRequests = 1;
  reader_stats_final.latencySampleCount = 2;
  reader_stats_final.latencyTotalMicros = 9000;
  reader_stats_final.lastLatencyMicros = 5000;
  reader_stats_final.avgLatencyMs = 4.5;
  reader_stats_final.maxLatencyMs = 5.0;
  reader_stats_final.p95LatencyMs = 5.0;
  reader_stats_final.p99LatencyMs = 5.0;

  emit writer1.statsUpdated(writer_stats_1);
  emit writer1.statsUpdated(writer_stats_1_final);
  emit writer2.statsUpdated(writer_stats_2);
  emit reader1.statsUpdated(reader_stats);
  emit reader1.statsUpdated(reader_stats_final);

  const auto stats = runner.getAggregatedStats();

  EXPECT_EQ(stats.totalRequests, 23);
  EXPECT_EQ(stats.successfulRequests, 21);
  EXPECT_EQ(stats.failedRequests, 2);
  EXPECT_EQ(stats.writerTotalRequests, 12);
  EXPECT_EQ(stats.writerSuccessfulRequests, 11);
  EXPECT_EQ(stats.writerFailedRequests, 1);
  EXPECT_EQ(stats.readerTotalRequests, 11);
  EXPECT_EQ(stats.readerSuccessfulRequests, 10);
  EXPECT_EQ(stats.readerFailedRequests, 1);
  EXPECT_NEAR(stats.avgLatencyMs, 3.0, 0.001);
  EXPECT_DOUBLE_EQ(stats.minLatencyMs, 1.0);
  EXPECT_DOUBLE_EQ(stats.maxLatencyMs, 5.0);
  EXPECT_DOUBLE_EQ(stats.p95LatencyMs, 4.0);
  EXPECT_DOUBLE_EQ(stats.p99LatencyMs, 4.0);
  EXPECT_NEAR(stats.writerAvgLatencyMs, 2.0, 0.001);
  EXPECT_NEAR(stats.readerAvgLatencyMs, 4.5, 0.001);
  EXPECT_GT(stats.requestsPerSecond, 0.0);

  runner.stop();
}

} // namespace

int main(int argc, char** argv) {
  QCoreApplication app(argc, argv);
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
