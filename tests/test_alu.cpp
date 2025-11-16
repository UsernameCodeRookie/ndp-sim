#include <gtest/gtest.h>

#include <cstring>

#include "../src/comp/alu.h"
#include "../src/scheduler.h"
#include "../src/trace.h"

class ALUTest : public ::testing::Test {
 protected:
  void SetUp() override {
    scheduler = std::make_unique<EventDriven::EventScheduler>();
  }

  void TearDown() override { scheduler.reset(); }

  std::unique_ptr<EventDriven::EventScheduler> scheduler;
};

TEST_F(ALUTest, BasicAddition) {
  auto alu = std::make_shared<ALUComponent>("test_alu", *scheduler, 1);
  alu->start();

  // Test: 5 + 3 = 8
  int result = ALUComponent::executeOperation(5, 3, ALUOp::ADD);
  EXPECT_EQ(result, 8);
}

TEST_F(ALUTest, BasicSubtraction) {
  auto alu = std::make_shared<ALUComponent>("test_alu", *scheduler, 1);
  alu->start();

  // Test: 10 - 4 = 6
  int result = ALUComponent::executeOperation(10, 4, ALUOp::SUB);
  EXPECT_EQ(result, 6);
}

TEST_F(ALUTest, BasicMultiplication) {
  auto alu = std::make_shared<ALUComponent>("test_alu", *scheduler, 1);
  alu->start();

  // Test: 7 * 6 = 42
  int result = ALUComponent::executeOperation(7, 6, ALUOp::MUL);
  EXPECT_EQ(result, 42);
}

TEST_F(ALUTest, BasicDivision) {
  auto alu = std::make_shared<ALUComponent>("test_alu", *scheduler, 1);
  alu->start();

  // Test: 20 / 4 = 5
  int result = ALUComponent::executeOperation(20, 4, ALUOp::DIV);
  EXPECT_EQ(result, 5);
}

TEST_F(ALUTest, BitwiseOperations) {
  auto alu = std::make_shared<ALUComponent>("test_alu", *scheduler, 1);
  alu->start();

  // Test AND: 0b1100 & 0b1010 = 0b1000 (12 & 10 = 8)
  EXPECT_EQ(ALUComponent::executeOperation(12, 10, ALUOp::AND), 8);

  // Test OR: 0b1100 | 0b1010 = 0b1110 (12 | 10 = 14)
  EXPECT_EQ(ALUComponent::executeOperation(12, 10, ALUOp::OR), 14);

  // Test XOR: 0b1100 ^ 0b1010 = 0b0110 (12 ^ 10 = 6)
  EXPECT_EQ(ALUComponent::executeOperation(12, 10, ALUOp::XOR), 6);
}

TEST_F(ALUTest, ShiftOperations) {
  auto alu = std::make_shared<ALUComponent>("test_alu", *scheduler, 1);
  alu->start();

  // Test SLL: 4 << 2 = 16
  EXPECT_EQ(ALUComponent::executeOperation(4, 2, ALUOp::SLL), 16);

  // Test SRL: 16 >> 2 = 4
  EXPECT_EQ(ALUComponent::executeOperation(16, 2, ALUOp::SRL), 4);
}

TEST_F(ALUTest, ComparisonOperations) {
  auto alu = std::make_shared<ALUComponent>("test_alu", *scheduler, 1);
  alu->start();

  // Test SLT: 3 < 5 = 1 (true)
  EXPECT_EQ(ALUComponent::executeOperation(3, 5, ALUOp::SLT), 1);

  // Test SLT: 5 < 3 = 0 (false)
  EXPECT_EQ(ALUComponent::executeOperation(5, 3, ALUOp::SLT), 0);

  // Test MAX
  EXPECT_EQ(ALUComponent::executeOperation(3, 5, ALUOp::MAX), 5);
  EXPECT_EQ(ALUComponent::executeOperation(5, 3, ALUOp::MAX), 5);

  // Test MIN
  EXPECT_EQ(ALUComponent::executeOperation(3, 5, ALUOp::MIN), 3);
  EXPECT_EQ(ALUComponent::executeOperation(5, 3, ALUOp::MIN), 3);
}

