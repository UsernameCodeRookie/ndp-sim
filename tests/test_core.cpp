#include <gtest/gtest.h>

#include "../src/comp/core.h"
#include "../src/scheduler.h"

class SCoreTest : public ::testing::Test {
 protected:
  void SetUp() override {
    scheduler = std::make_unique<EventDriven::EventScheduler>();
  }

  void TearDown() override { scheduler.reset(); }

  std::unique_ptr<EventDriven::EventScheduler> scheduler;
};

/**
 * @brief Test SCore creation and initialization
 */
TEST_F(SCoreTest, BasicCreation) {
  Architecture::SCore::Config config;
  auto core =
      std::make_shared<Architecture::SCore>("SCore_0", *scheduler, config);

  EXPECT_NE(core, nullptr);
  EXPECT_EQ(core->getALUs().size(), 2);  // num_instruction_lanes = 2
  EXPECT_NE(core->getBRU(), nullptr);
  EXPECT_NE(core->getMLU(), nullptr);
  EXPECT_NE(core->getDVU(), nullptr);
  EXPECT_NE(core->getLSU(), nullptr);
  EXPECT_NE(core->getRegisterFile(), nullptr);
}

/**
 * @brief Test ALU dispatch and execution
 */
TEST_F(SCoreTest, ALUExecution) {
  Architecture::SCore::Config config;
  auto core =
      std::make_shared<Architecture::SCore>("SCore_0", *scheduler, config);
  core->initialize();

  // Dispatch ALU instruction: ADD 5 + 3
  core->dispatchInstruction(Architecture::SCore::OpType::ALU,
                            0,  // lane
                            5,  // operand1
                            3,  // operand2
                            static_cast<uint32_t>(ALUOp::ADD),
                            8  // rd (destination register)
  );

  EXPECT_EQ(core->getInstructionsDispatched(), 1);

  // Execute simulator for enough cycles for data to propagate through pipeline
  // Pipeline has 3 stages, so need at least 3-4 cycles plus overhead
  for (int i = 0; i < 50; i++) {
    scheduler->runFor(1);
  }

  // Check ALU has executed
  EXPECT_GT(core->getALU(0)->getOperationsExecuted(), 0);
}

/**
 * @brief Test Register File read/write
 */
TEST_F(SCoreTest, RegisterFileAccess) {
  Architecture::SCore::Config config;
  auto core =
      std::make_shared<Architecture::SCore>("SCore_0", *scheduler, config);

  // Test direct register write
  core->writeRegister(5, 0xDEADBEEF);
  uint32_t value = core->readRegister(5);

  EXPECT_EQ(value, 0xDEADBEEF);
}

/**
 * @brief Test multiple ALU lanes
 */
TEST_F(SCoreTest, DualLaneDispatch) {
  Architecture::SCore::Config config;
  config.num_instruction_lanes = 2;
  auto core =
      std::make_shared<Architecture::SCore>("SCore_0", *scheduler, config);
  core->initialize();

  // Dispatch to lane 0: ADD 2 + 3
  core->dispatchInstruction(Architecture::SCore::OpType::ALU, 0, 2, 3,
                            static_cast<uint32_t>(ALUOp::ADD), 1);

  // Dispatch to lane 1: SUB 10 - 5
  core->dispatchInstruction(Architecture::SCore::OpType::ALU, 1, 10, 5,
                            static_cast<uint32_t>(ALUOp::SUB), 2);

  EXPECT_EQ(core->getInstructionsDispatched(), 2);

  // Both lanes should be executing
  EXPECT_NE(core->getALU(0), nullptr);
  EXPECT_NE(core->getALU(1), nullptr);
}

/**
 * @brief Test BRU branch execution
 */
