#include <gtest/gtest.h>

#include <cstdint>
#include <memory>
#include <vector>

#include "../src/comp/rvv/rvv_rob.h"

using namespace Architecture;

/**
 * @brief RVV Reorder Buffer Unit Tests
 */
class RVVROBTest : public ::testing::Test {
 protected:
  void SetUp() override {
    scheduler = std::make_unique<EventDriven::EventScheduler>();
    rob = std::make_unique<RVVReorderBuffer>("rob", *scheduler, 1, 64);
  }

  std::unique_ptr<EventDriven::EventScheduler> scheduler;
  std::unique_ptr<RVVReorderBuffer> rob;
};

TEST_F(RVVROBTest, EnqueueSingleEntry) {
  auto rob_idx = rob->enqueue(0, 1, 10, true, 0);
  EXPECT_EQ(rob_idx, 0);
  EXPECT_EQ(rob->getSize(), 1);
}

TEST_F(RVVROBTest, EnqueueMultipleEntries) {
  rob->enqueue(0, 1, 10, true, 0);
  rob->enqueue(0, 2, 11, true, 0);
  rob->enqueue(0, 3, 12, true, 0);
  EXPECT_EQ(rob->getSize(), 3);
}

TEST_F(RVVROBTest, EnqueueFull) {
  for (int i = 0; i < 64; i++) {
    auto idx = rob->enqueue(0, i, 10 + i, true, 0);
    EXPECT_EQ(idx, i);
  }
  auto full_idx = rob->enqueue(0, 64, 74, true, 0);
  EXPECT_EQ(full_idx, -1);
  EXPECT_EQ(rob->getSize(), 64);
}

TEST_F(RVVROBTest, MarkComplete) {
  auto rob_idx = rob->enqueue(0, 1, 10, true, 0);
  std::vector<uint8_t> result_data(16, 0xAA);
  std::vector<bool> byte_enable(64, true);
  std::vector<bool> vxsat(1, false);
  bool success = rob->markComplete(rob_idx, result_data, byte_enable, vxsat);
  EXPECT_TRUE(success);
}

TEST_F(RVVROBTest, GetRetireEntry) {
  auto rob_idx =
      rob->enqueue(10, 1, 5, true, 0);  // inst_id=10, uop_id=1, dest_reg=5
  std::vector<uint8_t> result_data(16, 0xBB);
  std::vector<bool> byte_enable(64, true);
  std::vector<bool> vxsat(1, false);
  rob->markComplete(rob_idx, result_data, byte_enable, vxsat);

  auto entry = rob->getRetireEntry();
  EXPECT_TRUE(entry != nullptr);
  EXPECT_EQ(entry->inst_id, 10);
  EXPECT_EQ(entry->dest_reg, 5);
  EXPECT_TRUE(entry->execution_complete);
}

TEST_F(RVVROBTest, GetRetireEntriesMultiple) {
  for (int i = 0; i < 3; i++) {
    auto rob_idx = rob->enqueue(i, i, 10 + i, true, 0);
    std::vector<uint8_t> result_data(16, 0xAA);
    std::vector<bool> byte_enable(64, true);
    std::vector<bool> vxsat(1, false);
    rob->markComplete(rob_idx, result_data, byte_enable, vxsat);
  }
  auto entries = rob->getRetireEntries(2);
  EXPECT_EQ(entries.size(), 2);
  EXPECT_EQ(entries[0].inst_id, 0);
  EXPECT_EQ(entries[1].inst_id, 1);
}

TEST_F(RVVROBTest, Retire) {
  auto rob_idx = rob->enqueue(0, 1, 10, true, 0);
  std::vector<uint8_t> result_data(16, 0xAA);
  std::vector<bool> byte_enable(64, true);
  std::vector<bool> vxsat(1, false);
  rob->markComplete(rob_idx, result_data, byte_enable, vxsat);

  auto entries = rob->getRetireEntries(1);
  EXPECT_EQ(entries.size(), 1);
  size_t retired = rob->retire(1);
  EXPECT_EQ(retired, 1);
  EXPECT_EQ(rob->getSize(), 0);
}

TEST_F(RVVROBTest, InOrderRetirement) {
  std::vector<int64_t> rob_indices;
  for (int i = 0; i < 3; i++) {
    auto idx = rob->enqueue(i, i, 10 + i, true, 0);
    rob_indices.push_back(idx);
  }

  for (int i = 2; i >= 0; i--) {
    std::vector<uint8_t> result_data(16, 0xAA);
    std::vector<bool> byte_enable(64, true);
    std::vector<bool> vxsat(1, false);
    rob->markComplete(rob_indices[i], result_data, byte_enable, vxsat);
  }

  auto entries = rob->getRetireEntries(3);
  EXPECT_EQ(entries.size(), 3);
  EXPECT_EQ(entries[0].inst_id, 0);
  EXPECT_EQ(entries[1].inst_id, 1);
  EXPECT_EQ(entries[2].inst_id, 2);
}

