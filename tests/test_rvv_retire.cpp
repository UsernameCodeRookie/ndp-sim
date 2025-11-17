#include <gtest/gtest.h>

#include <cstdint>
#include <memory>
#include <vector>

#include "../src/comp/rvv/rvv_retire.h"

using namespace Architecture;

/**
 * @brief Test WAW Hazard Resolution - Two Writes
 */
class WAWResolveTest : public ::testing::Test {};

TEST_F(WAWResolveTest, ResolveTwoNone) {
  std::vector<bool> be0(64, false);  // First write disables all bytes
  std::vector<bool> be1(64, false);  // Second write disables all bytes

  auto result = WAWHazardResolver::resolveTwo(be0, be1);

  EXPECT_EQ(result.size(), 64);
  for (bool bit : result) {
    EXPECT_FALSE(bit);
  }
}

TEST_F(WAWResolveTest, ResolveTwoNoConflict) {
  std::vector<bool> be0(64, false);
  be0[0] = true;
  be0[1] = true;  // First 16 bytes

  std::vector<bool> be1(64, false);
  be1[2] = true;
  be1[3] = true;  // Second 16 bytes (different range)

  auto result = WAWHazardResolver::resolveTwo(be0, be1);

  // First write keeps bytes 0-1, loses nothing
  EXPECT_TRUE(result[0]);
  EXPECT_TRUE(result[1]);
  EXPECT_FALSE(result[2]);
  EXPECT_FALSE(result[3]);
}

TEST_F(WAWResolveTest, ResolveTwoFullConflict) {
  std::vector<bool> be0(64, true);  // First write all bytes
  std::vector<bool> be1(64, true);  // Second write all bytes

  auto result = WAWHazardResolver::resolveTwo(be0, be1);

  // Later write wins, first write loses all
  for (bool bit : result) {
    EXPECT_FALSE(bit);
  }
}

TEST_F(WAWResolveTest, ResolveTwoPartialConflict) {
  std::vector<bool> be0(64, true);  // First write all bytes
  std::vector<bool> be1(64, false);
  be1[10] = true;
  be1[11] = true;  // Second write bytes 10-11

  auto result = WAWHazardResolver::resolveTwo(be0, be1);

  // Bytes 10-11 should be masked out (later write wins)
  EXPECT_TRUE(result[0]);
  EXPECT_TRUE(result[9]);
  EXPECT_FALSE(result[10]);  // Masked by be1
  EXPECT_FALSE(result[11]);  // Masked by be1
  EXPECT_TRUE(result[12]);
  EXPECT_TRUE(result[63]);
}

/**
 * @brief Test WAW Resolution - Three Writes
 */
TEST_F(WAWResolveTest, ResolveThreeAllDifferent) {
  std::vector<bool> be0(64, false);
  be0[0] = true;
  be0[1] = true;

  std::vector<bool> be1(64, false);
  be1[2] = true;
  be1[3] = true;

  std::vector<bool> be2(64, false);
  be2[4] = true;
  be2[5] = true;

  std::vector<bool> out0, out1;
  WAWHazardResolver::resolveThree(be0, be1, be2, out0, out1);

  // No conflicts, all writes should be valid
  EXPECT_TRUE(out0[0]);
  EXPECT_TRUE(out0[1]);
  EXPECT_FALSE(out0[2]);  // Not in be0

  EXPECT_FALSE(out1[0]);  // Not in be1
  EXPECT_TRUE(out1[2]);
  EXPECT_TRUE(out1[3]);
}

TEST_F(WAWResolveTest, ResolveThreeCompleteConflict) {
  std::vector<bool> be0(64, true);
  std::vector<bool> be1(64, true);
  std::vector<bool> be2(64, true);

  std::vector<bool> out0, out1;
  WAWHazardResolver::resolveThree(be0, be1, be2, out0, out1);

  // be2 wins, so be1 loses all, and be0 loses to be1's effective range
  for (bool bit : out0) {
    EXPECT_FALSE(bit);
  }
  for (bool bit : out1) {
    EXPECT_FALSE(bit);
  }
}