TEST_F(ALUTest, ExtendedBitOps) {
  // Test bit manipulation operations from ZBB extension
  // Test CLZ: count leading zeros
  EXPECT_EQ(ALUComponent::executeOperation(0x00000001, 0, ALUOp::CLZ), 31);
  EXPECT_EQ(ALUComponent::executeOperation(0x80000000, 0, ALUOp::CLZ), 0);

  // Test CTZ: count trailing zeros
  EXPECT_EQ(ALUComponent::executeOperation(0x80000000, 0, ALUOp::CTZ), 31);
  EXPECT_EQ(ALUComponent::executeOperation(0x00000001, 0, ALUOp::CTZ), 0);

  // Test CPOP: count population (number of 1 bits)
  EXPECT_EQ(ALUComponent::executeOperation(0xFF, 0, ALUOp::CPOP), 8);
  EXPECT_EQ(ALUComponent::executeOperation(0x0, 0, ALUOp::CPOP), 0);

  // Test MAXU/MINU: unsigned comparisons
  EXPECT_EQ(ALUComponent::executeOperation(-1, 1, ALUOp::MAXU),
            -1);  // -1 as unsigned is large
  EXPECT_EQ(ALUComponent::executeOperation(-1, 1, ALUOp::MINU), 1);

  // Test SEXTB: sign extend byte
  EXPECT_EQ(ALUComponent::executeOperation(0x000000FF, 0, ALUOp::SEXTB),
            -1);  // 0xFF -> -1
  EXPECT_EQ(ALUComponent::executeOperation(0x0000007F, 0, ALUOp::SEXTB), 127);

  // Test ZEXTH: zero extend half-word
  EXPECT_EQ(ALUComponent::executeOperation(0xFFFF0000, 0, ALUOp::ZEXTH), 0);
  EXPECT_EQ(ALUComponent::executeOperation(0x0000FFFF, 0, ALUOp::ZEXTH),
            0xFFFF);
}

TEST_F(ALUTest, PipelineOperation) {
  auto alu = std::make_shared<ALUComponent>("test_alu", *scheduler, 1);
  alu->start();

  auto input_port = alu->getPort("in");
  auto output_port = alu->getPort("out");

  // Send operation through pipeline
  auto packet = std::make_shared<ALUDataPacket>(10, 5, ALUOp::ADD);
  input_port->write(packet);

  // Execute pipeline stages (3 stages + 1 to output = 4 ticks)
  for (int i = 0; i < 4; i++) {
    alu->tick();
  }

  // Check output
  ASSERT_TRUE(output_port->hasData());
  auto result_packet = std::dynamic_pointer_cast<Architecture::IntDataPacket>(
      output_port->read());
  ASSERT_NE(result_packet, nullptr);
  EXPECT_EQ(result_packet->getValue(), 15);
}

TEST_F(ALUTest, AccumulatorMACOperation) {
  auto alu = std::make_shared<ALUComponent>("test_alu", *scheduler, 1);
  alu->start();

  // Reset accumulator
  alu->resetAccumulator();
  EXPECT_EQ(alu->getAccumulator(), 0);

  // MAC: acc = 0 + (3 * 4) = 12
  alu->setAccumulator(0);
  auto input_port = alu->getPort("in");
  auto packet1 = std::make_shared<ALUDataPacket>(3, 4, ALUOp::MAC);
  input_port->write(packet1);

  for (int i = 0; i < 3; i++) {
    alu->tick();
  }

  EXPECT_EQ(alu->getAccumulator(), 12);

  // MAC: acc = 12 + (2 * 5) = 22
  auto packet2 = std::make_shared<ALUDataPacket>(2, 5, ALUOp::MAC);
  input_port->write(packet2);

  for (int i = 0; i < 3; i++) {
    alu->tick();
  }

  EXPECT_EQ(alu->getAccumulator(), 22);
}

// Event-driven execution mode tests
TEST_F(ALUTest, EventDrivenExecution) {
  // Enable tracing
  EventDriven::Tracer::getInstance().initialize("test_alu_event_driven.log",
                                                true);

  auto alu = std::make_shared<ALUComponent>("test_alu_ed", *scheduler,
                                            2);  // period=2
  alu->start();

  auto input_port = alu->getPort("in");
  auto output_port = alu->getPort("out");

  // Schedule input at time 0
  scheduler->scheduleAt(0, [&](EventDriven::EventScheduler& sched) {
    auto packet = std::make_shared<ALUDataPacket>(10, 3, ALUOp::ADD);
    input_port->write(packet);
    EventDriven::Tracer::getInstance().traceEvent(
        sched.getCurrentTime(), "Test", "InputWrite", "ADD 10+3");
  });

  // Run simulation
  scheduler->run(20);  // Run until time 20

  // Check output appeared after 4 ticks (8 time units with period=2)
  EXPECT_TRUE(output_port->hasData());
  auto result = std::dynamic_pointer_cast<Architecture::IntDataPacket>(
      output_port->read());
  ASSERT_NE(result, nullptr);
  EXPECT_EQ(result->getValue(), 13);

  // Print trace summary
  std::cout << "\n=== Event-Driven ALU Test Trace ===" << std::endl;
  std::cout << "Final time: " << scheduler->getCurrentTime() << std::endl;
  std::cout << "Events processed: " << scheduler->getTotalEventCount()
            << std::endl;

  EventDriven::Tracer::getInstance().dump();
  EventDriven::Tracer::getInstance().setEnabled(false);
}

