#include <gtest/gtest.h>

#include <iostream>
#include <memory>

#include "../src/components/lsu.h"
#include "../src/components/pingpong.h"
#include "../src/ready_valid_connection.h"
#include "../src/scheduler.h"
#include "../src/trace.h"

/**
 * @brief Test demonstrating connection usage
 *
 * This test creates:
 * 1. A PingPong controller that generates store/load requests
 * 2. An LSU that processes memory operations
 * 3. Two ReadyValidConnections to transfer data between them
 *
 * The connections handle ready-valid handshaking automatically,
 * providing back pressure when needed.
 */
class PingPongConnectionTest : public ::testing::Test {
 protected:
  void SetUp() override {
    scheduler = std::make_unique<EventDriven::EventScheduler>();
    EventDriven::Tracer::getInstance().initialize(
        "test_pingpong_connection.log", true);
    EventDriven::Tracer::getInstance().setVerbose(false);
  }

  void TearDown() override {
    EventDriven::Tracer::getInstance().dump();
    scheduler.reset();
  }

  std::unique_ptr<EventDriven::EventScheduler> scheduler;
};

TEST_F(PingPongConnectionTest, BasicPingPongWithConnection) {
  const uint64_t CLOCK_PERIOD = 1;
  const uint64_t CONNECTION_PERIOD = 1;
  const uint32_t NUM_ITERATIONS = 5;

  // Create components
  auto controller = std::make_shared<PingPongController>(
      "PingPongCtrl", *scheduler, CLOCK_PERIOD, NUM_ITERATIONS);

  auto lsu = std::make_shared<LoadStoreUnit>("LSU", *scheduler, CLOCK_PERIOD,
                                             4 /* banks */, 2 /* queue depth */,
                                             64 /* bank capacity */);

  // Get ports
  auto ctrl_req_out = controller->getPort("req_out");
  auto ctrl_valid_out = controller->getPort("valid_out");
  auto ctrl_resp_in = controller->getPort("resp_in");
  auto ctrl_ready_in = controller->getPort("ready_in");

  auto lsu_req_in = lsu->getPort("req_in");
  auto lsu_valid_in = lsu->getPort("valid");
  auto lsu_resp_out = lsu->getPort("resp_out");
  auto lsu_ready_out = lsu->getPort("ready");

  // Create connections - one connection per data/control signal
  // Request data connection: Controller -> LSU
  auto req_conn = std::make_shared<Architecture::ReadyValidConnection>(
      "ReqConn", *scheduler, CONNECTION_PERIOD, 2);
  req_conn->addSourcePort(ctrl_req_out);
  req_conn->addDestinationPort(lsu_req_in);
  req_conn->setLatency(0);

  // Request valid connection: Controller -> LSU
  auto valid_conn = std::make_shared<Architecture::ReadyValidConnection>(
      "ValidConn", *scheduler, CONNECTION_PERIOD, 2);
  valid_conn->addSourcePort(ctrl_valid_out);
  valid_conn->addDestinationPort(lsu_valid_in);
  valid_conn->setLatency(0);

  // Response connection: LSU -> Controller
  auto resp_conn = std::make_shared<Architecture::ReadyValidConnection>(
      "RespConn", *scheduler, CONNECTION_PERIOD, 2);
  resp_conn->addSourcePort(lsu_resp_out);
  resp_conn->addDestinationPort(ctrl_resp_in);
  resp_conn->setLatency(0);

  // Ready signal connection: LSU -> Controller
  auto ready_conn = std::make_shared<Architecture::ReadyValidConnection>(
      "ReadyConn", *scheduler, CONNECTION_PERIOD, 2);
  ready_conn->addSourcePort(lsu_ready_out);
  ready_conn->addDestinationPort(ctrl_ready_in);
  ready_conn->setLatency(0);

  // Start components and connections
  controller->start();
  lsu->start();
  req_conn->start();
  valid_conn->start();
  resp_conn->start();
  ready_conn->start();

  // Begin the ping-pong sequence
  controller->begin();

  // Run simulation
  const uint64_t MAX_CYCLES = 1000;

  std::cout << "\n=== Starting PingPong Connection Test ===" << std::endl;

  // Run simulation until completion or max cycles
  scheduler->run(MAX_CYCLES);

  std::cout << "\n=== Simulation Complete at cycle "
            << scheduler->getCurrentTime() << " ===" << std::endl;

  // Print statistics
  controller->printStatistics();
  req_conn->printStatistics();
  valid_conn->printStatistics();
  resp_conn->printStatistics();
  ready_conn->printStatistics();

  std::cout << "\nLSU Statistics:" << std::endl;
  std::cout << "Operations completed: " << lsu->getOperationsCompleted()
            << std::endl;
  std::cout << "Cycles stalled: " << lsu->getCyclesStalled() << std::endl;

  // Verify results
  EXPECT_TRUE(controller->isDone());
  EXPECT_EQ(controller->getErrors(), 0);
  EXPECT_EQ(controller->getRequestsSent(), NUM_ITERATIONS * 2);  // Store + Load
  EXPECT_EQ(controller->getResponsesReceived(),
            NUM_ITERATIONS * 2);  // Store + Load responses
  EXPECT_EQ(lsu->getOperationsCompleted(), NUM_ITERATIONS * 2);

  std::cout << "\n=== Test Passed ===" << std::endl;
}

