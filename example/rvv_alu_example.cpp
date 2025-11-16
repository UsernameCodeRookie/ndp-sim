/**
 * @file rvv_alu_example.cpp
 * @brief Event-driven RVV Vector ALU example with Scalar Core frontend
 *
 * Demonstrates:
 * - Scalar Core (SCore) as instruction frontend
 * - RVV Backend for vector execution
 * - Direct Core→RVV interface integration
 * - Event-driven pipeline execution
 * - Vector ALU operations (VADD, VSUB, VAND, VOR)
 *
 * Architecture:
 * SCore (Scalar Core)
 *   ├─ Fetch Stage: Load instructions from memory
 *   ├─ Decode Stage: Decode vector instruction opcodes
 *   └─ Dispatch Stage: Issue vector instructions to RVV Backend
 *         ↓
 * RVVBackend (Vector Execution Engine - implements RVVCoreInterface)
 *   ├─ Decode: Convert instruction to micro-op
 *   ├─ Dispatch: Allocate ROB, check hazards
 *   ├─ Execute: Perform vector ALU operations
 *   └─ Retire: Commit results to vector register file
 *
 * Example: Compute vector operations on arrays
 * - VADD: c[i] = a[i] + b[i] for i=0..7 (8-bit elements)
 * - VSUB: d[i] = c[i] - a[i] for i=0..7
 * - VAND: e[i] = a[i] & b[i]
 * - VOR:  f[i] = a[i] | b[i]
 */

#include <cstdint>
#include <iomanip>
#include <iostream>
#include <memory>
#include <vector>

#include "../src/comp/core.h"
#include "../src/comp/rvv_backend.h"
#include "../src/comp/rvv_interface.h"
#include "../src/scheduler.h"
#include "../src/trace.h"

using namespace Architecture;
using namespace EventDriven;

/**
 * @class RVVALUExample
 * @brief Vector ALU example with SCore frontend and RVV backend
 */
class RVVALUExample {
 public:
  void run() {
    std::cout
        << "\n╔═════════════════════════════════════════════════════════╗\n"
        << "║  Event-Driven RVV Vector ALU Example                    ║\n"
        << "║  SCore Frontend → RVV Backend Execution                 ║\n"
        << "║  With Detailed Timing Trace                             ║\n"
        << "╚═════════════════════════════════════════════════════════╝\n"
        << std::endl;

    // Create scheduler
    auto scheduler = std::make_unique<EventScheduler>();

    // Create RVV Backend (256-bit VLEN for CoralNPU compliance)
    auto rvv_backend =
        std::make_shared<RVVBackend>("RVV-Backend", *scheduler, 1, 256);

    // Create Scalar Core (4-lane for CoralNPU compliance)
    SCore::Config core_config;
    core_config.num_instruction_lanes = 4;  // CoralNPU: 4-lane scalar
    core_config.num_bru_units = 4;          // 4 BRU units to match lanes
    core_config.vector_issue_width = 4;     // 4 vector instructions per cycle
    core_config.vlen = 256;                 // CoralNPU: 256-bit vectors
    auto core = std::make_shared<SCore>("SCore-0", *scheduler, core_config);

    // Set RVV backend as the vector execution interface
    // RVVBackend implements RVVCoreInterface, so it can be used directly
    core->setRVVInterface(rvv_backend);

    // Setup instruction memory in SCore
    setupInstructions(core);

    // Configure RVV Backend with vector state
    setupVectorState(rvv_backend);

    // Load vector instructions into SCore instruction memory
    // The pipeline will automatically fetch, decode, and dispatch these
    loadSCoreInstructions(core);

    // Initialize components
    core->initialize();
    rvv_backend->start(0);

    // Run simulation
    runSimulation(*scheduler, *rvv_backend);

    // Print SCore dispatch statistics
    std::cout << "\nSCore Dispatch Statistics:" << std::endl;
    std::cout << "  Instructions Dispatched: "
              << core->getInstructionsDispatched() << std::endl;
    std::cout << "  Instructions Retired: " << core->getInstructionsRetired()
              << std::endl;

    // Show results
    showResults(*rvv_backend);
  }

