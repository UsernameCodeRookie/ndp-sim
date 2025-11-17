#include <gtest/gtest.h>

#include <map>
#include <memory>

#include "comp/rvv/rvv_dvu.h"
#include "tick.h"

/**
 * @brief Test suite for RVV Vector Division Unit
 *
 * Tests functional model and latency parameters
 */
class RVVVectorDVUTest : public ::testing::Test {
 protected:
  EventDriven::EventScheduler scheduler;

  RVVVectorDVU* dvu = nullptr;

  void SetUp() override {
    // Create RVV DVU with 128-bit vector length
    dvu = new RVVVectorDVU("RVV_DVU_0", scheduler, 1, 128);
  }

  void TearDown() override { delete dvu; }
};

/**
 * @brief Test latency parameters for different element widths
 */
TEST_F(RVVVectorDVUTest, LatencyParameters) {
  // 8-bit division: 17 cycles (worst case)
  EXPECT_EQ(RVVVectorDVU::getLatency(8), 17);

  // 16-bit division: 33 cycles (worst case)
  EXPECT_EQ(RVVVectorDVU::getLatency(16), 33);

  // 32-bit division: 65 cycles (worst case)
  EXPECT_EQ(RVVVectorDVU::getLatency(32), 65);

  // 64-bit division: 129 cycles (worst case)
  EXPECT_EQ(RVVVectorDVU::getLatency(64), 129);
}

/**
 * @brief Test division operation type checks
 */
TEST_F(RVVVectorDVUTest, OperationTypeChecks) {
  // Unsigned operations
  EXPECT_FALSE(RVVVectorDVU::isSigned(RVVDVUOp::VDIVU));
  EXPECT_FALSE(RVVVectorDVU::isSigned(RVVDVUOp::VREMU));

  // Signed operations
  EXPECT_TRUE(RVVVectorDVU::isSigned(RVVDVUOp::VDIV));
  EXPECT_TRUE(RVVVectorDVU::isSigned(RVVDVUOp::VREM));

  // Remainder vs Quotient
  EXPECT_FALSE(RVVVectorDVU::isRemainder(RVVDVUOp::VDIVU));
  EXPECT_FALSE(RVVVectorDVU::isRemainder(RVVDVUOp::VDIV));
  EXPECT_TRUE(RVVVectorDVU::isRemainder(RVVDVUOp::VREMU));
  EXPECT_TRUE(RVVVectorDVU::isRemainder(RVVDVUOp::VREM));
}

/**
 * @brief Test operation name retrieval
 */
TEST_F(RVVVectorDVUTest, OperationNames) {
  EXPECT_EQ(RVVVectorDVU::getOpName(RVVDVUOp::VDIVU), "VDIVU");
  EXPECT_EQ(RVVVectorDVU::getOpName(RVVDVUOp::VDIV), "VDIV");
  EXPECT_EQ(RVVVectorDVU::getOpName(RVVDVUOp::VREMU), "VREMU");
  EXPECT_EQ(RVVVectorDVU::getOpName(RVVDVUOp::VREM), "VREM");
  EXPECT_EQ(RVVVectorDVU::getOpName(RVVDVUOp::UNKNOWN), "UNKNOWN");
}

/**
 * @brief Test DVU data packet creation
 */
TEST_F(RVVVectorDVUTest, DataPacketCreation) {
  // Create VDIVU operation
  auto op = std::make_shared<RVVDVUDataPacket>(1, 2, 3,  // rd=1, rs1=2, rs2=3
                                               32,       // 32-bit elements
                                               128,      // 128-bit vector
                                               RVVDVUOp::VDIVU);

  EXPECT_EQ(op->rd, 1);
  EXPECT_EQ(op->rs1, 2);
  EXPECT_EQ(op->rs2, 3);
  EXPECT_EQ(op->eew, 32);
  EXPECT_EQ(op->vlen, 128);
  EXPECT_EQ(op->op, RVVDVUOp::VDIVU);

  // Test cloning
  auto cloned = std::dynamic_pointer_cast<RVVDVUDataPacket>(op->clone());
  EXPECT_NE(cloned, nullptr);
  EXPECT_EQ(cloned->rd, 1);
  EXPECT_EQ(cloned->op, RVVDVUOp::VDIVU);
}

/**
 * @brief Test result packet creation
 */