TEST_F(PingPongConnectionTest, PingPongWithLatency) {
  const uint64_t CLOCK_PERIOD = 1;
  const uint64_t CONNECTION_PERIOD = 1;
  const uint32_t NUM_ITERATIONS = 3;
  const uint64_t CONNECTION_LATENCY = 2;  // 2 cycle latency

  // Create components
  auto controller = std::make_shared<PingPongController>(
      "PingPongCtrl", *scheduler, CLOCK_PERIOD, NUM_ITERATIONS);

  auto lsu = std::make_shared<LoadStoreUnit>("LSU", *scheduler, CLOCK_PERIOD, 4,
                                             2, 64);

  // Get ports
  auto ctrl_req_out = controller->getPort("req_out");
  auto ctrl_valid_out = controller->getPort("valid_out");
  auto ctrl_resp_in = controller->getPort("resp_in");
  auto ctrl_ready_in = controller->getPort("ready_in");

  auto lsu_req_in = lsu->getPort("req_in");
  auto lsu_valid_in = lsu->getPort("valid");
  auto lsu_resp_out = lsu->getPort("resp_out");
  auto lsu_ready_out = lsu->getPort("ready");

  // Create connections with latency
  auto req_conn = std::make_shared<Architecture::ReadyValidConnection>(
      "ReqConn", *scheduler, CONNECTION_PERIOD, 2);
  req_conn->addSourcePort(ctrl_req_out);
  req_conn->addDestinationPort(lsu_req_in);
  req_conn->setLatency(CONNECTION_LATENCY);

  auto valid_conn = std::make_shared<Architecture::ReadyValidConnection>(
      "ValidConn", *scheduler, CONNECTION_PERIOD, 2);
  valid_conn->addSourcePort(ctrl_valid_out);
  valid_conn->addDestinationPort(lsu_valid_in);
  valid_conn->setLatency(CONNECTION_LATENCY);

  auto resp_conn = std::make_shared<Architecture::ReadyValidConnection>(
      "RespConn", *scheduler, CONNECTION_PERIOD, 2);
  resp_conn->addSourcePort(lsu_resp_out);
  resp_conn->addDestinationPort(ctrl_resp_in);
  resp_conn->setLatency(CONNECTION_LATENCY);

  auto ready_conn = std::make_shared<Architecture::ReadyValidConnection>(
      "ReadyConn", *scheduler, CONNECTION_PERIOD, 2);
  ready_conn->addSourcePort(lsu_ready_out);
  ready_conn->addDestinationPort(ctrl_ready_in);
  ready_conn->setLatency(CONNECTION_LATENCY);

  // Start everything
  controller->start();
  lsu->start();
  req_conn->start();
  valid_conn->start();
  resp_conn->start();
  ready_conn->start();
  controller->begin();

  // Run simulation
  const uint64_t MAX_CYCLES = 1000;

  std::cout << "\n=== Starting PingPong with Latency Test ===" << std::endl;

  // Run simulation until completion or max cycles
  scheduler->run(MAX_CYCLES);

  std::cout << "\n=== Simulation Complete at cycle "
            << scheduler->getCurrentTime() << " ===" << std::endl;

  // Print statistics
  controller->printStatistics();
  req_conn->printStatistics();
  valid_conn->printStatistics();
  resp_conn->printStatistics();
  ready_conn->printStatistics();

  // Verify results
  EXPECT_TRUE(controller->isDone());
  EXPECT_EQ(controller->getErrors(), 0);
  EXPECT_GT(scheduler->getCurrentTime(),
            NUM_ITERATIONS * 10);  // Should take longer due to latency

  std::cout << "\n=== Test Passed ===" << std::endl;
}