 private:
  /**
   * @brief Setup vector instructions in Scalar Core instruction memory
   *
   * Vector instructions are loaded into SCore's instruction memory.
   * SCore's pipeline will:
   * 1. Fetch instructions from instruction memory
   * 2. Decode stage recognizes vector instruction opcodes (0x57, etc.)
   * 3. Dispatch stage automatically issues vector instructions to RVV backend
   *    via the RVVCoreInterface
   */
  void setupInstructions(std::shared_ptr<SCore> core) {
    // Instructions are loaded in loadSCoreInstructions()
    // No manual issuing needed - SCore handles it automatically
  }

  /**
   * @brief Load vector instructions into SCore instruction buffer
   *
   * These instructions will be fetched by SCore and recognized as
   * vector instructions by the decode stage, then dispatched to
   * the RVV backend via the RVVCoreInterface.
   */
  void loadSCoreInstructions(std::shared_ptr<SCore> core) {
    std::cout << "Setup: Load vector instructions into SCore instruction "
                 "buffer\n"
              << std::endl;

    // Vector instructions in RISC-V (RVV extension)
    // Format: We need to encode them with proper opcodes
    // Opcodes: 0x57 (VV format), 0x77 (VI format), etc.

    // For now, we'll use a simplified encoding where we load instruction
    // words that will be decoded as vector instructions

    // VADD v3, v1, v2 - opcode 0x57 (VV format)
    // Encoding: funct6(6) | vm(1) | vs2(5) | vs1(5) | 0(3) | vd(5) | 0x57
    // VADD uses funct6=0x00
    uint32_t vadd = (0x00 << 26) | (1 << 25) | (2 << 20) | (1 << 15) |
                    (0 << 12) | (3 << 7) | 0x57;
    core->loadInstruction(0, vadd);

    // VSUB v4, v3, v1 - opcode 0x57 (VV format), funct6=0x02
    uint32_t vsub = (0x02 << 26) | (1 << 25) | (1 << 20) | (3 << 15) |
                    (0 << 12) | (4 << 7) | 0x57;
    core->loadInstruction(4, vsub);

    // VAND v5, v1, v2 - opcode 0x57 (VV format), funct6=0x09
    uint32_t vand = (0x09 << 26) | (1 << 25) | (2 << 20) | (1 << 15) |
                    (0 << 12) | (5 << 7) | 0x57;
    core->loadInstruction(8, vand);

    // VOR v6, v1, v2 - opcode 0x57 (VV format), funct6=0x0A
    uint32_t vor = (0x0A << 26) | (1 << 25) | (2 << 20) | (1 << 15) |
                   (0 << 12) | (6 << 7) | 0x57;
    core->loadInstruction(12, vor);

    // Load some NOP instructions to prevent fetch from going beyond loaded
    // region
    for (uint32_t i = 4; i < 16; i++) {
      if (i != 1 && i != 2 && i != 3) {      // Skip positions we already loaded
        core->loadInstruction(i * 4, 0x13);  // ADDI x0, x0, 0 (NOP)
      }
    }

    std::cout << "  Loaded 4 vector instructions at addresses 0x0, 0x4, 0x8, "
                 "0xc\n"
              << "  Instructions:" << std::endl;
    std::cout << "    0x0: VADD v3, v1, v2" << std::endl;
    std::cout << "    0x4: VSUB v4, v3, v1" << std::endl;
    std::cout << "    0x8: VAND v5, v1, v2" << std::endl;
    std::cout << "    0xc: VOR  v6, v1, v2" << std::endl;
    std::cout << std::endl;
  }

