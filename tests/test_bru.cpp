#include <gtest/gtest.h>

#include <memory>
#include <vector>

#include "../src/comp/bru.h"
#include "../src/scheduler.h"

class BruTest : public ::testing::Test {
 protected:
  void SetUp() override {
    scheduler = std::make_shared<EventDriven::EventScheduler>();
  }

  void TearDown() override { scheduler.reset(); }

  std::shared_ptr<EventDriven::EventScheduler> scheduler;
};

// Test 1: BRU Initialization
TEST_F(BruTest, Initialization) {
  auto bru = std::make_shared<BruComponent>("test_bru", *scheduler, 1);
  bru->start();

  EXPECT_EQ(bru->getName(), "test_bru");
  EXPECT_EQ(bru->getBranchesResolved(), 0);
  EXPECT_EQ(bru->getBranchesTaken(), 0);
  EXPECT_EQ(bru->getSystemExceptions(), 0);
}

// Test 2: BEQ (Branch if equal) - condition true
TEST_F(BruTest, BranchEqual) {
  auto bru = std::make_shared<BruComponent>("test_bru", *scheduler, 1);
  bru->start();

  auto input_port = bru->getPort("in");
  auto output_port = bru->getPort("out");

  // BEQ: if (5 == 5), take branch to address 0x100
  auto cmd = std::make_shared<BruCommandPacket>(0x0, 0x100, BruOp::BEQ, 5, 5);
  input_port->write(cmd);

  // Execute 3 pipeline stages + 1 to output = 4 ticks
  for (int i = 0; i < 4; i++) {
    bru->tick();
  }

  EXPECT_TRUE(output_port->hasData());
  auto result = std::dynamic_pointer_cast<BruResultPacket>(output_port->read());
  ASSERT_NE(result, nullptr);
  EXPECT_TRUE(result->taken);
  EXPECT_EQ(result->target, 0x100);
}

// Test 3: BEQ (Branch if equal) - condition false
TEST_F(BruTest, BranchNotEqual) {
  auto bru = std::make_shared<BruComponent>("test_bru", *scheduler, 1);
  bru->start();

  auto input_port = bru->getPort("in");
  auto output_port = bru->getPort("out");

  // BEQ: if (5 == 3), don't take branch
  auto cmd = std::make_shared<BruCommandPacket>(0x0, 0x100, BruOp::BEQ, 5, 3);
  input_port->write(cmd);

  // Execute pipeline
  for (int i = 0; i < 4; i++) {
    bru->tick();
  }

  EXPECT_TRUE(output_port->hasData());
  auto result = std::dynamic_pointer_cast<BruResultPacket>(output_port->read());
  ASSERT_NE(result, nullptr);
  EXPECT_FALSE(result->taken);
}

// Test 4: BNE (Branch if not equal)
TEST_F(BruTest, BranchNotEqualOp) {
  auto bru = std::make_shared<BruComponent>("test_bru", *scheduler, 1);
  bru->start();

  auto input_port = bru->getPort("in");
  auto output_port = bru->getPort("out");

  // BNE: if (5 != 3), take branch to 0x200
  auto cmd = std::make_shared<BruCommandPacket>(0x0, 0x200, BruOp::BNE, 5, 3);
  input_port->write(cmd);

  for (int i = 0; i < 4; i++) {
    bru->tick();
  }

  EXPECT_TRUE(output_port->hasData());
  auto result = std::dynamic_pointer_cast<BruResultPacket>(output_port->read());
  ASSERT_NE(result, nullptr);
  EXPECT_TRUE(result->taken);
  EXPECT_EQ(result->target, 0x200);
}

// Test 5: BLT (Branch if less than, signed)
TEST_F(BruTest, BranchLessThanSigned) {
  auto bru = std::make_shared<BruComponent>("test_bru", *scheduler, 1);
  bru->start();

  auto input_port = bru->getPort("in");
  auto output_port = bru->getPort("out");

  // BLT: if (-5 < 3), take branch
  auto cmd = std::make_shared<BruCommandPacket>(
      0x0, 0x150, BruOp::BLT, 0xFFFFFFFB, 3);  // -5 as unsigned
  input_port->write(cmd);

  for (int i = 0; i < 4; i++) {
    bru->tick();
  }

  EXPECT_TRUE(output_port->hasData());
  auto result = std::dynamic_pointer_cast<BruResultPacket>(output_port->read());
  ASSERT_NE(result, nullptr);
  EXPECT_TRUE(result->taken);
}

