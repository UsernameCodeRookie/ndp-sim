#include <gtest/gtest.h>

#include "../src/components/pe.h"
#include "../src/scheduler.h"
#include "../src/trace.h"

class PETest : public ::testing::Test {
 protected:
  void SetUp() override {
    scheduler = std::make_unique<EventDriven::EventScheduler>();
  }

  void TearDown() override { scheduler.reset(); }

  std::unique_ptr<EventDriven::EventScheduler> scheduler;
};

TEST_F(PETest, RegisterReadWrite) {
  auto pe =
      std::make_shared<ProcessingElement>("test_pe", *scheduler, 1, 32, 4);
  pe->start();

  // Test register write and read
  pe->writeRegister(0, 42);
  EXPECT_EQ(pe->readRegister(0), 42);

  pe->writeRegister(5, 100);
  EXPECT_EQ(pe->readRegister(5), 100);

  // Test invalid address (should return 0)
  EXPECT_EQ(pe->readRegister(100), 0);
}

TEST_F(PETest, RegisterInitialization) {
  auto pe =
      std::make_shared<ProcessingElement>("test_pe", *scheduler, 1, 32, 4);
  pe->start();

  // Initialize registers
  pe->initRegister(0, 10);
  pe->initRegister(1, 20);
  pe->initRegister(2, 30);

  EXPECT_EQ(pe->readRegister(0), 10);
  EXPECT_EQ(pe->readRegister(1), 20);
  EXPECT_EQ(pe->readRegister(2), 30);
}

TEST_F(PETest, BasicAddOperation) {
  auto pe =
      std::make_shared<ProcessingElement>("test_pe", *scheduler, 1, 32, 4);
  pe->start();

  // Initialize source registers: R0 = 5, R1 = 3
  pe->initRegister(0, 5);
  pe->initRegister(1, 3);

  // Send ADD instruction: R2 = R0 + R1
  auto inst_in = pe->getPort("inst_in");
  auto inst = std::make_shared<PEInstructionPacket>(ALUOp::ADD, 0, 1, 2);
  inst_in->write(inst);

  // Execute
  pe->tick();

  // Check result in R2
  EXPECT_EQ(pe->readRegister(2), 8);
}

TEST_F(PETest, BasicMulOperation) {
  auto pe =
      std::make_shared<ProcessingElement>("test_pe", *scheduler, 1, 32, 4);
  pe->start();

  // Initialize source registers: R0 = 6, R1 = 7
  pe->initRegister(0, 6);
  pe->initRegister(1, 7);

  // Send MUL instruction: R2 = R0 * R1
  auto inst_in = pe->getPort("inst_in");
  auto inst = std::make_shared<PEInstructionPacket>(ALUOp::MUL, 0, 1, 2);
  inst_in->write(inst);

  // Execute
  pe->tick();

  // Check result in R2
  EXPECT_EQ(pe->readRegister(2), 42);
}

TEST_F(PETest, MACOperation) {
  auto pe =
      std::make_shared<ProcessingElement>("test_pe", *scheduler, 1, 32, 4);
  pe->start();

  // Reset MAC accumulator
  pe->resetMACAccumulator();
  EXPECT_EQ(pe->getMACAccumulator(), 0);

  // First MAC: acc = 0 + (3 * 4) = 12
  pe->initRegister(0, 3);
  pe->initRegister(1, 4);
  auto inst_in = pe->getPort("inst_in");
  auto inst1 = std::make_shared<PEInstructionPacket>(ALUOp::MAC, 0, 1, 2);
  inst_in->write(inst1);
  pe->tick();

  EXPECT_EQ(pe->getMACAccumulator(), 12);
  EXPECT_EQ(pe->readRegister(2), 12);

  // Second MAC: acc = 12 + (2 * 5) = 22
  pe->initRegister(3, 2);
  pe->initRegister(4, 5);
  auto inst2 = std::make_shared<PEInstructionPacket>(ALUOp::MAC, 3, 4, 5);
  inst_in->write(inst2);
  pe->tick();

  EXPECT_EQ(pe->getMACAccumulator(), 22);
  EXPECT_EQ(pe->readRegister(5), 22);
}