/**
 * @brief Test WAW Resolution - Four Writes
 */
TEST_F(WAWResolveTest, ResolveFourAllDifferent) {
  std::vector<bool> be0(64, false);
  be0[0] = true;

  std::vector<bool> be1(64, false);
  be1[1] = true;

  std::vector<bool> be2(64, false);
  be2[2] = true;

  std::vector<bool> be3(64, false);
  be3[3] = true;

  std::vector<bool> out0, out1, out2;
  WAWHazardResolver::resolveFour(be0, be1, be2, be3, out0, out1, out2);

  // No conflicts, each write to different byte
  EXPECT_TRUE(out0[0]);
  EXPECT_FALSE(out0[1]);

  EXPECT_FALSE(out1[0]);
  EXPECT_TRUE(out1[1]);
  EXPECT_FALSE(out1[2]);

  EXPECT_FALSE(out2[0]);
  EXPECT_FALSE(out2[1]);
  EXPECT_TRUE(out2[2]);
}

/**
 * @brief RVV Retire Stage Tests
 */
class RVVRetireStageTest : public ::testing::Test {
 protected:
  void SetUp() override {
    scheduler = std::make_unique<EventDriven::EventScheduler>();
    retire = std::make_unique<RVVRetireStage>("retire", *scheduler, 128, 4);
  }

  std::unique_ptr<EventDriven::EventScheduler> scheduler;
  std::unique_ptr<RVVRetireStage> retire;

  ROBEntry createEntry(uint64_t rob_idx, uint32_t dest_reg, bool dest_valid,
                       uint8_t dest_type = 0) {
    ROBEntry entry;
    entry.rob_index = rob_idx;
    entry.dest_reg = dest_reg;
    entry.dest_valid = dest_valid;
    entry.dest_type = dest_type;
    entry.result_data = std::vector<uint8_t>(16, 0xAA);
    entry.byte_enable = std::vector<bool>(64, true);
    entry.execution_complete = true;
    entry.write_complete = true;
    entry.retired = false;
    entry.trap_flag = false;
    return entry;
  }
};

TEST_F(RVVRetireStageTest, ProcessSingleEntry) {
  std::vector<ROBEntry> entries;
  entries.push_back(createEntry(0, 1, true));

  auto writes = retire->processRetireEntries(entries);

  EXPECT_EQ(writes.size(), 1);
  EXPECT_EQ(writes[0].rob_index, 0);
  EXPECT_EQ(writes[0].dest_reg, 1);
  EXPECT_FALSE(writes[0].trap_flag);
}

TEST_F(RVVRetireStageTest, ProcessMultipleEntries) {
  std::vector<ROBEntry> entries;
  entries.push_back(createEntry(0, 1, true));
  entries.push_back(createEntry(1, 2, true));
  entries.push_back(createEntry(2, 3, true));

  auto writes = retire->processRetireEntries(entries);

  EXPECT_EQ(writes.size(), 3);
  EXPECT_EQ(retire->getWritesThisCycle(), 3);
}

TEST_F(RVVRetireStageTest, SkipInvalidDest) {
  std::vector<ROBEntry> entries;
  entries.push_back(createEntry(0, 1, true));   // Valid
  entries.push_back(createEntry(1, 2, false));  // Invalid
  entries.push_back(createEntry(2, 3, true));   // Valid

  auto writes = retire->processRetireEntries(entries);

  EXPECT_EQ(writes.size(), 2);
  EXPECT_EQ(writes[0].rob_index, 0);
  EXPECT_EQ(writes[1].rob_index, 2);
}

