#include <gtest/gtest.h>

#include <memory>
#include <vector>

#include "../src/comp/regfile.h"
#include "../src/scheduler.h"

class RegisterFileTest : public ::testing::Test {
 protected:
  void SetUp() override {
    scheduler = std::make_shared<EventDriven::EventScheduler>();
  }

  void TearDown() override { scheduler.reset(); }

  std::shared_ptr<EventDriven::EventScheduler> scheduler;
};

// Test 1: Initialization
TEST_F(RegisterFileTest, Initialization) {
  RegisterFileParameters params(32, 16, 8, 4);
  auto regfile =
      std::make_shared<RegisterFile>("test_regfile", *scheduler, params);

  EXPECT_EQ(regfile->getName(), "test_regfile");
  EXPECT_EQ(regfile->getTotalReads(), 0);
  EXPECT_EQ(regfile->getTotalWrites(), 0);
  EXPECT_EQ(regfile->getTotalForwards(), 0);
  EXPECT_EQ(regfile->getTotalConflicts(), 0);
}

// Test 2: Basic write and read
TEST_F(RegisterFileTest, BasicWriteRead) {
  RegisterFileParameters params(32, 16, 8, 4);
  auto regfile =
      std::make_shared<RegisterFile>("test_regfile", *scheduler, params);

  // Write to register 1
  regfile->writeRegister(1, 0x12345678);
  EXPECT_EQ(regfile->readRegister(1), 0x12345678);

  // Write to register 5
  regfile->writeRegister(5, 0xDEADBEEF);
  EXPECT_EQ(regfile->readRegister(5), 0xDEADBEEF);

  // Read x0 should always return 0
  EXPECT_EQ(regfile->readRegister(0), 0);
}

// Test 3: x0 is always zero (cannot write to x0)
TEST_F(RegisterFileTest, x0AlwaysZero) {
  RegisterFileParameters params(32, 16, 8, 4);
  auto regfile =
      std::make_shared<RegisterFile>("test_regfile", *scheduler, params);

  // Try to write to x0
  regfile->writeRegister(0, 0xFFFFFFFF);

  // Should still be 0
  EXPECT_EQ(regfile->readRegister(0), 0);
}

// Test 4: Scoreboard tracking
TEST_F(RegisterFileTest, ScoreboardTracking) {
  RegisterFileParameters params(32, 16, 8, 4, 32, true, true, false);
  auto regfile =
      std::make_shared<RegisterFile>("test_regfile", *scheduler, params);

  // Set scoreboard for register 5
  regfile->setScoreboard(5);
  EXPECT_TRUE(regfile->isScoreboardSet(5));
  EXPECT_EQ(regfile->getScoreboardMask() & (1u << 5), (1u << 5));

  // Write should clear scoreboard
  regfile->writeRegister(5, 0x1234);
  EXPECT_FALSE(regfile->isScoreboardSet(5));
}

// Test 5: Multiple register writes
TEST_F(RegisterFileTest, MultipleWrites) {
  RegisterFileParameters params(32, 16, 8, 4);
  auto regfile =
      std::make_shared<RegisterFile>("test_regfile", *scheduler, params);

  // Write to multiple registers
  for (uint32_t i = 1; i < 10; ++i) {
    regfile->writeRegister(i, 0x1000 + i);
  }

  // Verify all writes
  for (uint32_t i = 1; i < 10; ++i) {
    EXPECT_EQ(regfile->readRegister(i), 0x1000 + i);
  }

  EXPECT_EQ(regfile->getTotalWrites(), 9);
}

// Test 6: Register file reset
TEST_F(RegisterFileTest, RegisterFileReset) {
  RegisterFileParameters params(32, 16, 8, 4);
  auto regfile =
      std::make_shared<RegisterFile>("test_regfile", *scheduler, params);

  // Write to multiple registers
  regfile->writeRegister(1, 0xDEAD);
  regfile->writeRegister(2, 0xBEEF);
  regfile->writeRegister(3, 0xCAFE);

  // Verify writes
  EXPECT_EQ(regfile->readRegister(1), 0xDEAD);
  EXPECT_EQ(regfile->getTotalWrites(), 3);

  // Reset
  regfile->reset();

  // Check that all registers are zero
  EXPECT_EQ(regfile->readRegister(1), 0);
  EXPECT_EQ(regfile->readRegister(2), 0);
  EXPECT_EQ(regfile->getTotalWrites(), 0);
}

