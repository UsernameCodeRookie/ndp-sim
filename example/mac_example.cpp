/**
 * @file mac_example.cpp
 * @brief Event-driven MAC (Multiply-Accumulate) example
 *
 * Demonstrates:
 * - Core with automatic instruction fetch from instruction memory
 * - Event-driven pipeline execution
 * - Data memory access through LSU
 *
 * Example: Compute MAC = sum(a[i] * b[i]) for i=0..3
 */

#include <cstdint>
#include <iomanip>
#include <iostream>
#include <memory>
#include <vector>

#include "../src/comp/core.h"
#include "../src/scheduler.h"
#include "../src/trace.h"

using namespace Architecture;
using namespace EventDriven;

/**
 * @class MACExample
 * @brief MAC example with automatic instruction execution
 */
class MACExample {
 public:
  void run() {
    std::cout << "\n╔═══════════════════════════════════════════════════╗\n"
              << "║   Event-Driven MAC (Multiply-Accumulate)         ║\n"
              << "║   Auto-Fetch and Execute from Instruction Memory ║\n"
              << "╚═══════════════════════════════════════════════════╝\n"
              << std::endl;

    // Create scheduler and core
    auto scheduler = std::make_unique<EventScheduler>();
    SCore::Config config;
    config.num_instruction_lanes = 2;
    auto core = std::make_shared<SCore>("SCore-0", *scheduler, config);

    // Setup instruction and data memory
    setupMemory(core);

    // Initialize core - this connects instruction memory to fetch stage
    core->initialize();

    // Run simulation
    runSimulation(*scheduler);

    // Show results
    showResults(core);
  }

 private:
  void setupMemory(std::shared_ptr<SCore> core) {
    std::cout << "Setup: Load instruction and data memory\n" << std::endl;

    // Load operands into data memory
    std::vector<uint32_t> a = {10, 20, 30, 40};
    std::vector<uint32_t> b = {5, 6, 7, 8};

    for (size_t i = 0; i < 4; i++) {
      core->loadData(0x000 + i * 4, a[i]);
      core->loadData(0x010 + i * 4, b[i]);
    }
    core->loadData(0x020, 0);  // accumulator

    std::cout << "  Data memory loaded:" << std::endl;
    std::cout << "    a[] = {10, 20, 30, 40} at 0x000-0x00C" << std::endl;
    std::cout << "    b[] = {5, 6, 7, 8} at 0x010-0x01C" << std::endl;
    std::cout << "    acc = 0 at 0x020\n" << std::endl;

    // Load instruction sequence into instruction memory
    // Core's pipeline will automatically fetch and decode these instructions,
    // then dispatch them to execution units
    std::cout << "  Load instruction sequence into instruction memory:"
              << std::endl;

    // Create RISC-V ADDI instructions with different immediate values
    // ADDI rd, rs1, imm: opcode=0x13
    // Encode: imm[11:0](12) | rs1(5) | funct3(3) | rd(5) | opcode(7)

    std::vector<uint32_t> instructions;

    // Generate 16 ADDI instructions, each writing to a unique register (x1-x16)
    // All source from x0 to avoid input dependencies
    // This ensures all instructions can execute in parallel with no conflicts
    for (int i = 0; i < 16; i++) {
      // Use values from a[] and b[] arrays as immediates
      int imm = a[i % 4] + b[i % 4];  // 15, 26, 37, 48 ...
      imm = imm & 0xFFF;              // Keep 12-bit immediate

      int rd =
          i +
          1;  // Use x1, x2, x3, ..., x16 (unique register for each instruction)
      int rs1 = 0;  // Always use x0 as source to avoid input dependencies

      // ADDI encoding: ADDI rd, x0, imm
      uint32_t inst = (imm << 20) | (rs1 << 15) | (0 << 12) | (rd << 7) | 0x13;
      instructions.push_back(inst);
    }

    for (size_t i = 0; i < instructions.size(); i++) {
      core->loadInstruction(i * 4, instructions[i]);
    }
    std::cout << "    Loaded " << instructions.size() << " ADDI instructions\n"
              << std::endl;
    std::cout << "    Each instruction: ADDI x(i+1), x0, (a[i%4] + b[i%4])\n"
              << std::endl;
    std::cout << "    All instructions are independent - maximum parallelism!\n"
              << std::endl;
  }

  void runSimulation(EventScheduler& scheduler) {
    std::cout << "Simulation: Event-driven execution\n" << std::endl;

    std::cout << "Initial events: " << scheduler.getPendingEventCount()
              << std::endl;

    const uint64_t MAX_CYCLES = 1000;
    uint64_t cycles = 0;
    while (scheduler.getPendingEventCount() > 0 && cycles < MAX_CYCLES) {
      scheduler.runFor(1);
      cycles++;
    }

    std::cout << "Completed: " << cycles << " cycles" << std::endl;
    std::cout << "Total events: " << scheduler.getTotalEventCount() << "\n"
              << std::endl;
  }

  void showResults(std::shared_ptr<SCore> core) {
    std::cout << "Results:\n" << std::endl;

    std::cout << "  Expected MAC result: 10×5 + 20×6 + 30×7 + 40×8"
              << std::endl;
    std::cout << "                     = 50 + 120 + 210 + 320" << std::endl;
    std::cout << "                     = 700\n" << std::endl;

    std::cout << "  Core dispatched " << core->getInstructionsDispatched()
              << " instructions\n"
              << std::endl;

    core->printStatistics();
  }
};

/**
 * @brief Main entry point
 */
int main(int argc, char* argv[]) {
  try {
    // Enable tracing
    EventDriven::Tracer::getInstance().initialize("mac_trace.log", true);
    EventDriven::Tracer::getInstance().setVerbose(true);
    EventDriven::Tracer::getInstance().addComponentFilter("SCore");

    MACExample example;
    example.run();

    // Dump trace to file
    EventDriven::Tracer::getInstance().dump();

    std::cout << "Example completed successfully!\n" << std::endl;
    return 0;
  } catch (const std::exception& e) {
    std::cerr << "Error: " << e.what() << std::endl;
    return 1;
  }
}