  /**
   * @brief Setup vector state in RVV Backend
   *
   * Initializes vector configuration and operand registers.
   * This is similar to setupMemory() in mac_example - it prepares
   * the architectural state before the core runs.
   */
  void setupVectorState(std::shared_ptr<RVVBackend> rvv) {
    std::cout << "Setup: Initialize vector state in RVV Backend\n" << std::endl;

    RVVConfigState config;
    config.vl = 8;    // 8 elements
    config.sew = 0;   // 8-bit elements
    config.lmul = 0;  // LMUL = 1
    config.vstart = 0;
    config.ma = false;
    config.ta = false;
    rvv->setConfigState(config);

    std::cout << "  Vector Configuration:" << std::endl;
    std::cout << "    vl=8 elements" << std::endl;
    std::cout << "    sew=0 (8-bit elements)" << std::endl;
    std::cout << "    lmul=0 (LMUL=1)" << std::endl;
    std::cout << "    vtype=0x" << std::hex << config.getVtype() << std::dec
              << std::endl;

    // Initialize input vector registers v1 and v2 with test data
    // v1: [10, 20, 30, 40, 50, 60, 70, 80]
    // v2: [5, 6, 7, 8, 9, 10, 11, 12]
    std::vector<uint8_t> v1_data = {10, 20, 30, 40, 50, 60, 70, 80};
    std::vector<uint8_t> v2_data = {5, 6, 7, 8, 9, 10, 11, 12};

    auto vrf = rvv->getVRF();
    if (vrf) {
      vrf->write(1, v1_data);  // v1
      vrf->write(2, v2_data);  // v2
      std::cout << "\n  Vector Register File initialized:" << std::endl;
      std::cout << "    v1 = [10, 20, 30, 40, 50, 60, 70, 80]" << std::endl;
      std::cout << "    v2 = [5, 6, 7, 8, 9, 10, 11, 12]" << std::endl;
    }

    std::cout << "\n  Expected Results:" << std::endl;
    std::cout << "    VADD v3:     [15, 26, 37, 48, 59, 70, 81, 92]"
              << std::endl;
    std::cout << "    VSUB v4:     [5, 6, 7, 8, 9, 10, 11, 12]" << std::endl;
    std::cout << "    VAND v5:     [0, 4, 6, 8, 8, 8, 6, 16] (bitwise AND)"
              << std::endl;
    std::cout << "    VOR  v6:     [15, 22, 31, 40, 59, 62, 79, 92] (bitwise "
                 "OR)"
              << std::endl;
    std::cout << std::endl;
  }

  /**
   * @brief Run event-driven simulation with timing trace
   */
  void runSimulation(EventScheduler& scheduler, RVVBackend& rvv) {
    std::cout << "Simulation: Event-driven execution with timing trace\n"
              << std::endl;

    std::cout << "Initial pending events: " << scheduler.getPendingEventCount()
              << std::endl;

    const uint64_t MAX_CYCLES = 200;
    uint64_t cycles = 0;

    // Print trace header
    std::cout << "\n┌─────┬──────┬────────┬──────────┬──────────┬─────────┐"
              << std::endl;
    std::cout << "│Cycle│Events│Pending │ Decode   │ Dispatch │ Execute │"
              << std::endl;
    std::cout << "├─────┼──────┼────────┼──────────┼──────────┼─────────┤"
              << std::endl;

    uint64_t prev_decode = 0, prev_dispatch = 0, prev_execute = 0,
             prev_retire = 0;

    while (scheduler.getPendingEventCount() > 0 && cycles < MAX_CYCLES) {
      uint64_t events_before = scheduler.getTotalEventCount();
      scheduler.runFor(1);

      // Get current stats
      uint64_t curr_decode = rvv.getDecodeCount();
      uint64_t curr_dispatch = rvv.getDispatchCount();
      uint64_t curr_execute = rvv.getExecuteCount();
      uint64_t curr_retire = rvv.getRetireCount();

      // Print trace information every cycle with activity
      if (scheduler.getTotalEventCount() > events_before ||
          cycles < 20) {  // Always print first 20 cycles
        uint64_t pending = scheduler.getPendingEventCount();
        uint64_t total_events = scheduler.getTotalEventCount();

        // Show if stage has activity (+1 means processed one instruction)
        std::string decode_str =
            (curr_decode > prev_decode) ? std::string(1, '*') : " ";
        std::string dispatch_str =
            (curr_dispatch > prev_dispatch) ? std::string(1, '*') : " ";
        std::string execute_str =
            (curr_execute > prev_execute) ? std::string(1, '*') : " ";

        std::cout << "│ " << std::setw(3) << cycles << " │ " << std::setw(4)
                  << total_events << " │ " << std::setw(6) << pending << " │ "
                  << std::setw(3) << curr_decode << " " << decode_str
                  << "    │ " << std::setw(3) << curr_dispatch << " "
                  << dispatch_str << "    │ " << std::setw(3) << curr_execute
                  << " " << execute_str << "   │" << std::endl;

        prev_decode = curr_decode;
        prev_dispatch = curr_dispatch;
        prev_execute = curr_execute;
        prev_retire = curr_retire;
      }

      cycles++;
    }

    std::cout << "└─────┴──────┴────────┴──────────┴──────────┴─────────┘"
              << std::endl;
    std::cout << "\nLegend: * = stage processed instruction this cycle"
              << std::endl;

    std::cout << "\nSimulation completed: " << cycles << " cycles" << std::endl;
    std::cout << "Total events processed: " << scheduler.getTotalEventCount()
              << "\n"
              << std::endl;
  }