TEST_F(ALUTest, EventDrivenMultipleOperations) {
  EventDriven::Tracer::getInstance().initialize("test_alu_multiple_ops.log",
                                                true);

  auto alu = std::make_shared<ALUComponent>("test_alu_multi", *scheduler,
                                            2);  // period=2
  alu->start();

  auto input_port = alu->getPort("in");
  auto output_port = alu->getPort("out");

  std::vector<int> results;

  // Schedule multiple operations at different times (spaced by 10 time units)
  // With period=2, ticks happen at t=0,2,4,6,8,10,12,...
  // Write at t=1, t=11, t=21 (between ticks, so they'll be seen on next tick)
  scheduler->scheduleAt(1, [&](EventDriven::EventScheduler& sched) {
    auto packet = std::make_shared<ALUDataPacket>(5, 3, ALUOp::ADD);
    input_port->write(packet);
    EventDriven::Tracer::getInstance().traceCompute(sched.getCurrentTime(),
                                                    "Test", "Op1", "5+3");
  });

  scheduler->scheduleAt(11, [&](EventDriven::EventScheduler& sched) {
    auto packet = std::make_shared<ALUDataPacket>(10, 2, ALUOp::MUL);
    input_port->write(packet);
    EventDriven::Tracer::getInstance().traceCompute(sched.getCurrentTime(),
                                                    "Test", "Op2", "10*2");
  });

  scheduler->scheduleAt(21, [&](EventDriven::EventScheduler& sched) {
    auto packet = std::make_shared<ALUDataPacket>(20, 4, ALUOp::DIV);
    input_port->write(packet);
    EventDriven::Tracer::getInstance().traceCompute(sched.getCurrentTime(),
                                                    "Test", "Op3", "20/4");
  });

  // Collect outputs as they become available
  // Results appear after 3 pipeline stages (6 time units with period=2)
  // So results at t=7, t=17, t=27
  scheduler->scheduleAt(9, [&](EventDriven::EventScheduler& sched) {
    if (output_port->hasData()) {
      auto result = std::dynamic_pointer_cast<Architecture::IntDataPacket>(
          output_port->read());
      if (result) results.push_back(result->getValue());
    }
  });

  scheduler->scheduleAt(19, [&](EventDriven::EventScheduler& sched) {
    if (output_port->hasData()) {
      auto result = std::dynamic_pointer_cast<Architecture::IntDataPacket>(
          output_port->read());
      if (result) results.push_back(result->getValue());
    }
  });

  scheduler->scheduleAt(29, [&](EventDriven::EventScheduler& sched) {
    if (output_port->hasData()) {
      auto result = std::dynamic_pointer_cast<Architecture::IntDataPacket>(
          output_port->read());
      if (result) results.push_back(result->getValue());
    }
  });

  // Run simulation
  scheduler->run(40);

  // Collect any remaining outputs
  std::cout << "\n=== Checking output port ===" << std::endl;
  std::cout << "Has data: " << (output_port->hasData() ? "yes" : "no")
            << std::endl;
  while (output_port->hasData()) {
    auto result = std::dynamic_pointer_cast<Architecture::IntDataPacket>(
        output_port->read());
    if (result) {
      std::cout << "Got result: " << result->getValue() << std::endl;
      results.push_back(result->getValue());
    }
  }
  std::cout << "Total results collected: " << results.size() << std::endl;

  // Verify results
  ASSERT_EQ(results.size(), 3);
  EXPECT_EQ(results[0], 8);   // 5+3
  EXPECT_EQ(results[1], 20);  // 10*2
  EXPECT_EQ(results[2], 5);   // 20/4

  std::cout << "\n=== Multiple Operations Test ===" << std::endl;
  std::cout << "Operations completed: " << results.size() << std::endl;
  std::cout << "Final time: " << scheduler->getCurrentTime() << std::endl;

  EventDriven::Tracer::getInstance().dump();
  EventDriven::Tracer::getInstance().setEnabled(false);
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
