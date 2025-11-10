#include <gtest/gtest.h>

#include <memory>

#include "../src/components/alu.h"
#include "../src/components/int_packet.h"
#include "../src/components/pe.h"
#include "../src/ready_valid_connection.h"
#include "../src/scheduler.h"
#include "../src/tick_connection.h"

/**
 * @brief Test ALU operations
 */
TEST(ALUTest, BasicOperations) {
  // Test ADD
  EXPECT_EQ(ALUComponent::executeOperation(10, 20, ALUOp::ADD), 30);

  // Test SUB
  EXPECT_EQ(ALUComponent::executeOperation(50, 15, ALUOp::SUB), 35);

  // Test MUL
  EXPECT_EQ(ALUComponent::executeOperation(6, 7, ALUOp::MUL), 42);

  // Test DIV
  EXPECT_EQ(ALUComponent::executeOperation(100, 4, ALUOp::DIV), 25);

  // Test AND
  EXPECT_EQ(ALUComponent::executeOperation(0xFF, 0x0F, ALUOp::AND), 15);

  // Test OR
  EXPECT_EQ(ALUComponent::executeOperation(0xF0, 0x0F, ALUOp::OR), 255);

  // Test XOR
  EXPECT_EQ(ALUComponent::executeOperation(0xFF, 0x55, ALUOp::XOR), 0xAA);

  // Test SLL
  EXPECT_EQ(ALUComponent::executeOperation(1, 3, ALUOp::SLL), 8);

  // Test MAX
  EXPECT_EQ(ALUComponent::executeOperation(10, 20, ALUOp::MAX), 20);

  // Test MIN
  EXPECT_EQ(ALUComponent::executeOperation(10, 20, ALUOp::MIN), 10);

  // Test ABS
  EXPECT_EQ(ALUComponent::executeOperation(-42, 0, ALUOp::ABS), 42);

  // Test NEG
  EXPECT_EQ(ALUComponent::executeOperation(42, 0, ALUOp::NEG), -42);
}

/**
 * @brief Test ALU data packet
 */
TEST(ALUTest, ALUDataPacket) {
  auto packet = std::make_shared<ALUDataPacket>(10, 20, ALUOp::ADD);

  EXPECT_EQ(packet->getOperandA(), 10);
  EXPECT_EQ(packet->getOperandB(), 20);
  EXPECT_EQ(packet->getOperation(), ALUOp::ADD);

  // Test clone
  auto cloned = packet->clone();
  auto cloned_alu = std::dynamic_pointer_cast<ALUDataPacket>(cloned);
  ASSERT_NE(cloned_alu, nullptr);
  EXPECT_EQ(cloned_alu->getOperandA(), 10);
  EXPECT_EQ(cloned_alu->getOperandB(), 20);
}

/**
 * @brief Test ready/valid connection
 */
TEST(ReadyValidConnectionTest, BasicHandshake) {
  EventDriven::EventScheduler scheduler;

  auto conn = std::make_shared<Architecture::ReadyValidConnection>(
      "test_conn", scheduler, 10, 2);

  EXPECT_TRUE(conn->canAcceptData());
  EXPECT_FALSE(conn->hasDataToSend());
  EXPECT_EQ(conn->getBufferOccupancy(), 0);
  EXPECT_EQ(conn->getBufferSize(), 2);
}

/**
 * @brief Test PE register file
 */
TEST(PETest, RegisterFile) {
  EventDriven::EventScheduler scheduler;

  auto pe = std::make_shared<ProcessingElement>("PE", scheduler, 10, 32, 4);

  // Test write and read
  pe->writeRegister(0, 42);
  EXPECT_EQ(pe->readRegister(0), 42);

  pe->writeRegister(10, 100);
  EXPECT_EQ(pe->readRegister(10), 100);

  // Test invalid address
  EXPECT_EQ(pe->readRegister(100), 0);
}

/**
 * @brief Test PE instruction queue
 */
TEST(PETest, InstructionQueue) {
  EventDriven::EventScheduler scheduler;

  auto pe = std::make_shared<ProcessingElement>("PE", scheduler, 10, 32, 2);

  EXPECT_EQ(pe->getQueueOccupancy(), 0);
  EXPECT_TRUE(pe->isReady());  // Queue not full, should be ready
}