  /**
   * @brief Show execution results and statistics
   */
  void showResults(RVVBackend& rvv) {
    std::cout << "═════════════════════════════════════════════════════"
              << std::endl;
    std::cout << "Results:\n" << std::endl;

    std::cout << "Vector Backend Pipeline Statistics:" << std::endl;
    std::cout << "  Decode Count:   " << rvv.getDecodeCount() << std::endl;
    std::cout << "  Dispatch Count: " << rvv.getDispatchCount() << std::endl;
    std::cout << "  Execute Count:  " << rvv.getExecuteCount() << std::endl;
    std::cout << "  Retire Count:   " << rvv.getRetireCount() << std::endl;
    std::cout << "  Stall Count:    " << rvv.getStallCount() << std::endl;
    std::cout << "  Is Idle:        " << (rvv.isIdle() ? "YES" : "NO")
              << std::endl;
    std::cout << "  Queue Capacity: " << rvv.getQueueCapacity() << std::endl;

    std::cout << "\nVector Configuration State:" << std::endl;
    auto config = rvv.getConfigState();
    std::cout << "  VL: " << config.vl << std::endl;
    std::cout << "  VSTART: " << config.vstart << std::endl;
    std::cout << "  SEW: " << static_cast<int>(config.sew) << " (8-bit)"
              << std::endl;
    std::cout << "  LMUL: " << static_cast<int>(config.lmul) << " (LMUL=1)"
              << std::endl;
    std::cout << "  MA: " << (config.ma ? "YES" : "NO") << std::endl;
    std::cout << "  TA: " << (config.ta ? "YES" : "NO") << std::endl;
    std::cout << "  VILL: " << (config.vill ? "ILLEGAL" : "VALID") << std::endl;

    std::cout << "\nvtype CSR encoding: 0x" << std::hex << config.getVtype()
              << std::dec << std::endl;

    std::cout << "\n" << std::string(57, '=') << std::endl;

    // Note: In a full implementation, we would verify results
    // by reading back from vector registers
    std::cout << "\nNote: Vector results stored in VRF (v3-v6)" << std::endl;
    std::cout << "      Access through VRF getRegister() in full integration"
              << std::endl;

    std::cout << "\nExample completed successfully!" << std::endl;
  }
};

/**
 * @brief Main entry point
 */
int main() {
  try {
    // Enable event tracing with SCore and RVV backend filters
    EventDriven::Tracer::getInstance().initialize("rvv_alu_trace.log", true);
    EventDriven::Tracer::getInstance().addComponentFilter("SCore");
    EventDriven::Tracer::getInstance().addComponentFilter("RVV");
    EventDriven::Tracer::getInstance().setVerbose(true);

    RVVALUExample example;
    example.run();

    // Dump trace to file
    EventDriven::Tracer::getInstance().dump();

    std::cout << "\nTrace saved to: data/rvv_alu_trace.log" << std::endl;

    return 0;
  } catch (const std::exception& e) {
    std::cerr << "Error: " << e.what() << std::endl;
    return 1;
  }
}