TEST_F(RVVRetireStageTest, WAWTwoWrites) {
  std::vector<ROBEntry> entries;
  auto entry0 = createEntry(0, 5, true);
  auto entry1 = createEntry(1, 5, true);  // Same register

  // entry0 writes bytes 0-7
  entry0.byte_enable = std::vector<bool>(64, false);
  for (int i = 0; i < 8; i++) {
    entry0.byte_enable[i] = true;
  }

  // entry1 writes bytes 8-15
  entry1.byte_enable = std::vector<bool>(64, false);
  for (int i = 8; i < 16; i++) {
    entry1.byte_enable[i] = true;
  }

  entries.push_back(entry0);
  entries.push_back(entry1);

  auto writes = retire->processRetireEntries(entries);

  EXPECT_EQ(writes.size(), 2);
  EXPECT_EQ(retire->getWAWCollisions(), 1);

  // Both should have their original byte enables since no conflict
  bool entry0_has_write = false;
  bool entry1_has_write = false;
  for (const auto& w : writes) {
    if (w.rob_index == 0) {
      entry0_has_write = true;
      EXPECT_TRUE(w.byte_enable[0]);
      EXPECT_FALSE(w.byte_enable[8]);
    }
    if (w.rob_index == 1) {
      entry1_has_write = true;
      EXPECT_FALSE(w.byte_enable[0]);
      EXPECT_TRUE(w.byte_enable[8]);
    }
  }
  EXPECT_TRUE(entry0_has_write);
  EXPECT_TRUE(entry1_has_write);
}

TEST_F(RVVRetireStageTest, WAWTwoWritesSameBytes) {
  std::vector<ROBEntry> entries;
  auto entry0 = createEntry(0, 5, true);
  auto entry1 = createEntry(1, 5, true);  // Same register

  // Both write all bytes - later write wins
  entry0.byte_enable = std::vector<bool>(64, true);
  entry1.byte_enable = std::vector<bool>(64, true);

  entries.push_back(entry0);
  entries.push_back(entry1);

  auto writes = retire->processRetireEntries(entries);

  EXPECT_EQ(writes.size(), 2);
  EXPECT_EQ(retire->getWAWCollisions(), 1);

  // entry0's bytes should all be masked
  EXPECT_EQ(writes[0].rob_index, 0);
  for (bool be : writes[0].byte_enable) {
    EXPECT_FALSE(be);
  }

  // entry1's bytes should all be enabled
  EXPECT_EQ(writes[1].rob_index, 1);
  for (bool be : writes[1].byte_enable) {
    EXPECT_TRUE(be);
  }
}

TEST_F(RVVRetireStageTest, WAWThreeWrites) {
  std::vector<ROBEntry> entries;
  auto entry0 = createEntry(0, 5, true);
  auto entry1 = createEntry(1, 5, true);
  auto entry2 = createEntry(2, 5, true);

  // Each writes different byte ranges
  entry0.byte_enable = std::vector<bool>(64, false);
  entry0.byte_enable[0] = true;

  entry1.byte_enable = std::vector<bool>(64, false);
  entry1.byte_enable[1] = true;

  entry2.byte_enable = std::vector<bool>(64, false);
  entry2.byte_enable[2] = true;

  entries.push_back(entry0);
  entries.push_back(entry1);
  entries.push_back(entry2);

  auto writes = retire->processRetireEntries(entries);

  EXPECT_EQ(writes.size(), 3);
  EXPECT_EQ(retire->getWAWCollisions(), 1);
}

TEST_F(RVVRetireStageTest, WAWFourWrites) {
  std::vector<ROBEntry> entries;
  for (int i = 0; i < 4; i++) {
    auto entry = createEntry(i, 5, true);
    entry.byte_enable = std::vector<bool>(64, false);
    entry.byte_enable[i] = true;
    entries.push_back(entry);
  }

  auto writes = retire->processRetireEntries(entries);

  EXPECT_EQ(writes.size(), 4);
  EXPECT_EQ(retire->getWAWCollisions(), 1);
}