// Test 6: BLTU (Branch if less than unsigned)
TEST_F(BruTest, BranchLessThanUnsigned) {
  auto bru = std::make_shared<BruComponent>("test_bru", *scheduler, 1);
  bru->start();

  auto input_port = bru->getPort("in");
  auto output_port = bru->getPort("out");

  // BLTU: if (5 < 10), take branch
  auto cmd = std::make_shared<BruCommandPacket>(0x0, 0x300, BruOp::BLTU, 5, 10);
  input_port->write(cmd);

  for (int i = 0; i < 4; i++) {
    bru->tick();
  }

  EXPECT_TRUE(output_port->hasData());
  auto result = std::dynamic_pointer_cast<BruResultPacket>(output_port->read());
  ASSERT_NE(result, nullptr);
  EXPECT_TRUE(result->taken);
}

// Test 7: JAL (Jump and Link)
TEST_F(BruTest, JumpAndLink) {
  auto bru = std::make_shared<BruComponent>("test_bru", *scheduler, 1);
  bru->start();

  auto input_port = bru->getPort("in");
  auto output_port = bru->getPort("out");

  // JAL: Unconditional jump to 0x400, link to register 1 (rd=1)
  auto cmd =
      std::make_shared<BruCommandPacket>(0x100, 0x400, BruOp::JAL, 0, 0, 1);
  input_port->write(cmd);

  for (int i = 0; i < 4; i++) {
    bru->tick();
  }

  EXPECT_TRUE(output_port->hasData());
  auto result = std::dynamic_pointer_cast<BruResultPacket>(output_port->read());
  ASSERT_NE(result, nullptr);
  EXPECT_TRUE(result->taken);
  EXPECT_EQ(result->target, 0x400);
  EXPECT_TRUE(result->link_valid);
  EXPECT_EQ(result->link_data, 0x104);  // PC + 4
}

// Test 8: JALR (Jump and Link Register)
TEST_F(BruTest, JumpAndLinkRegister) {
  auto bru = std::make_shared<BruComponent>("test_bru", *scheduler, 1);
  bru->start();

  auto input_port = bru->getPort("in");
  auto output_port = bru->getPort("out");

  // JALR: Jump to address in RS1 (0x500 with alignment), link to register 1
  auto cmd =
      std::make_shared<BruCommandPacket>(0x200, 0, BruOp::JALR, 0x501, 0, 1);
  input_port->write(cmd);

  for (int i = 0; i < 4; i++) {
    bru->tick();
  }

  EXPECT_TRUE(output_port->hasData());
  auto result = std::dynamic_pointer_cast<BruResultPacket>(output_port->read());
  ASSERT_NE(result, nullptr);
  EXPECT_TRUE(result->taken);
  EXPECT_EQ(result->target, 0x500);  // 0x501 & ~1 = 0x500
  EXPECT_TRUE(result->link_valid);
  EXPECT_EQ(result->link_data, 0x204);
}

// Test 9: Multiple branch operations
TEST_F(BruTest, MultipleBranches) {
  auto bru = std::make_shared<BruComponent>("test_bru", *scheduler, 1);
  bru->start();

  auto input_port = bru->getPort("in");
  auto output_port = bru->getPort("out");

  // Test 1: BEQ with condition true
  auto cmd1 = std::make_shared<BruCommandPacket>(0x0, 0x100, BruOp::BEQ, 7, 7);
  input_port->write(cmd1);
  for (int i = 0; i < 4; i++) {
    bru->tick();
  }
  auto result1 =
      std::dynamic_pointer_cast<BruResultPacket>(output_port->read());
  EXPECT_TRUE(result1->taken);

  // Test 2: BNE with condition true
  auto cmd2 =
      std::make_shared<BruCommandPacket>(0x100, 0x200, BruOp::BNE, 5, 3);
  input_port->write(cmd2);
  for (int i = 0; i < 4; i++) {
    bru->tick();
  }
  auto result2 =
      std::dynamic_pointer_cast<BruResultPacket>(output_port->read());
  EXPECT_TRUE(result2->taken);

  // Test 3: JAL
  auto cmd3 =
      std::make_shared<BruCommandPacket>(0x200, 0x300, BruOp::JAL, 0, 0, 1);
  input_port->write(cmd3);
  for (int i = 0; i < 4; i++) {
    bru->tick();
  }
  auto result3 =
      std::dynamic_pointer_cast<BruResultPacket>(output_port->read());
  EXPECT_TRUE(result3->taken);

  // Verify statistics
  EXPECT_EQ(bru->getBranchesResolved(), 3);
  EXPECT_EQ(bru->getBranchesTaken(), 3);
}