TEST_F(RVVVectorDVUTest, ResultPacketCreation) {
  // Create quotient result
  auto quotient =
      std::make_shared<Architecture::RVVDVUResultPacket>(1, 32, 128, false);
  EXPECT_EQ(quotient->rd, 1);
  EXPECT_EQ(quotient->eew, 32);
  EXPECT_FALSE(quotient->is_remainder);

  // Create remainder result
  auto remainder =
      std::make_shared<Architecture::RVVDVUResultPacket>(1, 32, 128, true);
  EXPECT_EQ(remainder->rd, 1);
  EXPECT_TRUE(remainder->is_remainder);

  // Test cloning
  auto cloned = std::dynamic_pointer_cast<Architecture::RVVDVUResultPacket>(
      quotient->clone());
  EXPECT_NE(cloned, nullptr);
  EXPECT_EQ(cloned->rd, 1);
  EXPECT_FALSE(cloned->is_remainder);
}

/**
 * @brief Test element width support
 */
TEST_F(RVVVectorDVUTest, ElementWidthSupport) {
  // 8-bit elements
  {
    auto op =
        std::make_shared<RVVDVUDataPacket>(1, 2, 3, 8, 128, RVVDVUOp::VDIVU);
    EXPECT_EQ(op->eew, 8);
    EXPECT_EQ(RVVVectorDVU::getLatency(op->eew), 17);
  }

  // 16-bit elements
  {
    auto op =
        std::make_shared<RVVDVUDataPacket>(1, 2, 3, 16, 128, RVVDVUOp::VDIV);
    EXPECT_EQ(op->eew, 16);
    EXPECT_EQ(RVVVectorDVU::getLatency(op->eew), 33);
  }

  // 32-bit elements
  {
    auto op =
        std::make_shared<RVVDVUDataPacket>(1, 2, 3, 32, 128, RVVDVUOp::VREMU);
    EXPECT_EQ(op->eew, 32);
    EXPECT_EQ(RVVVectorDVU::getLatency(op->eew), 65);
  }

  // 64-bit elements
  {
    auto op =
        std::make_shared<RVVDVUDataPacket>(1, 2, 3, 64, 128, RVVDVUOp::VREM);
    EXPECT_EQ(op->eew, 64);
    EXPECT_EQ(RVVVectorDVU::getLatency(op->eew), 129);
  }
}

/**
 * @brief Test vector lengths
 */
TEST_F(RVVVectorDVUTest, VectorLengths) {
  // 128-bit vectors
  {
    auto op =
        std::make_shared<RVVDVUDataPacket>(1, 2, 3, 32, 128, RVVDVUOp::VDIVU);
    EXPECT_EQ(op->vlen, 128);
  }

  // 256-bit vectors
  {
    auto op =
        std::make_shared<RVVDVUDataPacket>(1, 2, 3, 32, 256, RVVDVUOp::VDIV);
    EXPECT_EQ(op->vlen, 256);
  }

  // 512-bit vectors
  {
    auto op =
        std::make_shared<RVVDVUDataPacket>(1, 2, 3, 32, 512, RVVDVUOp::VREMU);
    EXPECT_EQ(op->vlen, 512);
  }
}

/**
 * @brief Test operation counter
 */
TEST_F(RVVVectorDVUTest, OperationCounter) {
  EXPECT_EQ(dvu->getOperationsExecuted(), 0);
  EXPECT_EQ(dvu->getDivisionByZeroCount(), 0);
}

/**
 * @brief Test all division operations
 */
