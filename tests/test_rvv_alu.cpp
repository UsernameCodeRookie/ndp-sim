#include <gtest/gtest.h>

#include <map>
#include <memory>

#include "comp/rvv/rvv_alu.h"
#include "tick.h"

/**
 * @brief Test suite for RVV Vector ALU
 *
 * Tests functional model and latency parameters
 */
class RVVVectorALUTest : public ::testing::Test {
 protected:
  EventDriven::EventScheduler scheduler;

  RVVVectorALU* alu = nullptr;

  void SetUp() override {
    // Create RVV ALU with 4 units, 128-bit vector length
    alu = new RVVVectorALU("RVV_ALU_0", scheduler, 1, 4, 128);
  }

  void TearDown() override { delete alu; }
};

/**
 * @brief Test latency parameters for different operation categories
 */
TEST_F(RVVVectorALUTest, LatencyParameters) {
  // Arithmetic operations: 2 cycles
  EXPECT_EQ(RVVVectorALU::getLatency(RVVCategory::ARITHMETIC), 2);

  // Shift operations: 2 cycles
  EXPECT_EQ(RVVVectorALU::getLatency(RVVCategory::SHIFT), 2);

  // Logical operations: 1 cycle (combinational)
  EXPECT_EQ(RVVVectorALU::getLatency(RVVCategory::LOGICAL), 1);

  // Mask operations: 2 cycles
  EXPECT_EQ(RVVVectorALU::getLatency(RVVCategory::MASK), 2);

  // Bit manipulation: 2 cycles
  EXPECT_EQ(RVVVectorALU::getLatency(RVVCategory::BITMANIP), 2);

  // Comparison: 1 cycle (combinational)
  EXPECT_EQ(RVVVectorALU::getLatency(RVVCategory::COMPARE), 1);

  // Memory ops: 4 cycles
  EXPECT_EQ(RVVVectorALU::getLatency(RVVCategory::MEMORY), 4);

  // Float ops: 5 cycles
  EXPECT_EQ(RVVVectorALU::getLatency(RVVCategory::FLOAT), 5);
}

/**
 * @brief Test execution unit availability tracking
 */
TEST_F(RVVVectorALUTest, ExecutionUnitTracking) {
  // Initially all 4 units should be available
  EXPECT_EQ(alu->getAvailableUnits(), 4);

  // Mark unit 0 as busy
  alu->markUnitBusy(0, 2);
  EXPECT_EQ(alu->getAvailableUnits(), 3);

  // Mark units 1 and 2 as busy
  alu->markUnitBusy(1, 1);
  alu->markUnitBusy(2, 3);
  EXPECT_EQ(alu->getAvailableUnits(), 1);

  // Mark all as busy
  alu->markUnitBusy(3, 2);
  EXPECT_EQ(alu->getAvailableUnits(), 0);

  // Free unit 0
  alu->markUnitFree(0);
  EXPECT_EQ(alu->getAvailableUnits(), 1);

  // Free all units
  alu->markUnitFree(1);
  alu->markUnitFree(2);
  alu->markUnitFree(3);
  EXPECT_EQ(alu->getAvailableUnits(), 4);
}

/**
 * @brief Test RVV ALU data packet creation and cloning
 */
TEST_F(RVVVectorALUTest, DataPacketCreation) {
  // Create VADD operation (32-bit elements, 128-bit vector)
  auto op = std::make_shared<RVVALUDataPacket>(1, 1, 2,  // rd=1, rs1=1, rs2=2
                                               32,       // 32-bit elements
                                               128,      // 128-bit vector
                                               RVVCategory::ARITHMETIC);

  EXPECT_EQ(op->rd, 1);
  EXPECT_EQ(op->rs1, 1);
  EXPECT_EQ(op->rs2, 2);
  EXPECT_EQ(op->eew, 32);
  EXPECT_EQ(op->vlen, 128);
  EXPECT_EQ(op->category, RVVCategory::ARITHMETIC);

  // Test cloning
  auto cloned = std::dynamic_pointer_cast<RVVALUDataPacket>(op->clone());
  EXPECT_NE(cloned, nullptr);
  EXPECT_EQ(cloned->rd, 1);
  EXPECT_EQ(cloned->eew, 32);
  EXPECT_EQ(cloned->vlen, 128);
}

