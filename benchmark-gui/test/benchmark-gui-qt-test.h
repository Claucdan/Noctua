#pragma once

#include <QObject>

class benchmark_gui_qt_test_t : public QObject {
  Q_OBJECT

private slots:
  void create_push_request_matches_rpc_wire_format();
  void create_pull_request_has_zero_message_length();
  void parse_response_reads_server_header_and_payload();
  void parse_response_rejects_invalid_magic();
  void gui_request_round_trips_against_rpc_server();
  void benchmark_runner_aggregates_stats_from_all_workers();
  void benchmark_runner_random_partition_stays_within_configured_range();
  void benchmark_runner_initialize_topic_creates_requested_partition_count();
};