TEST_F(RVVVectorDVUTest, AllDivisionOperations) {
  // VDIVU - unsigned divide
  {
    auto op =
        std::make_shared<RVVDVUDataPacket>(1, 2, 3, 32, 128, RVVDVUOp::VDIVU);
    EXPECT_FALSE(RVVVectorDVU::isSigned(op->op));
    EXPECT_FALSE(RVVVectorDVU::isRemainder(op->op));
    EXPECT_EQ(RVVVectorDVU::getOpName(op->op), "VDIVU");
  }

  // VDIV - signed divide
  {
    auto op =
        std::make_shared<RVVDVUDataPacket>(1, 2, 3, 32, 128, RVVDVUOp::VDIV);
    EXPECT_TRUE(RVVVectorDVU::isSigned(op->op));
    EXPECT_FALSE(RVVVectorDVU::isRemainder(op->op));
    EXPECT_EQ(RVVVectorDVU::getOpName(op->op), "VDIV");
  }

  // VREMU - unsigned remainder
  {
    auto op =
        std::make_shared<RVVDVUDataPacket>(1, 2, 3, 32, 128, RVVDVUOp::VREMU);
    EXPECT_FALSE(RVVVectorDVU::isSigned(op->op));
    EXPECT_TRUE(RVVVectorDVU::isRemainder(op->op));
    EXPECT_EQ(RVVVectorDVU::getOpName(op->op), "VREMU");
  }

  // VREM - signed remainder
  {
    auto op =
        std::make_shared<RVVDVUDataPacket>(1, 2, 3, 32, 128, RVVDVUOp::VREM);
    EXPECT_TRUE(RVVVectorDVU::isSigned(op->op));
    EXPECT_TRUE(RVVVectorDVU::isRemainder(op->op));
    EXPECT_EQ(RVVVectorDVU::getOpName(op->op), "VREM");
  }
}

/**
 * @brief Test register indexing
 */
TEST_F(RVVVectorDVUTest, RegisterIndexing) {
  // Test valid register indices (v0-v31 in RVV)
  for (uint32_t i = 0; i < 32; ++i) {
    auto op = std::make_shared<RVVDVUDataPacket>(i, (i + 1) % 32, (i + 2) % 32,
                                                 32, 128, RVVDVUOp::VDIVU);
    EXPECT_EQ(op->rd, i);
  }
}

/**
 * @brief Test division operation types for signed/unsigned combinations
 */
TEST_F(RVVVectorDVUTest, OperationSignedness) {
  std::map<RVVDVUOp, std::pair<bool, bool>> op_properties = {
      {RVVDVUOp::VDIVU, {false, false}},  // unsigned, quotient
      {RVVDVUOp::VDIV, {true, false}},    // signed, quotient
      {RVVDVUOp::VREMU, {false, true}},   // unsigned, remainder
      {RVVDVUOp::VREM, {true, true}}      // signed, remainder
  };

  for (const auto& [op, expected] : op_properties) {
    EXPECT_EQ(RVVVectorDVU::isSigned(op), expected.first)
        << "Operation " << RVVVectorDVU::getOpName(op)
        << " signedness mismatch";
    EXPECT_EQ(RVVVectorDVU::isRemainder(op), expected.second)
        << "Operation " << RVVVectorDVU::getOpName(op)
        << " remainder flag mismatch";
  }
}

/**
 * @brief Test different configurations
 */
TEST_F(RVVVectorDVUTest, DVUConfigurations) {
  // Create DVU with different vector lengths
  RVVVectorDVU dvu_128("DVU_128", scheduler, 1, 128);
  EXPECT_EQ(dvu_128.getOperationsExecuted(), 0);

  RVVVectorDVU dvu_256("DVU_256", scheduler, 1, 256);
  EXPECT_EQ(dvu_256.getOperationsExecuted(), 0);

  RVVVectorDVU dvu_512("DVU_512", scheduler, 1, 512);
  EXPECT_EQ(dvu_512.getOperationsExecuted(), 0);
}

/**
 * @brief Test element width to latency mapping
 */
TEST_F(RVVVectorDVUTest, LatencyMapping) {
  std::map<uint32_t, uint64_t> eew_to_latency = {
      {8, 17}, {16, 33}, {32, 65}, {64, 129}};

  for (const auto& [eew, expected_latency] : eew_to_latency) {
    EXPECT_EQ(RVVVectorDVU::getLatency(eew), expected_latency)
        << "Element width " << eew << " has incorrect latency";
  }
}

/**
 * @brief Test result packet variants
 */
TEST_F(RVVVectorDVUTest, ResultPacketVariants) {
  // Quotient packets
  for (uint32_t eew : {8, 16, 32, 64}) {
    auto quotient =
        std::make_shared<Architecture::RVVDVUResultPacket>(1, eew, 128, false);
    EXPECT_EQ(quotient->eew, eew);
    EXPECT_FALSE(quotient->is_remainder);
  }

  // Remainder packets
  for (uint32_t eew : {8, 16, 32, 64}) {
    auto remainder =
        std::make_shared<Architecture::RVVDVUResultPacket>(1, eew, 128, true);
    EXPECT_EQ(remainder->eew, eew);
    EXPECT_TRUE(remainder->is_remainder);
  }
}