TEST_F(RVVRetireStageTest, TrapFlagStopsRetirement) {
  std::vector<ROBEntry> entries;
  auto entry0 = createEntry(0, 1, true);  // No trap
  auto entry1 = createEntry(1, 1, true);  // Trap
  auto entry2 = createEntry(2, 1, true);  // No trap (should be skipped)

  entry1.trap_flag = true;  // Second entry to same register is trap

  entries.push_back(entry0);
  entries.push_back(entry1);
  entries.push_back(entry2);

  auto writes = retire->processRetireEntries(entries);

  // Only first 2 entries should be written (entry2 stopped by entry1 trap)
  EXPECT_EQ(writes.size(), 2);
  EXPECT_EQ(retire->getTrapsHandled(), 1);
}

TEST_F(RVVRetireStageTest, DifferentDestTypes) {
  std::vector<ROBEntry> entries;
  auto entry0 = createEntry(0, 1, true, 0);  // VRF write
  auto entry1 = createEntry(1, 2, true, 1);  // XRF write

  entries.push_back(entry0);
  entries.push_back(entry1);

  auto writes = retire->processRetireEntries(entries);

  EXPECT_EQ(writes.size(), 2);
  EXPECT_EQ(retire->getVRFWrites(), 1);
  EXPECT_EQ(retire->getXRFWrites(), 1);
}

TEST_F(RVVRetireStageTest, StatisticsReset) {
  std::vector<ROBEntry> entries;
  entries.push_back(createEntry(0, 1, true));

  retire->processRetireEntries(entries);
  EXPECT_EQ(retire->getWritesThisCycle(), 1);

  retire->resetStatistics();
  EXPECT_EQ(retire->getWritesThisCycle(), 0);
  EXPECT_EQ(retire->getVRFWrites(), 0);
  EXPECT_EQ(retire->getWAWCollisions(), 0);
}

TEST_F(RVVRetireStageTest, EmptyEntries) {
  std::vector<ROBEntry> entries;

  auto writes = retire->processRetireEntries(entries);

  EXPECT_EQ(writes.size(), 0);
  EXPECT_EQ(retire->getWritesThisCycle(), 0);
}

TEST_F(RVVRetireStageTest, NoValidDest) {
  std::vector<ROBEntry> entries;
  entries.push_back(createEntry(0, 1, false));
  entries.push_back(createEntry(1, 2, false));

  auto writes = retire->processRetireEntries(entries);

  EXPECT_EQ(writes.size(), 0);
}

TEST_F(RVVRetireStageTest, MultipleRegistersNoWAW) {
  std::vector<ROBEntry> entries;
  entries.push_back(createEntry(0, 1, true));
  entries.push_back(createEntry(1, 2, true));
  entries.push_back(createEntry(2, 3, true));
  entries.push_back(createEntry(3, 4, true));

  auto writes = retire->processRetireEntries(entries);

  EXPECT_EQ(writes.size(), 4);
  EXPECT_EQ(retire->getWAWCollisions(), 0);
}

TEST_F(RVVRetireStageTest, ComplexWAWMixed) {
  std::vector<ROBEntry> entries;

  // Register 1: 2 writes (WAW)
  entries.push_back(createEntry(0, 1, true));
  entries.push_back(createEntry(1, 1, true));

  // Register 2: 1 write (no WAW)
  entries.push_back(createEntry(2, 2, true));

  // Register 3: 3 writes (WAW)
  entries.push_back(createEntry(3, 3, true));
  entries.push_back(createEntry(4, 3, true));
  entries.push_back(createEntry(5, 3, true));

  auto writes = retire->processRetireEntries(entries);

  // Should have 2 (reg1) + 1 (reg2) + 3 (reg3) = 6 writes
  EXPECT_EQ(writes.size(), 6);
  EXPECT_EQ(retire->getWAWCollisions(), 2);  // One for reg1, one for reg3
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