TEST_F(PETest, InstructionQueue) {
  auto pe =
      std::make_shared<ProcessingElement>("test_pe", *scheduler, 1, 32, 4);
  pe->start();

  pe->initRegister(0, 10);
  pe->initRegister(1, 5);
  pe->initRegister(2, 3);
  pe->initRegister(3, 2);

  auto inst_in = pe->getPort("inst_in");

  // Enqueue multiple instructions
  auto inst1 =
      std::make_shared<PEInstructionPacket>(ALUOp::ADD, 0, 1, 4);  // R4 = 15
  auto inst2 =
      std::make_shared<PEInstructionPacket>(ALUOp::MUL, 2, 3, 5);  // R5 = 6

  inst_in->write(inst1);
  pe->tick();
  EXPECT_EQ(pe->readRegister(4), 15);

  inst_in->write(inst2);
  pe->tick();
  EXPECT_EQ(pe->readRegister(5), 6);
}

TEST_F(PETest, ReadySignal) {
  auto pe = std::make_shared<ProcessingElement>("test_pe", *scheduler, 1, 32,
                                                2);  // Small queue
  pe->start();

  // PE should be ready initially
  EXPECT_TRUE(pe->isReady());

  // Fill the queue
  auto inst_in = pe->getPort("inst_in");
  auto inst1 = std::make_shared<PEInstructionPacket>(ALUOp::ADD, 0, 1, 2);
  auto inst2 = std::make_shared<PEInstructionPacket>(ALUOp::ADD, 0, 1, 2);

  inst_in->write(inst1);
  pe->tick();

  inst_in->write(inst2);
  pe->tick();

  // Queue should be full now (depth=2), check ready signal
  auto ready_port = pe->getPort("ready");
  ASSERT_TRUE(ready_port->hasData());
}

TEST_F(PETest, ExternalDataInput) {
  auto pe =
      std::make_shared<ProcessingElement>("test_pe", *scheduler, 1, 32, 4);
  pe->start();

  // Write to register via data_in port
  auto data_in = pe->getPort("data_in");
  auto reg_packet =
      std::make_shared<RegisterPacket>(5, 99, true);  // Write 99 to R5
  data_in->write(reg_packet);

  // Execute tick to process data input
  pe->tick();

  // Check if register was written
  EXPECT_EQ(pe->readRegister(5), 99);
}

TEST_F(PETest, DataOutput) {
  auto pe =
      std::make_shared<ProcessingElement>("test_pe", *scheduler, 1, 32, 4);
  pe->start();

  pe->initRegister(0, 100);
  pe->initRegister(1, 50);

  auto inst_in = pe->getPort("inst_in");
  auto data_out = pe->getPort("data_out");

  // Execute SUB instruction
  auto inst = std::make_shared<PEInstructionPacket>(ALUOp::SUB, 0, 1, 2);
  inst_in->write(inst);
  pe->tick();

  // Check data output
  ASSERT_TRUE(data_out->hasData());
  auto result = std::dynamic_pointer_cast<IntDataPacket>(data_out->read());
  ASSERT_NE(result, nullptr);
  EXPECT_EQ(result->getValue(), 50);
}

TEST_F(PETest, MACAccumulatorManagement) {
  auto pe =
      std::make_shared<ProcessingElement>("test_pe", *scheduler, 1, 32, 4);
  pe->start();

  // Set accumulator
  pe->setMACAccumulator(100);
  EXPECT_EQ(pe->getMACAccumulator(), 100);

  // Reset accumulator
  pe->resetMACAccumulator();
  EXPECT_EQ(pe->getMACAccumulator(), 0);
}