/**
 * @brief Test PE instruction execution
 */
TEST(PETest, InstructionExecution) {
  EventDriven::EventScheduler scheduler;

  auto pe = std::make_shared<ProcessingElement>("PE", scheduler, 10, 32, 4);

  // Initialize registers
  pe->initRegister(0, 10);
  pe->initRegister(1, 20);

  // Create and send instruction
  auto inst_packet = std::make_shared<PEInstructionPacket>(ALUOp::ADD, 0, 1,
                                                           2);  // R2 = R0 + R1

  auto inst_in = pe->getPort("inst_in");
  inst_in->write(
      std::static_pointer_cast<Architecture::DataPacket>(inst_packet));

  // Start and run PE
  pe->start(0);
  scheduler.run(20);

  // Check result
  EXPECT_EQ(pe->readRegister(2), 30);  // R2 should be 10 + 20 = 30
}

/**
 * @brief Test PE with multiple instructions
 */
TEST(PETest, MultipleInstructions) {
  EventDriven::EventScheduler scheduler;

  auto pe = std::make_shared<ProcessingElement>("PE", scheduler, 10, 32, 8);

  // Initialize registers
  pe->initRegister(0, 5);
  pe->initRegister(1, 10);
  pe->initRegister(2, 3);

  // Put two instructions in the input port before starting
  // Note: Only the last write will be preserved since port holds one data
  // So we need to test with one instruction at a time

  auto inst1 = std::make_shared<PEInstructionPacket>(ALUOp::ADD, 0, 1, 3);
  pe->getPort("inst_in")->write(
      std::static_pointer_cast<Architecture::DataPacket>(inst1));

  pe->start(0);
  scheduler.run(20);

  // First instruction should execute: R3 = R0 + R1 = 5 + 10 = 15
  EXPECT_EQ(pe->readRegister(3), 15);
  EXPECT_GT(pe->getTickCount(), 0);
}

/**
 * @brief Test ready/valid connection buffer
 */
TEST(ReadyValidConnectionTest, BufferManagement) {
  EventDriven::EventScheduler scheduler;

  auto conn = std::make_shared<Architecture::ReadyValidConnection>(
      "test_conn", scheduler, 10, 2);

  // Initially can accept data
  EXPECT_TRUE(conn->canAcceptData());
  EXPECT_EQ(conn->getBufferOccupancy(), 0);
}

/**
 * @brief Test ALU operation names
 */
TEST(ALUTest, OperationNames) {
  EXPECT_EQ(ALUComponent::getOpName(ALUOp::ADD), "ADD");
  EXPECT_EQ(ALUComponent::getOpName(ALUOp::SUB), "SUB");
  EXPECT_EQ(ALUComponent::getOpName(ALUOp::MUL), "MUL");
  EXPECT_EQ(ALUComponent::getOpName(ALUOp::DIV), "DIV");

  EXPECT_EQ(ALUComponent::getOpSymbol(ALUOp::ADD), "+");
  EXPECT_EQ(ALUComponent::getOpSymbol(ALUOp::SUB), "-");
  EXPECT_EQ(ALUComponent::getOpSymbol(ALUOp::MUL), "*");
  EXPECT_EQ(ALUComponent::getOpSymbol(ALUOp::DIV), "/");
}

/**
 * @brief Test PE instruction packet
 */
TEST(PETest, InstructionPacket) {
  auto packet = std::make_shared<PEInstructionPacket>(ALUOp::ADD, 0, 1, 2);

  EXPECT_EQ(packet->getOperation(), ALUOp::ADD);
  EXPECT_EQ(packet->getSrcRegA(), 0);
  EXPECT_EQ(packet->getSrcRegB(), 1);
  EXPECT_EQ(packet->getDstReg(), 2);

  // Test clone
  auto cloned = packet->clone();
  auto cloned_inst = std::dynamic_pointer_cast<PEInstructionPacket>(cloned);
  ASSERT_NE(cloned_inst, nullptr);
  EXPECT_EQ(cloned_inst->getOperation(), ALUOp::ADD);
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