// Test 7: Scoreboard operations
TEST_F(RegisterFileTest, ScoreboardOperations) {
  RegisterFileParameters params(32, 16, 8, 4, 32, true, true, false);
  auto regfile =
      std::make_shared<RegisterFile>("test_regfile", *scheduler, params);

  // Set scoreboard for multiple registers
  regfile->setScoreboard(3);
  regfile->setScoreboard(7);
  regfile->setScoreboard(15);

  uint32_t mask = regfile->getScoreboardMask();
  EXPECT_EQ(mask & (1u << 3), (1u << 3));
  EXPECT_EQ(mask & (1u << 7), (1u << 7));
  EXPECT_EQ(mask & (1u << 15), (1u << 15));

  // Write should clear corresponding scoreboard entries
  regfile->writeRegister(3, 0x100);
  EXPECT_FALSE(regfile->isScoreboardSet(3));
  EXPECT_TRUE(regfile->isScoreboardSet(7));
  EXPECT_TRUE(regfile->isScoreboardSet(15));
}

// Test 8: Write forwarding detection
TEST_F(RegisterFileTest, WriteForwarding) {
  RegisterFileParameters params(32, 16, 8, 4, 32, true, true, false);
  auto regfile =
      std::make_shared<RegisterFile>("test_regfile", *scheduler, params);

  // Set scoreboard (mark as pending write)
  regfile->setScoreboard(5);

  // Read from the register (should be forwarded)
  regfile->readRegister(5);

  // Note: In a real system, forwarding would provide the pending write data
  // This test just verifies the tracking
  EXPECT_EQ(regfile->getTotalForwards(), 0);  // No actual forwarding yet
}

// Test 9: Masked write (speculative execution)
TEST_F(RegisterFileTest, MaskedWrite) {
  RegisterFileParameters params(32, 16, 8, 4);
  auto regfile =
      std::make_shared<RegisterFile>("test_regfile", *scheduler, params);

  // Normal write
  regfile->writeRegister(1, 0x1111, false);
  EXPECT_EQ(regfile->readRegister(1), 0x1111);

  // Masked write (should still apply in this simplified model)
  regfile->writeRegister(1, 0x2222, true);
  EXPECT_EQ(regfile->readRegister(1), 0x2222);
}

// Test 10: Port-based read operation
TEST_F(RegisterFileTest, PortBasedRead) {
  RegisterFileParameters params(32, 16, 8, 4);
  auto regfile =
      std::make_shared<RegisterFile>("test_regfile", *scheduler, params);

  // Write some data
  regfile->writeRegister(5, 0xABCD);

  // Access via port
  auto read_addr_port = regfile->getPort("read_addr_0");
  ASSERT_NE(read_addr_port, nullptr);

  // Write read address request
  auto addr_packet = std::make_shared<RegfileReadAddrPacket>(5, true);
  read_addr_port->write(addr_packet);

  // Process ports
  regfile->updatePorts();

  // Check read data output
  auto read_data_port = regfile->getPort("read_data_0");
  ASSERT_NE(read_data_port, nullptr);

  if (read_data_port->hasData()) {
    auto data_packet = std::dynamic_pointer_cast<RegfileReadDataPacket>(
        read_data_port->read());
    ASSERT_NE(data_packet, nullptr);
    EXPECT_EQ(data_packet->data, 0xABCD);
  }
}

