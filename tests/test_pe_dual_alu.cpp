#include <gtest/gtest.h>

#include "../src/components/pe.h"
#include "../src/scheduler.h"
#include "../src/trace.h"

class PEDualALUTest : public ::testing::Test {
 protected:
  void SetUp() override {
    scheduler = std::make_unique<EventDriven::EventScheduler>();
  }

  void TearDown() override { scheduler.reset(); }

  std::unique_ptr<EventDriven::EventScheduler> scheduler;
};

// Test PE with integer operations (using INTU)
TEST_F(PEDualALUTest, IntegerOperations) {
  auto pe = std::make_shared<ProcessingElement>("test_pe", *scheduler, 1, 8, 2);
  pe->start();

  // Initialize registers
  pe->initRegister(0, 10);
  pe->initRegister(1, 5);
  pe->initRegister(2, 3);

  auto inst_port = pe->getPort("inst_in");

  // Test integer ADD (should use INTU)
  auto inst1 =
      std::make_shared<PEInstructionPacket>(ALUOp::ADD, 0, 1, 3, false);
  inst_port->write(inst1);
  pe->tick();

  EXPECT_EQ(pe->readRegister(3), 15);  // 10 + 5 = 15

  // Test integer MUL (should use INTU)
  auto inst2 =
      std::make_shared<PEInstructionPacket>(ALUOp::MUL, 1, 2, 4, false);
  inst_port->write(inst2);
  pe->tick();

  EXPECT_EQ(pe->readRegister(4), 15);  // 5 * 3 = 15

  std::cout << "\n=== Integer Operations Test ===" << std::endl;
  pe->printRegisters();
}

// Test PE with floating point operations (using FPU)
TEST_F(PEDualALUTest, FloatingPointOperations) {
  auto pe = std::make_shared<ProcessingElement>("test_pe", *scheduler, 1, 8, 2);
  pe->start();

  // Initialize FP registers
  pe->initFPRegister(0, 10.5f);
  pe->initFPRegister(1, 5.2f);
  pe->initFPRegister(2, 2.0f);

  auto inst_port = pe->getPort("inst_in");

  // Test FP ADD (should use FPU)
  auto inst1 = std::make_shared<PEInstructionPacket>(ALUOp::ADD, 0, 1, 3, true);
  inst_port->write(inst1);
  pe->tick();

  EXPECT_FLOAT_EQ(pe->readFPRegister(3), 15.7f);  // 10.5 + 5.2 = 15.7

  // Test FP MUL (should use FPU)
  auto inst2 = std::make_shared<PEInstructionPacket>(ALUOp::MUL, 1, 2, 4, true);
  inst_port->write(inst2);
  pe->tick();

  EXPECT_FLOAT_EQ(pe->readFPRegister(4), 10.4f);  // 5.2 * 2.0 = 10.4

  std::cout << "\n=== Floating Point Operations Test ===" << std::endl;
  pe->printRegisters();
}

// Test PE with mixed integer and FP operations
TEST_F(PEDualALUTest, MixedOperations) {
  auto pe = std::make_shared<ProcessingElement>("test_pe", *scheduler, 1, 8, 4);
  pe->start();

  // Initialize both int and FP registers
  pe->initRegister(0, 10);
  pe->initRegister(1, 5);
  pe->initFPRegister(0, 10.5f);
  pe->initFPRegister(1, 5.5f);

  auto inst_port = pe->getPort("inst_in");

  // Queue integer operation
  auto int_inst =
      std::make_shared<PEInstructionPacket>(ALUOp::ADD, 0, 1, 2, false);
  inst_port->write(int_inst);

  // Tick to enqueue
  pe->tick();

  // Queue FP operation
  auto fp_inst =
      std::make_shared<PEInstructionPacket>(ALUOp::ADD, 0, 1, 2, true);
  inst_port->write(fp_inst);

  // Execute both operations
  pe->tick();                          // Process int operation
  EXPECT_EQ(pe->readRegister(2), 15);  // 10 + 5 = 15

  pe->tick();                                     // Process FP operation
  EXPECT_FLOAT_EQ(pe->readFPRegister(2), 16.0f);  // 10.5 + 5.5 = 16.0

  std::cout << "\n=== Mixed Operations Test ===" << std::endl;
  pe->printRegisters();
  pe->printStatistics();
}

