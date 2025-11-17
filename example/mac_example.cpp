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

#include "../src/comp/core/core.h"
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

    // Generate MAC instructions: multiply then add
    // Step 1: Load immediates into registers (use ADDI)
    // Step 2: Multiply a[i] * b[i] using MLU (MUL rd, rs1, rs2 = opcode 0x33)
    // Step 3: Accumulate results using ALU ADD
    // Expected: (10*5) + (20*6) + (30*7) + (40*8) = 50 + 120 + 210 + 320 = 700

    std::vector<uint32_t> instructions;

    // Load a[0]=10 into x1: ADDI x1, x0, 10
    instructions.push_back((10 << 20) | (0 << 15) | (0 << 12) | (1 << 7) |
                           0x13);

    // Load b[0]=5 into x2: ADDI x2, x0, 5
    instructions.push_back((5 << 20) | (0 << 15) | (0 << 12) | (2 << 7) | 0x13);

    // Load a[1]=20 into x3: ADDI x3, x0, 20
    instructions.push_back((20 << 20) | (0 << 15) | (0 << 12) | (3 << 7) |
                           0x13);

    // Load b[1]=6 into x4: ADDI x4, x0, 6
    instructions.push_back((6 << 20) | (0 << 15) | (0 << 12) | (4 << 7) | 0x13);

    // Load a[2]=30 into x5: ADDI x5, x0, 30
    instructions.push_back((30 << 20) | (0 << 15) | (0 << 12) | (5 << 7) |
                           0x13);

    // Load b[2]=7 into x6: ADDI x6, x0, 7
    instructions.push_back((7 << 20) | (0 << 15) | (0 << 12) | (6 << 7) | 0x13);

    // Load a[3]=40 into x7: ADDI x7, x0, 40
    instructions.push_back((40 << 20) | (0 << 15) | (0 << 12) | (7 << 7) |
                           0x13);

    // Load b[3]=8 into x8: ADDI x8, x0, 8
    instructions.push_back((8 << 20) | (0 << 15) | (0 << 12) | (8 << 7) | 0x13);

    // MUL x9, x1, x2 (10 * 5 = 50)
    // MUL encoding: funct7=1, rs2, rs1, funct3=0, rd, opcode=0x33
    instructions.push_back((1 << 25) | (2 << 20) | (1 << 15) | (0 << 12) |
                           (9 << 7) | 0x33);

    // MUL x10, x3, x4 (20 * 6 = 120)
    instructions.push_back((1 << 25) | (4 << 20) | (3 << 15) | (0 << 12) |
                           (10 << 7) | 0x33);

    // MUL x11, x5, x6 (30 * 7 = 210)
    instructions.push_back((1 << 25) | (6 << 20) | (5 << 15) | (0 << 12) |
                           (11 << 7) | 0x33);

    // MUL x12, x7, x8 (40 * 8 = 320)
    instructions.push_back((1 << 25) | (8 << 20) | (7 << 15) | (0 << 12) |
                           (12 << 7) | 0x33);

    // ADD x13, x9, x10 (50 + 120 = 170)
    // ADD encoding: funct7=0, rs2, rs1, funct3=0, rd, opcode=0x33
    instructions.push_back((0 << 25) | (10 << 20) | (9 << 15) | (0 << 12) |
                           (13 << 7) | 0x33);

    // ADD x14, x11, x12 (210 + 320 = 530)
    instructions.push_back((0 << 25) | (12 << 20) | (11 << 15) | (0 << 12) |
                           (14 << 7) | 0x33);

    // ADD x15, x13, x14 (170 + 530 = 700)
    instructions.push_back((0 << 25) | (14 << 20) | (13 << 15) | (0 << 12) |
                           (15 << 7) | 0x33);

    for (size_t i = 0; i < instructions.size(); i++) {
      core->loadInstruction(i * 4, instructions[i]);
    }
    std::cout << "    Loaded " << instructions.size() << " instructions\n"
              << std::endl;
    std::cout << "    Instruction sequence:\n"
              << "      1. Load operands into x1-x8 (8 ADDI instructions)\n"
              << "      2. Multiply 4 pairs using MLU (4 MUL instructions)\n"
              << "      3. Add partial sums using ALU (3 ADD instructions)\n"
              << "      4. Final result in x15 = 700\n"
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