// Event-driven execution mode tests
TEST_F(PETest, EventDrivenMACOperation) {
  EventDriven::Tracer::getInstance().initialize("test_pe_event_driven.log",
                                                true);

  auto pe =
      std::make_shared<ProcessingElement>("test_pe_ed", *scheduler, 2, 32, 4);
  pe->start();

  auto inst_in = pe->getPort("inst_in");
  auto data_out = pe->getPort("data_out");

  // Initialize register file
  pe->writeRegister(0, 5);
  pe->writeRegister(1, 3);

  // Schedule MAC instruction at time 0
  scheduler->scheduleAt(0, [&](EventDriven::EventScheduler& sched) {
    auto inst = std::make_shared<PEInstructionPacket>(ALUOp::MAC, 0, 1, 2);
    inst_in->write(inst);
    EventDriven::Tracer::getInstance().traceInstruction(
        sched.getCurrentTime(), "Test", "MAC_Inst", "R0*R1->Acc");
  });

  // Run simulation
  scheduler->run(30);

  // Verify MAC result (5 * 3 = 15)
  EXPECT_EQ(pe->getMACAccumulator(), 15);

  std::cout << "\n=== Event-Driven PE MAC Test ===" << std::endl;
  std::cout << "MAC Accumulator: " << pe->getMACAccumulator() << std::endl;
  std::cout << "Final time: " << scheduler->getCurrentTime() << std::endl;

  EventDriven::Tracer::getInstance().dump();
  EventDriven::Tracer::getInstance().setEnabled(false);
}

TEST_F(PETest, EventDrivenInstructionStream) {
  EventDriven::Tracer::getInstance().initialize(
      "test_pe_instruction_stream.log", true);

  auto pe = std::make_shared<ProcessingElement>("test_pe_stream", *scheduler, 1,
                                                32, 8);
  pe->start();

  auto inst_in = pe->getPort("inst_in");
  auto data_out = pe->getPort("data_out");
  auto ready_out = pe->getPort("ready");

  // Initialize registers
  pe->writeRegister(0, 10);
  pe->writeRegister(1, 5);
  pe->writeRegister(2, 3);

  // Schedule multiple instructions
  scheduler->scheduleAt(0, [&](EventDriven::EventScheduler& sched) {
    auto inst1 = std::make_shared<PEInstructionPacket>(ALUOp::ADD, 0, 1,
                                                       3);  // R3 = R0 + R1
    inst_in->write(inst1);
    EventDriven::Tracer::getInstance().traceInstruction(
        sched.getCurrentTime(), "Test", "ADD", "R3=R0+R1=15");
  });

  scheduler->scheduleAt(5, [&](EventDriven::EventScheduler& sched) {
    auto inst2 = std::make_shared<PEInstructionPacket>(ALUOp::MUL, 3, 2,
                                                       4);  // R4 = R3 * R2
    inst_in->write(inst2);
    EventDriven::Tracer::getInstance().traceInstruction(
        sched.getCurrentTime(), "Test", "MUL", "R4=R3*R2=45");
  });

  scheduler->scheduleAt(10, [&](EventDriven::EventScheduler& sched) {
    auto inst3 = std::make_shared<PEInstructionPacket>(ALUOp::PASS_A, 4, 0,
                                                       5);  // R5 = R4
    inst_in->write(inst3);
    EventDriven::Tracer::getInstance().traceInstruction(
        sched.getCurrentTime(), "Test", "PASS_A", "R5=R4");
  });

  // Run simulation
  scheduler->run(30);

  // Verify register values
  EXPECT_EQ(pe->readRegister(3), 15);  // 10 + 5
  EXPECT_EQ(pe->readRegister(4), 45);  // 15 * 3

  // Check output
  EXPECT_TRUE(data_out->hasData());
  auto output = std::dynamic_pointer_cast<IntDataPacket>(data_out->read());
  ASSERT_NE(output, nullptr);
  EXPECT_EQ(output->getValue(), 45);

  std::cout << "\n=== Event-Driven PE Instruction Stream ===" << std::endl;
  std::cout << "R3 (10+5): " << pe->readRegister(3) << std::endl;
  std::cout << "R4 (15*3): " << pe->readRegister(4) << std::endl;
  std::cout << "Final time: " << scheduler->getCurrentTime() << std::endl;

  EventDriven::Tracer::getInstance().dump();
  EventDriven::Tracer::getInstance().setEnabled(false);
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