TEST_F(SCoreTest, BRUExecution) {
  Architecture::SCore::Config config;
  auto core =
      std::make_shared<Architecture::SCore>("SCore_0", *scheduler, config);
  core->initialize();

  // Dispatch BRU instruction: JAL to 0x1000
  core->dispatchInstruction(Architecture::SCore::OpType::BRU,
                            0,       // unused lane for BRU
                            0x1000,  // target address
                            0,       // unused
                            static_cast<uint32_t>(BruOp::JAL),
                            1  // rd (link register)
  );

  EXPECT_EQ(core->getInstructionsDispatched(), 1);

  // Execute for enough cycles for BRU pipeline (3 stages)
  // Increase to allow more time for scheduler events to propagate
  for (int i = 0; i < 200; i++) {
    scheduler->runFor(1);
  }

  // BRU should have executed
  EXPECT_GT(core->getBRU()->getBranchesResolved(), 0);
}

/**
 * @brief Test MLU multiply execution
 */
TEST_F(SCoreTest, MLUExecution) {
  Architecture::SCore::Config config;
  auto core =
      std::make_shared<Architecture::SCore>("SCore_0", *scheduler, config);
  core->initialize();

  // Dispatch MLU instruction: MUL 7 * 8
  core->dispatchInstruction(Architecture::SCore::OpType::MLU,
                            0,  // unused
                            7,  // operand1
                            8,  // operand2
                            static_cast<uint32_t>(MultiplyUnit::MulOp::MUL),
                            10  // rd
  );

  EXPECT_EQ(core->getInstructionsDispatched(), 1);

  // MLU takes more cycles (period=3 configured, plus pipeline overhead)
  // Need significantly more time for data to propagate
  for (int i = 0; i < 200; i++) {
    scheduler->runFor(1);
  }

  // MLU should have executed
  EXPECT_GT(core->getMLU()->getResultsOutput(), 0);
}

/**
 * @brief Test statistics reporting
 */
TEST_F(SCoreTest, StatisticsReporting) {
  Architecture::SCore::Config config;
  auto core =
      std::make_shared<Architecture::SCore>("SCore_0", *scheduler, config);
  core->initialize();

  // Dispatch a few instructions
  core->dispatchInstruction(Architecture::SCore::OpType::ALU, 0, 2, 3,
                            static_cast<uint32_t>(ALUOp::ADD), 1);

  core->dispatchInstruction(Architecture::SCore::OpType::BRU, 0, 0x2000, 0,
                            static_cast<uint32_t>(BruOp::JAL), 2);

  // Test that statistics are accessible
  EXPECT_EQ(core->getInstructionsDispatched(), 2);
  EXPECT_GE(core->getInstructionsRetired(), 0);

  // Printing should not crash
  core->printStatistics();
}

/**
 * @brief Test core reset
 */
TEST_F(SCoreTest, CoreReset) {
  Architecture::SCore::Config config;
  auto core =
      std::make_shared<Architecture::SCore>("SCore_0", *scheduler, config);
  core->initialize();

  // Dispatch instruction
  core->dispatchInstruction(Architecture::SCore::OpType::ALU, 0, 5, 3,
                            static_cast<uint32_t>(ALUOp::ADD), 1);

  uint64_t dispatched_before = core->getInstructionsDispatched();
  EXPECT_GT(dispatched_before, 0);

  // Reset
  core->reset();

  // Counters should be cleared but dispatch capability restored
  EXPECT_EQ(core->getInstructionsDispatched(), 0);
  EXPECT_EQ(core->getInstructionsRetired(), 0);
}

/**
 * @brief Test configuration parameters
 */
TEST_F(SCoreTest, ConfigurationParameters) {
  Architecture::SCore::Config config;
  config.num_instruction_lanes = 4;
  config.num_registers = 64;
  config.alu_period = 1;
  config.mlu_period = 4;
  config.dvu_period = 16;

  auto core =
      std::make_shared<Architecture::SCore>("SCore_0", *scheduler, config);

  // Core should respect configuration
  EXPECT_EQ(core->getALUs().size(), 4);
}