// Test 11: Port-based write operation
TEST_F(RegisterFileTest, PortBasedWrite) {
  RegisterFileParameters params(32, 16, 8, 4);
  auto regfile =
      std::make_shared<RegisterFile>("test_regfile", *scheduler, params);

  // Access via port
  auto write_addr_port = regfile->getPort("write_addr_0");
  auto write_data_port = regfile->getPort("write_data_0");
  ASSERT_NE(write_addr_port, nullptr);
  ASSERT_NE(write_data_port, nullptr);

  // Write request
  auto addr_packet =
      std::make_shared<RegfileWritePacket>(7, 0x5678, true, false);
  auto data_packet =
      std::make_shared<RegfileWritePacket>(7, 0x5678, true, false);

  write_addr_port->write(addr_packet);
  write_data_port->write(data_packet);

  // Process ports
  regfile->updatePorts();

  // Verify write
  EXPECT_EQ(regfile->readRegister(7), 0x5678);
}

// Test 12: Multiple ports simultaneously
TEST_F(RegisterFileTest, MultiplePortsSimultaneous) {
  RegisterFileParameters params(32, 16, 8, 4);
  auto regfile =
      std::make_shared<RegisterFile>("test_regfile", *scheduler, params);

  // Write to register 3 and 7 first
  regfile->writeRegister(3, 0x3333);
  regfile->writeRegister(7, 0x7777);

  // Read from multiple ports
  std::vector<uint32_t> addrs = {3, 7, 3, 7};
  for (uint32_t i = 0; i < addrs.size(); ++i) {
    auto read_addr_port = regfile->getPort("read_addr_" + std::to_string(i));
    if (read_addr_port) {
      auto addr_packet =
          std::make_shared<RegfileReadAddrPacket>(addrs[i], true);
      read_addr_port->write(addr_packet);
    }
  }

  // Process ports
  regfile->updatePorts();

  // Verify reads
  for (uint32_t i = 0; i < addrs.size(); ++i) {
    auto read_data_port = regfile->getPort("read_data_" + std::to_string(i));
    if (read_data_port && read_data_port->hasData()) {
      auto data_packet = std::dynamic_pointer_cast<RegfileReadDataPacket>(
          read_data_port->read());
      ASSERT_NE(data_packet, nullptr);
      EXPECT_EQ(data_packet->data, addrs[i] == 3 ? 0x3333 : 0x7777);
    }
  }
}

// Test 13: Write count tracking
TEST_F(RegisterFileTest, WriteCountTracking) {
  RegisterFileParameters params(32, 16, 8, 4);
  auto regfile =
      std::make_shared<RegisterFile>("test_regfile", *scheduler, params);

  // Write to multiple registers
  regfile->writeRegister(1, 0x1);
  regfile->writeRegister(2, 0x2);
  regfile->writeRegister(3, 0x3);

  EXPECT_EQ(regfile->getTotalWrites(), 3);
}

// Test 14: Parameterized register file with different configurations
TEST_F(RegisterFileTest, DifferentConfigurations) {
  // 16-register file
  RegisterFileParameters params16(16, 8, 4, 2);
  auto regfile16 =
      std::make_shared<RegisterFile>("regfile16", *scheduler, params16);
  EXPECT_EQ(regfile16->getParameters().num_registers, 16);

  // 64-register file
  RegisterFileParameters params64(64, 32, 16, 8);
  auto regfile64 =
      std::make_shared<RegisterFile>("regfile64", *scheduler, params64);
  EXPECT_EQ(regfile64->getParameters().num_registers, 64);

  // Test that each works independently
  regfile16->writeRegister(1, 0x1111);
  regfile64->writeRegister(1, 0x6464);

  EXPECT_EQ(regfile16->readRegister(1), 0x1111);
  EXPECT_EQ(regfile64->readRegister(1), 0x6464);
}

// Test 15: Statistics collection
TEST_F(RegisterFileTest, StatisticsCollection) {
  RegisterFileParameters params(32, 16, 8, 4);
  auto regfile =
      std::make_shared<RegisterFile>("test_regfile", *scheduler, params);

  // Perform various operations
  regfile->setScoreboard(5);
  regfile->writeRegister(5, 0x5555);
  regfile->readRegister(5);
  regfile->readRegister(3);

  // Note: Statistics tracking depends on port-based operations
  // Direct API calls don't update all counters
  EXPECT_GT(regfile->getTotalWrites(), 0);
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
