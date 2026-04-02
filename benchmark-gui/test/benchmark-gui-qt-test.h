#pragma once

#include <QObject>

class BenchmarkGuiQtTest : public QObject {
  Q_OBJECT

private slots:
  void createPushRequest_matchesRpcWireFormat();
  void createPullRequest_hasZeroMessageLength();
  void parseResponse_readsServerHeaderAndPayload();
  void parseResponse_rejectsInvalidMagic();
  void guiRequest_roundTripsAgainstRpcServer();
  void benchmarkRunner_aggregatesStatsFromAllWorkers();
  void benchmarkRunner_randomPartitionStaysWithinConfiguredRange();
  void benchmarkRunner_initializeTopicCreatesRequestedPartitionCount();
};
