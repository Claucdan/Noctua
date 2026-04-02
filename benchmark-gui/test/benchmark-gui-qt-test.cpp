#define private public
#include "benchmarkrunner.h"
#undef private

#include "benchmark-gui-qt-test.h"
#include "protocolutils.h"

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
    boost::asio::ip::tcp::acceptor acceptor(
        io_ctx, boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), 0));
    return acceptor.local_endpoint().port();
}

class TestRpcServer {
public:
    explicit TestRpcServer(uint16_t port)
        : server_(io_ctx_, port) {
        server_.start();
        thread_ = std::thread([this]() { io_ctx_.run(); });
        std::this_thread::sleep_for(50ms);
    }

    ~TestRpcServer() {
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

void BenchmarkGuiQtTest::createPushRequest_matchesRpcWireFormat() {
    const QByteArray topic{"bench-topic"};
    const QByteArray message{"payload"};

    const QByteArray packet = ProtocolUtils::createPushRequest(topic, 7, message);

    QCOMPARE(
        packet.size(),
        static_cast<int>(sizeof(noctua::rpc::request_header_t) + topic.size() + message.size()));

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

void BenchmarkGuiQtTest::createPullRequest_hasZeroMessageLength() {
    const QByteArray topic{"bench-topic"};

    const QByteArray packet = ProtocolUtils::createPullRequest(topic, 2);

    noctua::rpc::request_header_t header{};
    std::memcpy(&header, packet.constData(), sizeof(header));

    QCOMPARE(header.magic, noctua::rpc::RPC_MAGIC);
    QCOMPARE(header.opcode, noctua::rpc::opcode_t::PULL);
    QCOMPARE(header.partition_id, static_cast<uint16_t>(2));
    QCOMPARE(header.message_len, static_cast<uint16_t>(0));
    QCOMPARE(packet.mid(sizeof(header), topic.size()), topic);
}

void BenchmarkGuiQtTest::parseResponse_readsServerHeaderAndPayload() {
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

    QVERIFY(response.valid);
    QCOMPARE(response.opcode, noctua::rpc::opcode_t::PULL);
    QCOMPARE(response.errorCode, noctua::rpc::error_code_t::OK);
    QCOMPARE(response.message, message);
}

void BenchmarkGuiQtTest::parseResponse_rejectsInvalidMagic() {
    noctua::rpc::response_header_t header{};
    header.magic = 0x12345678;
    header.opcode = noctua::rpc::opcode_t::PUSH;
    header.error_code = noctua::rpc::error_code_t::OK;
    header.message_len = 0;

    QByteArray packet;
    packet.resize(static_cast<int>(sizeof(header)));
    std::memcpy(packet.data(), &header, sizeof(header));

    const auto response = ProtocolUtils::parseResponse(packet);

    QVERIFY(!response.valid);
    QCOMPARE(response.opcode, noctua::rpc::opcode_t::ERROR);
    QCOMPARE(response.errorCode, noctua::rpc::error_code_t::INTERNAL_ERROR);
}

void BenchmarkGuiQtTest::guiRequest_roundTripsAgainstRpcServer() {
    const auto port = find_free_port();
    TestRpcServer server(port);

    boost::asio::io_context client_ctx;
    boost::asio::ip::tcp::socket socket(client_ctx);
    socket.connect({boost::asio::ip::address_v4::loopback(), port});

    const QByteArray push = ProtocolUtils::createPushRequest("bench-topic", 1, "payload");
    boost::asio::write(socket, boost::asio::buffer(push.constData(), static_cast<size_t>(push.size())));

    noctua::rpc::response_header_t pushResponse{};
    boost::asio::read(socket, boost::asio::buffer(&pushResponse, sizeof(pushResponse)));
    QCOMPARE(pushResponse.magic, noctua::rpc::RPC_MAGIC);
    QCOMPARE(pushResponse.error_code, noctua::rpc::error_code_t::OK);
    QCOMPARE(pushResponse.opcode, noctua::rpc::opcode_t::PUSH);

    const QByteArray pull = ProtocolUtils::createPullRequest("bench-topic", 1);
    boost::asio::write(socket, boost::asio::buffer(pull.constData(), static_cast<size_t>(pull.size())));

    noctua::rpc::response_header_t pullResponse{};
    boost::asio::read(socket, boost::asio::buffer(&pullResponse, sizeof(pullResponse)));
    QCOMPARE(pullResponse.magic, noctua::rpc::RPC_MAGIC);
    QCOMPARE(pullResponse.error_code, noctua::rpc::error_code_t::OK);
    QCOMPARE(pullResponse.opcode, noctua::rpc::opcode_t::PULL);
    QCOMPARE(pullResponse.message_len, static_cast<uint16_t>(7));

    std::array<char, 32> payload{};
    boost::asio::read(socket, boost::asio::buffer(payload.data(), pullResponse.message_len));
    QCOMPARE(QByteArray(payload.data(), pullResponse.message_len), QByteArray("payload"));
}

void BenchmarkGuiQtTest::benchmarkRunner_aggregatesStatsFromAllWorkers() {
    BenchmarkRunner runner;
    BenchmarkRunner::Config config;
    config.numWriters = 0;
    config.numReaders = 0;
    runner.setConfig(config);

    WriterWorker writer1("127.0.0.1", 8080, "topic", 0, 1, 16);
    WriterWorker writer2("127.0.0.1", 8080, "topic", 0, 1, 16);
    ReaderWorker reader1("127.0.0.1", 8080, "topic", 0, 1);

    QVERIFY(QObject::connect(&writer1, SIGNAL(statsUpdated(Worker::Stats)),
                             &runner, SLOT(onWorkerStatsUpdated(Worker::Stats))));
    QVERIFY(QObject::connect(&writer2, SIGNAL(statsUpdated(Worker::Stats)),
                             &runner, SLOT(onWorkerStatsUpdated(Worker::Stats))));
    QVERIFY(QObject::connect(&reader1, SIGNAL(statsUpdated(Worker::Stats)),
                             &runner, SLOT(onWorkerStatsUpdated(Worker::Stats))));

    Worker::Stats writerStats1;
    writerStats1.totalRequests = 4;
    writerStats1.successfulRequests = 4;
    writerStats1.latencySampleCount = 1;
    writerStats1.latencyTotalMicros = 1000;
    writerStats1.lastLatencyMicros = 1000;
    writerStats1.hasLatencySample = true;
    writerStats1.avgLatencyMs = 1.0;
    writerStats1.minLatencyMs = 1.0;
    writerStats1.maxLatencyMs = 1.0;
    writerStats1.p95LatencyMs = 1.0;
    writerStats1.p99LatencyMs = 1.0;

    Worker::Stats writerStats1Final = writerStats1;
    writerStats1Final.totalRequests = 5;
    writerStats1Final.failedRequests = 1;
    writerStats1Final.latencySampleCount = 2;
    writerStats1Final.latencyTotalMicros = 3000;
    writerStats1Final.lastLatencyMicros = 2000;
    writerStats1Final.avgLatencyMs = 1.5;
    writerStats1Final.maxLatencyMs = 2.0;
    writerStats1Final.p95LatencyMs = 2.0;
    writerStats1Final.p99LatencyMs = 2.0;

    Worker::Stats writerStats2;
    writerStats2.totalRequests = 7;
    writerStats2.successfulRequests = 7;
    writerStats2.latencySampleCount = 1;
    writerStats2.latencyTotalMicros = 3000;
    writerStats2.lastLatencyMicros = 3000;
    writerStats2.hasLatencySample = true;
    writerStats2.avgLatencyMs = 3.0;
    writerStats2.minLatencyMs = 3.0;
    writerStats2.maxLatencyMs = 3.0;
    writerStats2.p95LatencyMs = 3.0;
    writerStats2.p99LatencyMs = 3.0;

    Worker::Stats readerStats;
    readerStats.totalRequests = 10;
    readerStats.successfulRequests = 10;
    readerStats.latencySampleCount = 1;
    readerStats.latencyTotalMicros = 4000;
    readerStats.lastLatencyMicros = 4000;
    readerStats.hasLatencySample = true;
    readerStats.avgLatencyMs = 4.0;
    readerStats.minLatencyMs = 4.0;
    readerStats.maxLatencyMs = 4.0;
    readerStats.p95LatencyMs = 4.0;
    readerStats.p99LatencyMs = 4.0;

    Worker::Stats readerStatsFinal = readerStats;
    readerStatsFinal.totalRequests = 11;
    readerStatsFinal.failedRequests = 1;
    readerStatsFinal.latencySampleCount = 2;
    readerStatsFinal.latencyTotalMicros = 9000;
    readerStatsFinal.lastLatencyMicros = 5000;
    readerStatsFinal.avgLatencyMs = 4.5;
    readerStatsFinal.maxLatencyMs = 5.0;
    readerStatsFinal.p95LatencyMs = 5.0;
    readerStatsFinal.p99LatencyMs = 5.0;

    emit writer1.statsUpdated(writerStats1);
    emit writer1.statsUpdated(writerStats1Final);
    emit writer2.statsUpdated(writerStats2);
    emit reader1.statsUpdated(readerStats);
    emit reader1.statsUpdated(readerStatsFinal);

    const auto stats = runner.getAggregatedStats();

    QCOMPARE(stats.totalRequests, static_cast<uint64_t>(23));
    QCOMPARE(stats.successfulRequests, static_cast<uint64_t>(21));
    QCOMPARE(stats.failedRequests, static_cast<uint64_t>(2));
    QCOMPARE(stats.writerTotalRequests, static_cast<uint64_t>(12));
    QCOMPARE(stats.writerSuccessfulRequests, static_cast<uint64_t>(11));
    QCOMPARE(stats.writerFailedRequests, static_cast<uint64_t>(1));
    QCOMPARE(stats.readerTotalRequests, static_cast<uint64_t>(11));
    QCOMPARE(stats.readerSuccessfulRequests, static_cast<uint64_t>(10));
    QCOMPARE(stats.readerFailedRequests, static_cast<uint64_t>(1));
    QVERIFY(qAbs(stats.avgLatencyMs - 3.0) < 0.001);
    QCOMPARE(stats.minLatencyMs, 1.0);
    QCOMPARE(stats.maxLatencyMs, 5.0);
    QCOMPARE(stats.p95LatencyMs, 4.0);
    QCOMPARE(stats.p99LatencyMs, 4.0);
    QVERIFY(qAbs(stats.writerAvgLatencyMs - 2.0) < 0.001);
    QVERIFY(qAbs(stats.readerAvgLatencyMs - 4.5) < 0.001);
}

void BenchmarkGuiQtTest::benchmarkRunner_randomPartitionStaysWithinConfiguredRange() {
    BenchmarkRunner runner;
    BenchmarkRunner::Config config;
    config.partitionCount = 5;
    runner.setConfig(config);

    bool sawZero = false;
    bool sawUpperBound = false;

    for (int i = 0; i < 500; ++i) {
        const auto partition = runner.randomPartitionForWorker();
        QVERIFY2(partition < config.partitionCount, "Partition must stay within configured range");
        if (partition == 0) {
            sawZero = true;
        }
        if (partition == config.partitionCount - 1) {
            sawUpperBound = true;
        }
    }

    QVERIFY(sawZero);
    QVERIFY(sawUpperBound);
}

void BenchmarkGuiQtTest::benchmarkRunner_initializeTopicCreatesRequestedPartitionCount() {
    const auto port = find_free_port();
    TestRpcServer server(port);

    BenchmarkRunner runner;
    BenchmarkRunner::Config config;
    config.host = "127.0.0.1";
    config.port = port;
    config.topicName = "init-topic";
    config.partitionCount = 4;
    config.numWriters = 0;
    config.numReaders = 0;
    runner.setConfig(config);

    QString errorMessage;
    QVERIFY2(runner.initializeTopic(&errorMessage), qPrintable(errorMessage));

    boost::asio::io_context clientCtx;
    boost::asio::ip::tcp::socket socket(clientCtx);
    socket.connect({boost::asio::ip::address_v4::loopback(), port});

    const QByteArray request = ProtocolUtils::createPullRequest("init-topic", 3);
    boost::asio::write(socket, boost::asio::buffer(request.constData(), static_cast<size_t>(request.size())));

    noctua::rpc::response_header_t response{};
    boost::asio::read(socket, boost::asio::buffer(&response, sizeof(response)));

    QVERIFY(response.magic == noctua::rpc::RPC_MAGIC);
    QVERIFY(response.opcode == noctua::rpc::opcode_t::PULL);
    QVERIFY(response.error_code == noctua::rpc::error_code_t::EMPTY_PARTITION ||
            response.error_code == noctua::rpc::error_code_t::OK);
}

QTEST_MAIN(BenchmarkGuiQtTest)
