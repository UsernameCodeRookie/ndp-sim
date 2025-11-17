#include <gtest/gtest.h>

#include <memory>

#include "comp/rvv/rvv_regfile.h"

using namespace Architecture;

class RVVVectorRegisterFileTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Create regfile with 32 registers, 128-bit VLEN, 4 read/write ports
    scheduler_ = std::make_shared<EventDriven::EventScheduler>();
    regfile_ = std::make_unique<RVVVectorRegisterFile>("rvv_vrf", *scheduler_,
                                                       1000, 32, 128, 4, 4);
  }

  std::shared_ptr<EventDriven::EventScheduler> scheduler_;
  std::unique_ptr<RVVVectorRegisterFile> regfile_;
};

// Test 1: Register File Initialization
TEST_F(RVVVectorRegisterFileTest, Initialization) {
  EXPECT_EQ(regfile_->getNumRegisters(), 32);
  EXPECT_EQ(regfile_->getVectorLength(), 128);
  EXPECT_EQ(regfile_->getNumReadPorts(), 4);
  EXPECT_EQ(regfile_->getNumWritePorts(), 4);
  EXPECT_EQ(regfile_->getReadCount(), 0);
  EXPECT_EQ(regfile_->getWriteCount(), 0);
}

// Test 2: Write and Read Operations
TEST_F(RVVVectorRegisterFileTest, WriteAndRead) {
  std::vector<uint8_t> test_data(16, 0xAB);  // 128-bit = 16 bytes
  bool write_success = regfile_->write(5, test_data);
  EXPECT_TRUE(write_success);
  EXPECT_EQ(regfile_->getWriteCount(), 1);

  std::vector<uint8_t> read_data = regfile_->read(5);
  EXPECT_EQ(read_data, test_data);
  EXPECT_EQ(regfile_->getReadCount(), 1);
}

// Test 3: Write with Byte Enable
TEST_F(RVVVectorRegisterFileTest, WriteWithByteEnable) {
  std::vector<uint8_t> initial_data(16, 0x00);
  regfile_->write(7, initial_data);

  std::vector<uint8_t> update_data(16, 0xFF);
  std::vector<bool> byte_enable(16, false);
  byte_enable[0] = true;
  byte_enable[7] = true;
  byte_enable[15] = true;

  regfile_->write(7, update_data, byte_enable);
  std::vector<uint8_t> result = regfile_->read(7);

  EXPECT_EQ(result[0], 0xFF);
  EXPECT_EQ(result[1], 0x00);
  EXPECT_EQ(result[7], 0xFF);
  EXPECT_EQ(result[15], 0xFF);
}

// Test 4: All Registers Accessible
TEST_F(RVVVectorRegisterFileTest, AllRegistersAccessible) {
  std::vector<uint8_t> test_data(16);

  // Write unique data to each register
  for (size_t i = 0; i < 32; ++i) {
    for (size_t j = 0; j < 16; ++j) {
      test_data[j] = static_cast<uint8_t>(i + j);
    }
    bool success = regfile_->write(i, test_data);
    EXPECT_TRUE(success);
  }

  // Read and verify each register
  for (size_t i = 0; i < 32; ++i) {
    std::vector<uint8_t> read_data = regfile_->read(i);
    for (size_t j = 0; j < 16; ++j) {
      EXPECT_EQ(read_data[j], static_cast<uint8_t>(i + j));
    }
  }

  EXPECT_EQ(regfile_->getWriteCount(), 32);
  EXPECT_EQ(regfile_->getReadCount(), 32);
}

// Test 5: Mask Register (v0) Special Handling
TEST_F(RVVVectorRegisterFileTest, MaskRegister) {
  std::vector<uint8_t> mask_data(16, 0xAA);

  bool success = regfile_->setMaskRegister(mask_data);
  EXPECT_TRUE(success);

  std::vector<uint8_t> read_mask = regfile_->getMaskRegister();
  EXPECT_EQ(read_mask, mask_data);

  // Verify mask is actually v0
  std::vector<uint8_t> v0_read = regfile_->read(0);
  EXPECT_EQ(v0_read, mask_data);
}

// Test 6: Clear Single Register
TEST_F(RVVVectorRegisterFileTest, ClearSingleRegister) {
  std::vector<uint8_t> test_data(16, 0xFF);
  regfile_->write(10, test_data);

  bool clear_success = regfile_->clear(10);
  EXPECT_TRUE(clear_success);

  std::vector<uint8_t> result = regfile_->read(10);
  for (const auto& byte : result) {
    EXPECT_EQ(byte, 0);
  }
}