/**
 * @brief Test result packet creation
 */
TEST_F(RVVVectorALUTest, ResultPacketCreation) {
  auto result = std::make_shared<Architecture::RVVALUResultPacket>(1, 32, 128);

  EXPECT_EQ(result->rd, 1);
  EXPECT_EQ(result->eew, 32);
  EXPECT_EQ(result->vlen, 128);
  EXPECT_EQ(result->result_data.size(), 16);  // 128 bits / 8 bytes

  // Test cloning
  auto cloned = std::dynamic_pointer_cast<Architecture::RVVALUResultPacket>(
      result->clone());
  EXPECT_NE(cloned, nullptr);
  EXPECT_EQ(cloned->rd, 1);
  EXPECT_EQ(cloned->eew, 32);
}

/**
 * @brief Test all operation categories
 */
TEST_F(RVVVectorALUTest, OperationCategories) {
  // Test VADD (ARITHMETIC)
  {
    auto op = std::make_shared<RVVALUDataPacket>(1, 1, 2, 32, 128,
                                                 RVVCategory::ARITHMETIC);
    EXPECT_EQ(RVVVectorALU::getLatency(op->category), 2);
  }

  // Test VSLL (SHIFT)
  {
    auto op = std::make_shared<RVVALUDataPacket>(3, 1, 2, 16, 128,
                                                 RVVCategory::SHIFT);
    EXPECT_EQ(RVVVectorALU::getLatency(op->category), 2);
  }

  // Test VAND (LOGICAL)
  {
    auto op = std::make_shared<RVVALUDataPacket>(5, 1, 2, 64, 128,
                                                 RVVCategory::LOGICAL);
    EXPECT_EQ(RVVVectorALU::getLatency(op->category), 1);
  }

  // Test VMAND (MASK)
  {
    auto op =
        std::make_shared<RVVALUDataPacket>(0, 1, 2, 8, 128, RVVCategory::MASK);
    EXPECT_EQ(RVVVectorALU::getLatency(op->category), 2);
  }

  // Test VCPOP (BITMANIP)
  {
    auto op = std::make_shared<RVVALUDataPacket>(7, 0, 0, 8, 128,
                                                 RVVCategory::BITMANIP);
    EXPECT_EQ(RVVVectorALU::getLatency(op->category), 2);
  }

  // Test VMSEQ (COMPARE)
  {
    auto op = std::make_shared<RVVALUDataPacket>(0, 1, 2, 32, 128,
                                                 RVVCategory::COMPARE);
    EXPECT_EQ(RVVVectorALU::getLatency(op->category), 1);
  }
}

/**
 * @brief Test operation execution counter
 */
TEST_F(RVVVectorALUTest, OperationCounter) {
  EXPECT_EQ(alu->getOperationsExecuted(), 0);

  // Simulate operation executions
  // (In real usage, operations would be processed through pipeline)
  // For now, just verify counter initialization
}

/**
 * @brief Test different element widths
 */
TEST_F(RVVVectorALUTest, ElementWidths) {
  // 8-bit elements
  {
    auto op = std::make_shared<RVVALUDataPacket>(1, 1, 2, 8, 128,
                                                 RVVCategory::ARITHMETIC);
    EXPECT_EQ(op->eew, 8);
    EXPECT_EQ(op->operand_a.size(), 16);  // 128 bits / 8 bits per element
  }

  // 16-bit elements
  {
    auto op = std::make_shared<RVVALUDataPacket>(1, 1, 2, 16, 128,
                                                 RVVCategory::ARITHMETIC);
    EXPECT_EQ(op->eew, 16);
  }

  // 32-bit elements
  {
    auto op = std::make_shared<RVVALUDataPacket>(1, 1, 2, 32, 128,
                                                 RVVCategory::ARITHMETIC);
    EXPECT_EQ(op->eew, 32);
  }

  // 64-bit elements
  {
    auto op = std::make_shared<RVVALUDataPacket>(1, 1, 2, 64, 128,
                                                 RVVCategory::ARITHMETIC);
    EXPECT_EQ(op->eew, 64);
  }
}

/**
 * @brief Test different vector lengths
 */