TEST_F(PingPongConnectionTest, PingPongWithBackPressure) {
  const uint64_t CLOCK_PERIOD = 1;
  const uint64_t CONNECTION_PERIOD = 1;
  const uint32_t NUM_ITERATIONS = 10;

  // Create components with smaller queue to cause back pressure
  auto controller = std::make_shared<PingPongController>(
      "PingPongCtrl", *scheduler, CLOCK_PERIOD, NUM_ITERATIONS);

  auto lsu = std::make_shared<LoadStoreUnit>("LSU", *scheduler, CLOCK_PERIOD, 4,
                                             1 /* small queue */, 64);

  // Get ports
  auto ctrl_req_out = controller->getPort("req_out");
  auto ctrl_valid_out = controller->getPort("valid_out");
  auto ctrl_resp_in = controller->getPort("resp_in");
  auto ctrl_ready_in = controller->getPort("ready_in");

  auto lsu_req_in = lsu->getPort("req_in");
  auto lsu_valid_in = lsu->getPort("valid");
  auto lsu_resp_out = lsu->getPort("resp_out");
  auto lsu_ready_out = lsu->getPort("ready");

  // Create connections with small buffer to cause stalls
  auto req_conn = std::make_shared<Architecture::ReadyValidConnection>(
      "ReqConn", *scheduler, CONNECTION_PERIOD, 1);
  req_conn->addSourcePort(ctrl_req_out);
  req_conn->addDestinationPort(lsu_req_in);

  auto valid_conn = std::make_shared<Architecture::ReadyValidConnection>(
      "ValidConn", *scheduler, CONNECTION_PERIOD, 1);
  valid_conn->addSourcePort(ctrl_valid_out);
  valid_conn->addDestinationPort(lsu_valid_in);

  auto resp_conn = std::make_shared<Architecture::ReadyValidConnection>(
      "RespConn", *scheduler, CONNECTION_PERIOD, 1);
  resp_conn->addSourcePort(lsu_resp_out);
  resp_conn->addDestinationPort(ctrl_resp_in);

  auto ready_conn = std::make_shared<Architecture::ReadyValidConnection>(
      "ReadyConn", *scheduler, CONNECTION_PERIOD, 1);
  ready_conn->addSourcePort(lsu_ready_out);
  ready_conn->addDestinationPort(ctrl_ready_in);

  // Start everything
  controller->start();
  lsu->start();
  req_conn->start();
  valid_conn->start();
  resp_conn->start();
  ready_conn->start();
  controller->begin();

  // Run simulation
  const uint64_t MAX_CYCLES = 2000;

  std::cout << "\n=== Starting PingPong with Back Pressure Test ==="
            << std::endl;

  // Run simulation until completion or max cycles
  scheduler->run(MAX_CYCLES);

  std::cout << "\n=== Simulation Complete at cycle "
            << scheduler->getCurrentTime() << " ===" << std::endl;

  // Print statistics
  controller->printStatistics();
  req_conn->printStatistics();
  valid_conn->printStatistics();
  resp_conn->printStatistics();
  ready_conn->printStatistics();

  // Verify results
  EXPECT_TRUE(controller->isDone());
  EXPECT_EQ(controller->getErrors(), 0);

  // Should see some stalls due to small buffers
  std::cout << "\nConnection stalls (back pressure evidence):" << std::endl;
  std::cout << "Request connection stalls: " << req_conn->getStalls()
            << std::endl;
  std::cout << "Response connection stalls: " << resp_conn->getStalls()
            << std::endl;
  std::cout << "Ready connection stalls: " << ready_conn->getStalls()
            << std::endl;

  // With smaller buffers, we expect to see some stalls on ready signal
  EXPECT_GT(ready_conn->getStalls(), 0);

  std::cout << "\n=== Test Passed ===" << std::endl;
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
