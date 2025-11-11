#include <iostream>

#include "../src/components/alu.h"
#include "../src/components/pe.h"
#include "../src/components/tpu.h"
#include "../src/trace.h"

// Test MAC operation using PE-based MACUnit
void test_mac_pe() {
  std::cout << "========================================\n";
  std::cout << "Test MAC Unit based on PE\n";
  std::cout << "========================================\n\n";

  // Create event scheduler
  EventDriven::EventScheduler scheduler;

  // Create a single MAC unit
  auto mac = std::make_shared<MACUnit<Float32PrecisionTraits>>(
      "MAC_0_0", scheduler, 1, 0, 0);
  mac->start();

  std::cout << "Test 1: Basic MAC operation\n";
  std::cout << "Computing: acc = 0 + (3 * 4) = 12\n";

  // Set inputs
  mac->setInputA(3.0f);
  mac->setInputB(4.0f);

  // Execute one MAC operation
  mac->tick();

  auto result1 = mac->getAccumulator();
  std::cout << "Result: " << result1 << " (expected: 12)\n\n";

  std::cout << "Test 2: Accumulate another value\n";
  std::cout << "Computing: acc = 12 + (5 * 2) = 22\n";

  // Set new inputs
  mac->setInputA(5.0f);
  mac->setInputB(2.0f);

  // Execute another MAC operation
  mac->tick();

  auto result2 = mac->getAccumulator();
  std::cout << "Result: " << result2 << " (expected: 22)\n\n";

  std::cout << "Test 3: Reset and compute again\n";
  std::cout << "Reset accumulator, then compute: acc = 0 + (7 * 8) = 56\n";

  // Reset accumulator
  mac->resetAccumulator();

  // Set new inputs
  mac->setInputA(7.0f);
  mac->setInputB(8.0f);

  // Execute MAC operation
  mac->tick();

  auto result3 = mac->getAccumulator();
  std::cout << "Result: " << result3 << " (expected: 56)\n\n";

  // Verify results
  bool test1_pass = (result1 == 12);
  bool test2_pass = (result2 == 22);
  bool test3_pass = (result3 == 56);

  std::cout << "========================================\n";
  std::cout << "Test Results:\n";
  std::cout << "Test 1: " << (test1_pass ? "PASSED ✓" : "FAILED ✗") << "\n";
  std::cout << "Test 2: " << (test2_pass ? "PASSED ✓" : "FAILED ✗") << "\n";
  std::cout << "Test 3: " << (test3_pass ? "PASSED ✓" : "FAILED ✗") << "\n";
  std::cout << "Overall: "
            << ((test1_pass && test2_pass && test3_pass) ? "ALL PASSED ✓"
                                                         : "SOME FAILED ✗")
            << "\n";
  std::cout << "========================================\n";
}

// Test standalone PE with MAC operation
void test_pe_mac() {
  std::cout << "\n========================================\n";
  std::cout << "Test PE with MAC Operation\n";
  std::cout << "========================================\n\n";

  // Create event scheduler
  EventDriven::EventScheduler scheduler;

  // Create a PE
  auto pe = std::make_shared<ProcessingElement>("PE_0", scheduler, 1, 32, 4);
  pe->start();

  std::cout << "Test: Direct MAC instruction to PE\n";

  // Initialize registers
  pe->initRegister(0, 6);  // Input A = 6
  pe->initRegister(1, 7);  // Input B = 7

  // Create MAC instruction: acc = acc + (R0 * R1)
  auto mac_inst = std::make_shared<PEInstructionPacket>(ALUOp::MAC, 0, 1,
                                                        2);  // MAC R0, R1 -> R2

  auto inst_in = pe->getPort("inst_in");
  inst_in->write(mac_inst);

  // Execute
  pe->tick();

  auto result1 = pe->getMACAccumulator();
  std::cout << "MAC Result 1: " << result1 << " (expected: 42)\n\n";

  // Execute another MAC
  std::cout << "Execute MAC again with same inputs\n";
  pe->initRegister(0, 3);  // Input A = 3
  pe->initRegister(1, 4);  // Input B = 4

  inst_in->write(mac_inst);
  pe->tick();

  auto result2 = pe->getMACAccumulator();
  std::cout << "MAC Result 2: " << result2
            << " (expected: 54, which is 42 + 12)\n\n";

  // Verify
  bool test1_pass = (result1 == 42);
  bool test2_pass = (result2 == 54);

  std::cout << "========================================\n";
  std::cout << "PE MAC Test Results:\n";
  std::cout << "Test 1: " << (test1_pass ? "PASSED ✓" : "FAILED ✗") << "\n";
  std::cout << "Test 2: " << (test2_pass ? "PASSED ✓" : "FAILED ✗") << "\n";
  std::cout << "Overall: "
            << ((test1_pass && test2_pass) ? "ALL PASSED ✓" : "SOME FAILED ✗")
            << "\n";
  std::cout << "========================================\n";
}

int main() {
  // Initialize tracer
  EventDriven::Tracer::getInstance().initialize("test_mac_pe_trace.log", true);
  EventDriven::Tracer::getInstance().setVerbose(false);

  test_mac_pe();
  test_pe_mac();

  // Dump trace file
  EventDriven::Tracer::getInstance().dump();
  std::cout << "\nTrace file saved to: "
            << EventDriven::Tracer::getInstance().getOutputPath() << "\n";

  return 0;
}