// Test MAC operations on both INTU and FPU
TEST_F(PEDualALUTest, DualMACOperations) {
  auto pe = std::make_shared<ProcessingElement>("test_pe", *scheduler, 1, 8, 4);
  pe->start();

  // Initialize registers
  pe->initRegister(0, 3);
  pe->initRegister(1, 4);
  pe->initFPRegister(0, 2.5f);
  pe->initFPRegister(1, 3.0f);

  auto inst_port = pe->getPort("inst_in");

  // Reset accumulators
  pe->resetMACAccumulator();
  pe->resetFPMACAccumulator();

  // Integer MAC: 0 + (3 * 4) = 12
  auto int_mac1 =
      std::make_shared<PEInstructionPacket>(ALUOp::MAC, 0, 1, 2, false);
  inst_port->write(int_mac1);
  pe->tick();
  EXPECT_EQ(pe->getMACAccumulator(), 12);

  // FP MAC: 0 + (2.5 * 3.0) = 7.5
  auto fp_mac1 =
      std::make_shared<PEInstructionPacket>(ALUOp::MAC, 0, 1, 2, true);
  inst_port->write(fp_mac1);
  pe->tick();
  EXPECT_FLOAT_EQ(pe->getFPMACAccumulator(), 7.5f);

  // Integer MAC: 12 + (3 * 4) = 24
  auto int_mac2 =
      std::make_shared<PEInstructionPacket>(ALUOp::MAC, 0, 1, 3, false);
  inst_port->write(int_mac2);
  pe->tick();
  EXPECT_EQ(pe->getMACAccumulator(), 24);

  // FP MAC: 7.5 + (2.5 * 3.0) = 15.0
  auto fp_mac2 =
      std::make_shared<PEInstructionPacket>(ALUOp::MAC, 0, 1, 3, true);
  inst_port->write(fp_mac2);
  pe->tick();
  EXPECT_FLOAT_EQ(pe->getFPMACAccumulator(), 15.0f);

  std::cout << "\n=== Dual MAC Operations Test ===" << std::endl;
  std::cout << "Integer MAC accumulator: " << pe->getMACAccumulator()
            << std::endl;
  std::cout << "FP MAC accumulator: " << pe->getFPMACAccumulator() << std::endl;
  pe->printStatistics();
}

// Test pipeline depth differences between INTU and FPU
TEST_F(PEDualALUTest, PipelineDepthDifference) {
  std::cout << "\n=== Pipeline Depth Test ===" << std::endl;
  std::cout << "INTU pipeline stages: "
            << PrecisionTraits<Int32Precision>::pipeline_stages << std::endl;
  std::cout << "FPU pipeline stages: "
            << PrecisionTraits<Float32Precision>::pipeline_stages << std::endl;

  EXPECT_EQ(PrecisionTraits<Int32Precision>::pipeline_stages, 3);
  EXPECT_EQ(PrecisionTraits<Float32Precision>::pipeline_stages, 5);
}

// Test statistics collection for both units
TEST_F(PEDualALUTest, StatisticsCollection) {
  auto pe = std::make_shared<ProcessingElement>("test_pe", *scheduler, 1, 8, 4);
  pe->start();

  // Initialize registers
  pe->initRegister(0, 10);
  pe->initRegister(1, 5);
  pe->initFPRegister(0, 10.5f);
  pe->initFPRegister(1, 5.5f);

  auto inst_port = pe->getPort("inst_in");

  // Execute 3 integer operations
  for (int i = 0; i < 3; i++) {
    auto int_inst =
        std::make_shared<PEInstructionPacket>(ALUOp::ADD, 0, 1, 2, false);
    inst_port->write(int_inst);
    pe->tick();
  }

  // Execute 2 FP operations
  for (int i = 0; i < 2; i++) {
    auto fp_inst =
        std::make_shared<PEInstructionPacket>(ALUOp::MUL, 0, 1, 3, true);
    inst_port->write(fp_inst);
    pe->tick();
  }

  std::cout << "\n=== Statistics Collection Test ===" << std::endl;
  pe->printStatistics();
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
