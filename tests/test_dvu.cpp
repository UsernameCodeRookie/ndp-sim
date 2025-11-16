#include <gtest/gtest.h>

#include <iostream>
#include <memory>

#include "../src/comp/dvu.h"
#include "../src/scheduler.h"
#include "../src/trace.h"

/**
 * @brief Test suite for DVU (Division/Remainder Logic Unit) Component
 *
 * Tests the multi-cycle divider with various division operations
 */
class DvuTest : public ::testing::Test {
 protected:
  void SetUp() override {
    scheduler = std::make_unique<EventDriven::EventScheduler>();
    EventDriven::Tracer::getInstance().initialize("test_dvu.log", true);
    EventDriven::Tracer::getInstance().setVerbose(false);
  }

  void TearDown() override {
    EventDriven::Tracer::getInstance().dump();
    scheduler.reset();
  }

  std::unique_ptr<EventDriven::EventScheduler> scheduler;
};

TEST_F(DvuTest, Initialization) {
  const uint64_t CLOCK_PERIOD = 1;
  const uint32_t NUM_LANES = 4;

  auto dvu = std::make_shared<DvuComponent>("DVU", *scheduler, CLOCK_PERIOD,
                                            NUM_LANES);
  dvu->start();

  // Initially, no requests or outputs
  EXPECT_EQ(dvu->getRequestsProcessed(), 0);
  EXPECT_EQ(dvu->getResultsOutput(), 0);
  EXPECT_EQ(dvu->getDivByZeroCount(), 0);

  dvu->stop();
}

TEST_F(DvuTest, UnsignedDivision) {
  const uint64_t CLOCK_PERIOD = 1;
  const uint32_t NUM_LANES = 4;

  auto dvu = std::make_shared<DvuComponent>("DVU", *scheduler, CLOCK_PERIOD,
                                            NUM_LANES);

  dvu->start();

  // Test DIVU: 12 / 3 = 4
  dvu->processRequest(5, DvuComponent::DivOp::DIVU, 12, 3);

  // Run simulation for enough cycles (33 for division + pipeline)
  scheduler->run(50);

  std::cout << "\n=== DVU Unsigned Division Test ===" << std::endl;
  dvu->printStatistics();

  EXPECT_EQ(dvu->getRequestsProcessed(), 1);
  EXPECT_GE(dvu->getResultsOutput(), 1);

  dvu->stop();
}

TEST_F(DvuTest, SignedDivision) {
  const uint64_t CLOCK_PERIOD = 1;
  const uint32_t NUM_LANES = 4;

  auto dvu = std::make_shared<DvuComponent>("DVU", *scheduler, CLOCK_PERIOD,
                                            NUM_LANES);

  dvu->start();

  // Test DIV: -20 / 4 = -5
  dvu->processRequest(10, DvuComponent::DivOp::DIV, -20, 4);

  // Run simulation
  scheduler->run(50);

  std::cout << "\n=== DVU Signed Division Test ===" << std::endl;
  dvu->printStatistics();

  EXPECT_EQ(dvu->getRequestsProcessed(), 1);
  EXPECT_GE(dvu->getResultsOutput(), 1);

  dvu->stop();
}

TEST_F(DvuTest, UnsignedRemainder) {
  const uint64_t CLOCK_PERIOD = 1;
  const uint32_t NUM_LANES = 4;

  auto dvu = std::make_shared<DvuComponent>("DVU", *scheduler, CLOCK_PERIOD,
                                            NUM_LANES);

  dvu->start();

  // Test REMU: 17 % 5 = 2
  dvu->processRequest(15, DvuComponent::DivOp::REMU, 17, 5);

  // Run simulation
  scheduler->run(50);

  std::cout << "\n=== DVU Unsigned Remainder Test ===" << std::endl;
  dvu->printStatistics();

  EXPECT_EQ(dvu->getRequestsProcessed(), 1);
  EXPECT_GE(dvu->getResultsOutput(), 1);

  dvu->stop();
}

TEST_F(DvuTest, SignedRemainder) {
  const uint64_t CLOCK_PERIOD = 1;
  const uint32_t NUM_LANES = 4;

  auto dvu = std::make_shared<DvuComponent>("DVU", *scheduler, CLOCK_PERIOD,
                                            NUM_LANES);

  dvu->start();

  // Test REM: -17 % 5 = -2 (sign follows dividend in signed modulo)
  dvu->processRequest(20, DvuComponent::DivOp::REM, -17, 5);

  // Run simulation
  scheduler->run(50);

  std::cout << "\n=== DVU Signed Remainder Test ===" << std::endl;
  dvu->printStatistics();

  EXPECT_EQ(dvu->getRequestsProcessed(), 1);
  EXPECT_GE(dvu->getResultsOutput(), 1);

  dvu->stop();
}

TEST_F(DvuTest, DivisionByZero) {
  const uint64_t CLOCK_PERIOD = 1;
  const uint32_t NUM_LANES = 4;

  auto dvu = std::make_shared<DvuComponent>("DVU", *scheduler, CLOCK_PERIOD,
                                            NUM_LANES);

  dvu->start();

  // Test division by zero
  dvu->processRequest(25, DvuComponent::DivOp::DIV, 100, 0);

  // Run simulation
  scheduler->run(50);

  std::cout << "\n=== DVU Division by Zero Test ===" << std::endl;
  dvu->printStatistics();

  EXPECT_EQ(dvu->getRequestsProcessed(), 1);
  EXPECT_EQ(dvu->getDivByZeroCount(), 1);
  EXPECT_GE(dvu->getResultsOutput(), 1);

  dvu->stop();
}

TEST_F(DvuTest, MultipleRequests) {
  const uint64_t CLOCK_PERIOD = 1;
  const uint32_t NUM_LANES = 4;

  auto dvu = std::make_shared<DvuComponent>("DVU", *scheduler, CLOCK_PERIOD,
                                            NUM_LANES);

  dvu->start();

  // Submit first request: 100 / 10 = 10
  dvu->processRequest(1, DvuComponent::DivOp::DIVU, 100, 10);

  // Need to wait for first request to complete before sending next
  // (DVU is not fully pipelined in our simulation)
  scheduler->run(50);

  // Submit second request: 50 / 7
  dvu->processRequest(2, DvuComponent::DivOp::DIVU, 50, 7);

  // Continue simulation
  scheduler->run(50);

  std::cout << "\n=== DVU Multiple Requests Test ===" << std::endl;
  dvu->printStatistics();

  EXPECT_GE(dvu->getRequestsProcessed(), 2);
  EXPECT_GE(dvu->getResultsOutput(), 1);

  dvu->stop();
}

TEST_F(DvuTest, LargeNumbers) {
  const uint64_t CLOCK_PERIOD = 1;
  const uint32_t NUM_LANES = 4;

  auto dvu = std::make_shared<DvuComponent>("DVU", *scheduler, CLOCK_PERIOD,
                                            NUM_LANES);

  dvu->start();

  // Test with large numbers: 1000000 / 1000 = 1000
  dvu->processRequest(30, DvuComponent::DivOp::DIVU, 1000000, 1000);

  // Run simulation
  scheduler->run(50);

  std::cout << "\n=== DVU Large Numbers Test ===" << std::endl;
  dvu->printStatistics();

  EXPECT_EQ(dvu->getRequestsProcessed(), 1);
  EXPECT_GE(dvu->getResultsOutput(), 1);

  dvu->stop();
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
