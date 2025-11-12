#include <iostream>

#include "../components/alu.h"
#include "../components/pe.h"
#include "../scheduler.h"

int main() {
  std::cout << "\n========================================" << std::endl;
  std::cout << "ALU Precision Support Demo" << std::endl;
  std::cout << "========================================\n" << std::endl;

  // Create scheduler
  auto scheduler = std::make_shared<EventDriven::EventScheduler>();

  // ============================================
  // Part 1: Demonstrate INTU (INT32 ALU)
  // ============================================
  std::cout << "=== Part 1: INTU (Integer Unit - INT32) ===" << std::endl;

  auto intu = std::make_shared<INTUComponent>("INTU", *scheduler, 1);
  intu->start();

  std::cout << "Precision: " << PrecisionTraits<Int32Precision>::name
            << std::endl;
  std::cout << "Pipeline stages: "
            << PrecisionTraits<Int32Precision>::pipeline_stages << std::endl;
  std::cout << "Data type size: "
            << sizeof(PrecisionTraits<Int32Precision>::DataType) << " bytes\n"
            << std::endl;

  // Test integer operations
  int int_a = 15, int_b = 7;
  std::cout << "Testing integer operations:" << std::endl;
  std::cout << "  " << int_a << " + " << int_b << " = "
            << INTUComponent::executeOperation(int_a, int_b, ALUOp::ADD)
            << std::endl;
  std::cout << "  " << int_a << " * " << int_b << " = "
            << INTUComponent::executeOperation(int_a, int_b, ALUOp::MUL)
            << std::endl;
  std::cout << "  " << int_a << " & " << int_b << " = "
            << INTUComponent::executeOperation(int_a, int_b, ALUOp::AND)
            << " (bitwise AND)" << std::endl;

  // ============================================
  // Part 2: Demonstrate FPU (FP32 ALU)
  // ============================================
  std::cout << "\n=== Part 2: FPU (Floating Point Unit - FP32) ==="
            << std::endl;

  auto fpu = std::make_shared<FPUComponent>("FPU", *scheduler, 1);
  fpu->start();

  std::cout << "Precision: " << PrecisionTraits<Float32Precision>::name
            << std::endl;
  std::cout << "Pipeline stages: "
            << PrecisionTraits<Float32Precision>::pipeline_stages << std::endl;
  std::cout << "Data type size: "
            << sizeof(PrecisionTraits<Float32Precision>::DataType) << " bytes\n"
            << std::endl;

  // Test floating point operations
  float fp_a = 3.14f, fp_b = 2.5f;
  std::cout << "Testing floating point operations:" << std::endl;
  std::cout << "  " << fp_a << " + " << fp_b << " = "
            << FPUComponent::executeOperation(fp_a, fp_b, ALUOp::ADD)
            << std::endl;
  std::cout << "  " << fp_a << " * " << fp_b << " = "
            << FPUComponent::executeOperation(fp_a, fp_b, ALUOp::MUL)
            << std::endl;
  std::cout << "  " << fp_a << " / " << fp_b << " = "
            << FPUComponent::executeOperation(fp_a, fp_b, ALUOp::DIV)
            << std::endl;

  // ============================================
  // Part 3: Demonstrate PE with dual ALUs
  // ============================================
  std::cout << "\n=== Part 3: Processing Element with INTU + FPU ==="
            << std::endl;

  auto pe = std::make_shared<ProcessingElement>("PE0", *scheduler, 1, 16, 4);
  pe->start();

  // Initialize integer registers
  pe->initRegister(0, 10);
  pe->initRegister(1, 20);
  pe->initRegister(2, 5);

  // Initialize floating point registers
  pe->initFPRegister(0, 3.5f);
  pe->initFPRegister(1, 2.0f);
  pe->initFPRegister(2, 1.5f);

  std::cout << "\nInitial register state:" << std::endl;
  std::cout << "  Integer registers: R0=10, R1=20, R2=5" << std::endl;
  std::cout << "  Float registers: FR0=3.5, FR1=2.0, FR2=1.5" << std::endl;

  auto inst_port = pe->getPort("inst_in");

  // Execute integer operations (using INTU)
  std::cout << "\nExecuting integer operations (INTU):" << std::endl;
  auto int_add = std::make_shared<PEInstructionPacket>(ALUOp::ADD, 0, 1, 3,
                                                       false);  // R3 = R0 + R1
  inst_port->write(int_add);
  pe->tick();
  std::cout << "  R3 = R0 + R1 = " << pe->readRegister(3) << " (expected: 30)"
            << std::endl;

  auto int_mul = std::make_shared<PEInstructionPacket>(ALUOp::MUL, 2, 3, 4,
                                                       false);  // R4 = R2 * R3
  inst_port->write(int_mul);
  pe->tick();
  std::cout << "  R4 = R2 * R3 = " << pe->readRegister(4) << " (expected: 150)"
            << std::endl;

  // Execute floating point operations (using FPU)
  std::cout << "\nExecuting floating point operations (FPU):" << std::endl;
  auto fp_add = std::make_shared<PEInstructionPacket>(ALUOp::ADD, 0, 1, 3,
                                                      true);  // FR3 = FR0 + FR1
  inst_port->write(fp_add);
  pe->tick();
  std::cout << "  FR3 = FR0 + FR1 = " << pe->readFPRegister(3)
            << " (expected: 5.5)" << std::endl;

  auto fp_mul = std::make_shared<PEInstructionPacket>(ALUOp::MUL, 2, 3, 4,
                                                      true);  // FR4 = FR2 * FR3
  inst_port->write(fp_mul);
  pe->tick();
  std::cout << "  FR4 = FR2 * FR3 = " << pe->readFPRegister(4)
            << " (expected: 8.25)" << std::endl;

  // ============================================
  // Part 4: Demonstrate MAC operations
  // ============================================
  std::cout << "\n=== Part 4: MAC (Multiply-Accumulate) Operations ==="
            << std::endl;

  // Integer MAC
  pe->resetMACAccumulator();
  std::cout << "\nInteger MAC operations:" << std::endl;
  auto int_mac1 = std::make_shared<PEInstructionPacket>(
      ALUOp::MAC, 0, 1, 5, false);  // acc = 0 + (10 * 20)
  inst_port->write(int_mac1);
  pe->tick();
  std::cout << "  MAC(10, 20): accumulator = " << pe->getMACAccumulator()
            << " (expected: 200)" << std::endl;

  auto int_mac2 = std::make_shared<PEInstructionPacket>(
      ALUOp::MAC, 2, 2, 5, false);  // acc = 200 + (5 * 5)
  inst_port->write(int_mac2);
  pe->tick();
  std::cout << "  MAC(5, 5): accumulator = " << pe->getMACAccumulator()
            << " (expected: 225)" << std::endl;

  // Floating point MAC
  pe->resetFPMACAccumulator();
  std::cout << "\nFloating point MAC operations:" << std::endl;
  auto fp_mac1 = std::make_shared<PEInstructionPacket>(
      ALUOp::MAC, 0, 1, 5, true);  // acc = 0 + (3.5 * 2.0)
  inst_port->write(fp_mac1);
  pe->tick();
  std::cout << "  MAC(3.5, 2.0): accumulator = " << pe->getFPMACAccumulator()
            << " (expected: 7.0)" << std::endl;

  auto fp_mac2 = std::make_shared<PEInstructionPacket>(
      ALUOp::MAC, 2, 2, 5, true);  // acc = 7.0 + (1.5 * 1.5)
  inst_port->write(fp_mac2);
  pe->tick();
  std::cout << "  MAC(1.5, 1.5): accumulator = " << pe->getFPMACAccumulator()
            << " (expected: 9.25)" << std::endl;

  // ============================================
  // Part 5: Statistics
  // ============================================
  std::cout << "\n=== Part 5: Performance Statistics ===" << std::endl;
  pe->printStatistics();

  std::cout << "\n========================================" << std::endl;
  std::cout << "Demo completed successfully!" << std::endl;
  std::cout << "========================================\n" << std::endl;

  return 0;
}