// Test 7: Clear All Registers
TEST_F(RVVVectorRegisterFileTest, ClearAllRegisters) {
  std::vector<uint8_t> test_data(16, 0xCC);

  // Fill all registers with non-zero data
  for (size_t i = 0; i < 32; ++i) {
    regfile_->write(i, test_data);
  }

  regfile_->clearAll();

  // Verify all registers are cleared
  for (size_t i = 0; i < 32; ++i) {
    std::vector<uint8_t> result = regfile_->read(i);
    for (const auto& byte : result) {
      EXPECT_EQ(byte, 0);
    }
  }
}

// Test 8: Is Non-Zero Check
TEST_F(RVVVectorRegisterFileTest, IsNonZeroCheck) {
  std::vector<uint8_t> zero_data(16, 0x00);
  std::vector<uint8_t> nonzero_data(16, 0x00);
  nonzero_data[8] = 0x42;

  regfile_->write(5, zero_data);
  regfile_->write(10, nonzero_data);

  EXPECT_FALSE(regfile_->isNonZero(5));
  EXPECT_TRUE(regfile_->isNonZero(10));
}

// Test 9: Invalid Register Index
TEST_F(RVVVectorRegisterFileTest, InvalidRegisterIndex) {
  std::vector<uint8_t> test_data(16, 0xFF);

  // Write to invalid index
  bool write_success = regfile_->write(32, test_data);
  EXPECT_FALSE(write_success);

  bool clear_success = regfile_->clear(32);
  EXPECT_FALSE(clear_success);

  // Read from invalid index returns zero
  std::vector<uint8_t> result = regfile_->read(32);
  for (const auto& byte : result) {
    EXPECT_EQ(byte, 0);
  }
}

// Test 10: Wrong Data Size
TEST_F(RVVVectorRegisterFileTest, WrongDataSize) {
  std::vector<uint8_t> wrong_size_data(8, 0xFF);  // Wrong size (should be 16)

  bool write_success = regfile_->write(5, wrong_size_data);
  EXPECT_FALSE(write_success);
}

// Test 11: Byte Enable Size Mismatch
TEST_F(RVVVectorRegisterFileTest, ByteEnableSizeMismatch) {
  std::vector<uint8_t> test_data(16, 0xFF);
  std::vector<bool> wrong_enable_size(8, true);  // Wrong size

  bool write_success = regfile_->write(5, test_data, wrong_enable_size);
  EXPECT_FALSE(write_success);
}

// Test 12: Multiple Vector Lengths
TEST_F(RVVVectorRegisterFileTest, MultipleVectorLengths) {
  // Test with different VLEN values
  auto regfile_256 = std::make_unique<RVVVectorRegisterFile>(
      "rvv_vrf_256", *scheduler_, 1000, 32, 256, 4, 4);
  EXPECT_EQ(regfile_256->getVectorLength(), 256);

  std::vector<uint8_t> data_256(32, 0xBB);  // 256-bit = 32 bytes
  bool write_success = regfile_256->write(0, data_256);
  EXPECT_TRUE(write_success);

  std::vector<uint8_t> result = regfile_256->read(0);
  EXPECT_EQ(result, data_256);

  auto regfile_512 = std::make_unique<RVVVectorRegisterFile>(
      "rvv_vrf_512", *scheduler_, 1000, 32, 512, 4, 4);
  EXPECT_EQ(regfile_512->getVectorLength(), 512);

  std::vector<uint8_t> data_512(64, 0xDD);  // 512-bit = 64 bytes
  write_success = regfile_512->write(0, data_512);
  EXPECT_TRUE(write_success);

  result = regfile_512->read(0);
  EXPECT_EQ(result, data_512);
}

// Test 13: Port Configuration
TEST_F(RVVVectorRegisterFileTest, PortConfiguration) {
  auto regfile_custom = std::make_unique<RVVVectorRegisterFile>(
      "rvv_vrf_custom", *scheduler_, 1000, 32, 128, 2, 2);

  EXPECT_EQ(regfile_custom->getNumReadPorts(), 2);
  EXPECT_EQ(regfile_custom->getNumWritePorts(), 2);
}

// Test 14: Statistics Reset
TEST_F(RVVVectorRegisterFileTest, StatisticsReset) {
  std::vector<uint8_t> test_data(16, 0x55);

  regfile_->write(0, test_data);
  regfile_->read(0);
  regfile_->write(1, test_data);
  regfile_->read(1);

  EXPECT_EQ(regfile_->getWriteCount(), 2);
  EXPECT_EQ(regfile_->getReadCount(), 2);

  regfile_->resetStatistics();
  EXPECT_EQ(regfile_->getWriteCount(), 0);
  EXPECT_EQ(regfile_->getReadCount(), 0);
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