TEST_F(RVVROBTest, SetTrap) {
  auto rob_idx = rob->enqueue(0, 1, 10, true, 0);
  rob->setTrap(rob_idx, 42);

  std::vector<uint8_t> result_data(16, 0xAA);
  std::vector<bool> byte_enable(64, true);
  std::vector<bool> vxsat(1, false);
  rob->markComplete(rob_idx, result_data, byte_enable, vxsat);

  auto entry = rob->getRetireEntry();
  EXPECT_TRUE(entry != nullptr);
  EXPECT_TRUE(entry->trap_flag);
  EXPECT_EQ(entry->trap_code, 42);
}

TEST_F(RVVROBTest, ResultData) {
  auto rob_idx = rob->enqueue(0, 1, 10, true, 0);
  std::vector<uint8_t> result_data(16);
  for (int i = 0; i < 16; i++) {
    result_data[i] = i * 0x11;
  }
  std::vector<bool> byte_enable(64, true);
  std::vector<bool> vxsat(1, false);
  rob->markComplete(rob_idx, result_data, byte_enable, vxsat);

  auto entry = rob->getRetireEntry();
  EXPECT_TRUE(entry != nullptr);
  EXPECT_EQ(entry->result_data.size(), 16);
  for (int i = 0; i < 16; i++) {
    EXPECT_EQ(entry->result_data[i], i * 0x11);
  }
}

TEST_F(RVVROBTest, ByteEnable) {
  auto rob_idx = rob->enqueue(0, 1, 10, true, 0);
  std::vector<uint8_t> result_data(16, 0xAA);
  std::vector<bool> byte_enable(64, false);
  byte_enable[0] = true;
  byte_enable[2] = true;
  byte_enable[4] = true;
  std::vector<bool> vxsat(1, false);
  rob->markComplete(rob_idx, result_data, byte_enable, vxsat);

  auto entry = rob->getRetireEntry();
  EXPECT_TRUE(entry != nullptr);
  EXPECT_TRUE(entry->byte_enable[0]);
  EXPECT_FALSE(entry->byte_enable[1]);
  EXPECT_TRUE(entry->byte_enable[2]);
  EXPECT_FALSE(entry->byte_enable[3]);
  EXPECT_TRUE(entry->byte_enable[4]);
}

TEST_F(RVVROBTest, Statistics) {
  for (int i = 0; i < 5; i++) {
    rob->enqueue(0, i, 10 + i, true, 0);
  }
  EXPECT_EQ(rob->getDispatchedCount(), 5);

  for (int i = 0; i < 5; i++) {
    std::vector<uint8_t> result_data(16, 0xAA);
    std::vector<bool> byte_enable(64, true);
    std::vector<bool> vxsat(1, false);
    rob->markComplete(i, result_data, byte_enable, vxsat);
  }
  EXPECT_EQ(rob->getCompletedCount(), 5);

  rob->retire(5);
  EXPECT_EQ(rob->getRetiredCount(), 5);
}

TEST_F(RVVROBTest, EmptyFull) {
  EXPECT_TRUE(rob->isEmpty());
  rob->enqueue(0, 1, 10, true, 0);
  EXPECT_FALSE(rob->isEmpty());

  for (int i = 1; i < 64; i++) {
    rob->enqueue(0, i, 10 + i, true, 0);
  }
  EXPECT_TRUE(rob->isFull());
}

TEST_F(RVVROBTest, CircularWrap) {
  for (int cycle = 0; cycle < 2; cycle++) {
    for (int i = 0; i < 10; i++) {
      auto rob_idx = rob->enqueue(cycle, i, 10 + i, true, 0);
      std::vector<uint8_t> result_data(16, 0xAA);
      std::vector<bool> byte_enable(64, true);
      std::vector<bool> vxsat(1, false);
      if (rob_idx >= 0) {
        rob->markComplete(rob_idx, result_data, byte_enable, vxsat);
      }
    }
    auto entries = rob->getRetireEntries(10);
    EXPECT_EQ(entries.size(), 10);
    rob->retire(10);
    EXPECT_EQ(rob->getSize(), 0);
  }
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
