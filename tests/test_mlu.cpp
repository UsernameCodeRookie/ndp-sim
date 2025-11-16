#include <gtest/gtest.h>

#include <iostream>
#include <memory>

#include "../src/comp/mlu.h"
#include "../src/scheduler.h"
#include "../src/trace.h"

/**
 * @brief Test suite for MLU (Multiplication Logic Unit) Component
 *
 * Tests the 3-stage pipelined multiplier with various operations
 */
class MluTest : public ::testing::Test {
 protected:
  void SetUp() override {
    scheduler = std::make_unique<EventDriven::EventScheduler>();
    EventDriven::Tracer::getInstance().initialize("test_mlu.log", true);
    EventDriven::Tracer::getInstance().setVerbose(false);
  }

  void TearDown() override {
    EventDriven::Tracer::getInstance().dump();
    scheduler.reset();
  }

  std::unique_ptr<EventDriven::EventScheduler> scheduler;
};

TEST_F(MluTest, Initialization) {
  const uint64_t CLOCK_PERIOD = 1;
  const uint32_t NUM_LANES = 4;

  auto mlu = std::make_shared<MluComponent>("MLU", *scheduler, CLOCK_PERIOD,
                                            NUM_LANES);
  mlu->start();

  // Initially, no requests or outputs
  EXPECT_EQ(mlu->getRequestsProcessed(), 0);
  EXPECT_EQ(mlu->getResultsOutput(), 0);

  mlu->stop();
}

TEST_F(MluTest, MultiplyLowBits) {
  const uint64_t CLOCK_PERIOD = 1;
  const uint32_t NUM_LANES = 4;

  auto mlu = std::make_shared<MluComponent>("MLU", *scheduler, CLOCK_PERIOD,
                                            NUM_LANES);

  mlu->start();

  // Test MUL: 3 * 4 = 12 (lower 32 bits)
  mlu->processRequest(5, MluComponent::MulOp::MUL, 3, 4);

  // Run simulation for 4 cycles (3 pipeline stages + 1)
  scheduler->run(5);

  std::cout << "\n=== MLU Multiply Test ===" << std::endl;
  mlu->printStatistics();

  EXPECT_EQ(mlu->getRequestsProcessed(), 1);
  EXPECT_GE(mlu->getResultsOutput(), 1);

  mlu->stop();
}

TEST_F(MluTest, MultiplyHighBits) {
  const uint64_t CLOCK_PERIOD = 1;
  const uint32_t NUM_LANES = 4;

  auto mlu = std::make_shared<MluComponent>("MLU", *scheduler, CLOCK_PERIOD,
                                            NUM_LANES);

  mlu->start();

  // Test MULH: 0xFFFFFFFF * 0xFFFFFFFF = 0xFFFFFFFE00000001
  // High 32 bits: 0xFFFFFFFE
  // -1 (signed) * -1 (signed) = 1, high bits = 0
  mlu->processRequest(10, MluComponent::MulOp::MULH, -1, -1);

  // Run simulation
  scheduler->run(5);

  std::cout << "\n=== MLU MULH Test ===" << std::endl;
  mlu->printStatistics();

  EXPECT_EQ(mlu->getRequestsProcessed(), 1);
  EXPECT_GE(mlu->getResultsOutput(), 1);

  mlu->stop();
}

TEST_F(MluTest, MultipleRequests) {
  const uint64_t CLOCK_PERIOD = 1;
  const uint32_t NUM_LANES = 4;

  auto mlu = std::make_shared<MluComponent>("MLU", *scheduler, CLOCK_PERIOD,
                                            NUM_LANES);

  mlu->start();

  // Submit multiple requests at different times
  mlu->processRequest(1, MluComponent::MulOp::MUL, 5, 6);  // 5*6=30

  scheduler->run(5);

  mlu->processRequest(2, MluComponent::MulOp::MUL, 7, 8);  // 7*8=56

  // Continue simulation
  scheduler->run(10);

  std::cout << "\n=== MLU Multiple Requests Test ===" << std::endl;
  mlu->printStatistics();

  EXPECT_GE(mlu->getRequestsProcessed(), 2);
  EXPECT_GE(mlu->getResultsOutput(), 1);

  mlu->stop();
}

TEST_F(MluTest, MulhsuOperation) {
  const uint64_t CLOCK_PERIOD = 1;
  const uint32_t NUM_LANES = 4;

  auto mlu = std::make_shared<MluComponent>("MLU", *scheduler, CLOCK_PERIOD,
                                            NUM_LANES);

  mlu->start();

  // Test MULHSU: -1 (signed) * 2 (unsigned) = -2, high bits
  mlu->processRequest(20, MluComponent::MulOp::MULHSU, -1, 2);

  // Run simulation
  scheduler->run(5);

  std::cout << "\n=== MLU MULHSU Test ===" << std::endl;
  mlu->printStatistics();

  EXPECT_EQ(mlu->getRequestsProcessed(), 1);
  EXPECT_GE(mlu->getResultsOutput(), 1);

  mlu->stop();
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