// Test 10: BGE (Branch if greater or equal, signed)
TEST_F(BruTest, BranchGreaterEqualSigned) {
  auto bru = std::make_shared<BruComponent>("test_bru", *scheduler, 1);
  bru->start();

  auto input_port = bru->getPort("in");
  auto output_port = bru->getPort("out");

  // BGE: if (10 >= 5), take branch
  auto cmd = std::make_shared<BruCommandPacket>(0x0, 0x250, BruOp::BGE, 10, 5);
  input_port->write(cmd);

  for (int i = 0; i < 4; i++) {
    bru->tick();
  }

  EXPECT_TRUE(output_port->hasData());
  auto result = std::dynamic_pointer_cast<BruResultPacket>(output_port->read());
  ASSERT_NE(result, nullptr);
  EXPECT_TRUE(result->taken);
}

// Test 11: BGEU (Branch if greater or equal unsigned)
TEST_F(BruTest, BranchGreaterEqualUnsigned) {
  auto bru = std::make_shared<BruComponent>("test_bru", *scheduler, 1);
  bru->start();

  auto input_port = bru->getPort("in");
  auto output_port = bru->getPort("out");

  // BGEU: if (10 >=u 10), take branch
  auto cmd =
      std::make_shared<BruCommandPacket>(0x0, 0x350, BruOp::BGEU, 10, 10);
  input_port->write(cmd);

  for (int i = 0; i < 4; i++) {
    bru->tick();
  }

  EXPECT_TRUE(output_port->hasData());
  auto result = std::dynamic_pointer_cast<BruResultPacket>(output_port->read());
  ASSERT_NE(result, nullptr);
  EXPECT_TRUE(result->taken);
}

// Test 12: System operations (ECALL)
TEST_F(BruTest, SystemOperations) {
  auto bru = std::make_shared<BruComponent>("test_bru", *scheduler, 1);
  bru->start();

  auto input_port = bru->getPort("in");
  auto output_port = bru->getPort("out");

  // ECALL: Environment call
  auto cmd = std::make_shared<BruCommandPacket>(0x100, 0x0, BruOp::ECALL);
  input_port->write(cmd);

  for (int i = 0; i < 4; i++) {
    bru->tick();
  }

  EXPECT_TRUE(output_port->hasData());
  auto result = std::dynamic_pointer_cast<BruResultPacket>(output_port->read());
  ASSERT_NE(result, nullptr);
  EXPECT_TRUE(result->taken);
  EXPECT_EQ(bru->getSystemExceptions(), 1);
}

// Test 13: Event-driven execution
TEST_F(BruTest, EventDrivenExecution) {
  EventDriven::Tracer::getInstance().initialize("test_bru_event_driven.log",
                                                true);

  auto bru = std::make_shared<BruComponent>("test_bru_event", *scheduler, 2);
  bru->start();

  auto input_port = bru->getPort("in");
  auto output_port = bru->getPort("out");

  int results_count = 0;

  // Schedule branch operations at different times
  scheduler->scheduleAt(0, [&](EventDriven::EventScheduler& sched) {
    auto cmd = std::make_shared<BruCommandPacket>(0x0, 0x100, BruOp::BEQ, 5, 5);
    input_port->write(cmd);
    EventDriven::Tracer::getInstance().traceEvent(sched.getCurrentTime(),
                                                  "Test", "BranchIn", "BEQ");
  });

  scheduler->scheduleAt(10, [&](EventDriven::EventScheduler& sched) {
    if (output_port->hasData()) {
      auto result =
          std::dynamic_pointer_cast<BruResultPacket>(output_port->read());
      if (result) {
        results_count++;
        EventDriven::Tracer::getInstance().traceEvent(
            sched.getCurrentTime(), "Test", "BranchOut", "Result ready");
      }
    }
  });

  // Run simulation
  scheduler->run(20);

  EXPECT_GT(results_count, 0);
  EventDriven::Tracer::getInstance().dump();
  EventDriven::Tracer::getInstance().setEnabled(false);
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