TEST_F(RVVVectorALUTest, VectorLengths) {
  // 128-bit vectors
  {
    auto op = std::make_shared<RVVALUDataPacket>(1, 1, 2, 32, 128,
                                                 RVVCategory::ARITHMETIC);
    EXPECT_EQ(op->vlen, 128);
  }

  // 256-bit vectors
  {
    auto op = std::make_shared<RVVALUDataPacket>(1, 1, 2, 32, 256,
                                                 RVVCategory::ARITHMETIC);
    EXPECT_EQ(op->vlen, 256);
  }

  // 512-bit vectors
  {
    auto op = std::make_shared<RVVALUDataPacket>(1, 1, 2, 32, 512,
                                                 RVVCategory::ARITHMETIC);
    EXPECT_EQ(op->vlen, 512);
  }
}

/**
 * @brief Test fast logical operations (1 cycle latency)
 */
TEST_F(RVVVectorALUTest, FastLogicalOperations) {
  std::vector<RVVCategory> fast_ops = {
      RVVCategory::LOGICAL,  // Combinational
      RVVCategory::COMPARE   // Combinational
  };

  for (auto category : fast_ops) {
    EXPECT_EQ(RVVVectorALU::getLatency(category), 1)
        << "Category should have 1-cycle latency";
  }
}

/**
 * @brief Test multi-cycle operations
 */
TEST_F(RVVVectorALUTest, MultiCycleOperations) {
  std::vector<RVVCategory> multi_ops = {
      RVVCategory::ARITHMETIC,  // 2 cycles
      RVVCategory::SHIFT,       // 2 cycles
      RVVCategory::MASK,        // 2 cycles
      RVVCategory::BITMANIP     // 2 cycles
  };

  for (auto category : multi_ops) {
    EXPECT_EQ(RVVVectorALU::getLatency(category), 2)
        << "Category should have 2-cycle latency";
  }
}

/**
 * @brief Test parallel unit configuration
 */
TEST_F(RVVVectorALUTest, ParallelUnitConfiguration) {
  // Create ALU with different numbers of units
  RVVVectorALU alu_2("RVV_ALU_2", scheduler, 1, 2, 128);
  EXPECT_EQ(alu_2.getAvailableUnits(), 2);

  RVVVectorALU alu_4("RVV_ALU_4", scheduler, 1, 4, 128);
  EXPECT_EQ(alu_4.getAvailableUnits(), 4);

  RVVVectorALU alu_8("RVV_ALU_8", scheduler, 1, 8, 128);
  EXPECT_EQ(alu_8.getAvailableUnits(), 8);
}

/**
 * @brief Test execution unit busy state transitions
 */
TEST_F(RVVVectorALUTest, UnitBusyStateTransitions) {
  // All units initially free
  EXPECT_EQ(alu->getAvailableUnits(), 4);

  // Make all units busy
  for (size_t i = 0; i < 4; ++i) {
    alu->markUnitBusy(i, 2);
  }
  EXPECT_EQ(alu->getAvailableUnits(), 0);

  // Free units one by one
  for (size_t i = 0; i < 4; ++i) {
    alu->markUnitFree(i);
    EXPECT_EQ(alu->getAvailableUnits(), i + 1);
  }
}

/**
 * @brief Test register indexing
 */
TEST_F(RVVVectorALUTest, RegisterIndexing) {
  // Test valid register indices (v0-v31 in RVV)
  for (uint32_t i = 0; i < 32; ++i) {
    auto op = std::make_shared<RVVALUDataPacket>(
        i, (i + 1) % 32, (i + 2) % 32, 32, 128, RVVCategory::ARITHMETIC);
    EXPECT_EQ(op->rd, i);
  }
}

/**
 * @brief Test instruction categories match expected latencies
 */
TEST_F(RVVVectorALUTest, LatencyCategoryMapping) {
  // Map of operation to expected latency
  std::map<RVVCategory, uint64_t> expected_latencies = {
      {RVVCategory::ARITHMETIC, 2}, {RVVCategory::SHIFT, 2},
      {RVVCategory::LOGICAL, 1},    {RVVCategory::MASK, 2},
      {RVVCategory::BITMANIP, 2},   {RVVCategory::COMPARE, 1},
      {RVVCategory::MEMORY, 4},     {RVVCategory::FLOAT, 5}};

  for (const auto& [category, expected_latency] : expected_latencies) {
    EXPECT_EQ(RVVVectorALU::getLatency(category), expected_latency);
  }
}
