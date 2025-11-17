/**
 * @file rvv_alu_example.cpp
 * @brief Event-driven RVV Vector ALU example with Scalar Core frontend
 *
 * Demonstrates:
 * - Scalar Core (SCore) as instruction frontend with 4-lane multi-issue
 * - RVV Backend for vector execution with ROB and out-of-order execution
 * - Direct Core→RVV interface integration
 * - Event-driven pipeline execution
 * - Advanced vector ALU operations with data dependencies
 * - ROB (Reorder Buffer) management and hazard detection
 * - Multiple vector instructions testing WAR/WAW hazards
 *
 * Test Cases:
 * 1. Basic ALU ops: VADD, VSUB, VAND, VOR (no dependencies)
 * 2. Dependent chains: Multiple operations with data dependencies
 * 3. Multi-issue: Multiple instructions issuing in same cycle
 * 4. ROB utilization: Testing reorder buffer capacity
 * 5. Hazard detection: RAW, WAR, WAW dependencies
 *
 * Architecture:
 * SCore (Scalar Core) - 4-lane issue width
 *   ├─ Fetch Stage: Load instructions from memory
 *   ├─ Decode Stage: Decode vector instruction opcodes
 *   └─ Dispatch Stage: Issue up to 4 vector instructions per cycle to RVV
 *         ↓
 * RVVBackend (Vector Execution Engine - implements RVVCoreInterface)
 *   ├─ Decode: Convert instruction to micro-op (up to 6 per cycle)
 *   ├─ Dispatch: Allocate ROB (up to 4), check hazards
 *   ├─ Execute: Perform vector ALU operations (up to 4 parallel)
 *   └─ Retire: Commit results to vector register file (up to 4 per cycle)
 *
 * Example: Comprehensive vector operations test
 * - VADD: Addition with RAW dependency
 * - VSUB: Subtraction with result feedback
 * - VAND: Bitwise AND with new operands
 * - VOR:  Bitwise OR with WAR hazard
 * - VMUL: Multiplication (if ALU supports)
 */

#include <cstdint>
#include <iomanip>
#include <iostream>
#include <memory>
#include <vector>

#include "../src/comp/core/core.h"
#include "../src/comp/rvv/rvv_backend.h"
#include "../src/comp/rvv/rvv_interface.h"
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
   * Test comprehensive scenarios:
   * 1. Independent operations (no data dependencies)
   * 2. RAW dependencies (read-after-write hazards)
   * 3. WAR dependencies (write-after-read hazards)
   * 4. WAW dependencies (write-after-write hazards)
   * 5. Multi-issue scenarios (multiple instructions per cycle)
   *
   * These instructions will be fetched by SCore and recognized as
   * vector instructions by the decode stage, then dispatched to
   * the RVV backend via the RVVCoreInterface.
   */
  void loadSCoreInstructions(std::shared_ptr<SCore> core) {
    std::cout << "Setup: Load vector instructions into SCore instruction "
                 "buffer\n"
              << "       Testing ROB, multi-issue, and data dependencies\n"
              << std::endl;

    // Vector instructions in RISC-V (RVV extension)
    // Format: funct6(6) | vm(1) | vs2(5) | vs1(5) | funct3(3) | vd(5) | 0x57

    uint32_t addr = 0;

    // === TEST BLOCK 1: Independent Operations (0x0-0xf) ===
    // These can all execute in parallel with no dependencies

    // 0x0: VADD v3, v1, v2 - funct6=0x00
    uint32_t vadd1 = (0x00 << 26) | (1 << 25) | (2 << 20) | (1 << 15) |
                     (0 << 12) | (3 << 7) | 0x57;
    core->loadInstruction(addr, vadd1);
    addr += 4;

    // 0x4: VAND v4, v1, v2 - funct6=0x09
    uint32_t vand1 = (0x09 << 26) | (1 << 25) | (2 << 20) | (1 << 15) |
                     (0 << 12) | (4 << 7) | 0x57;
    core->loadInstruction(addr, vand1);
    addr += 4;

    // 0x8: VOR v5, v1, v2 - funct6=0x0A
    uint32_t vor1 = (0x0A << 26) | (1 << 25) | (2 << 20) | (1 << 15) |
                    (0 << 12) | (5 << 7) | 0x57;
    core->loadInstruction(addr, vor1);
    addr += 4;

    // 0xc: VSUB v6, v2, v1 - funct6=0x02
    uint32_t vsub1 = (0x02 << 26) | (1 << 25) | (1 << 20) | (2 << 15) |
                     (0 << 12) | (6 << 7) | 0x57;
    core->loadInstruction(addr, vsub1);
    addr += 4;

    // === TEST BLOCK 2: RAW Dependencies (0x10-0x1f) ===
    // Chain: v3 <- VADD(v1, v2) -> VSUB(v3, v1) -> VAND(result, v2)

    // 0x10: VADD v7, v1, v2 - funct6=0x00
    uint32_t vadd2 = (0x00 << 26) | (1 << 25) | (2 << 20) | (1 << 15) |
                     (0 << 12) | (7 << 7) | 0x57;
    core->loadInstruction(addr, vadd2);
    addr += 4;

    // 0x14: VSUB v8, v7, v1 - depends on v7 (RAW) - funct6=0x02
    uint32_t vsub2 = (0x02 << 26) | (1 << 25) | (1 << 20) | (7 << 15) |
                     (0 << 12) | (8 << 7) | 0x57;
    core->loadInstruction(addr, vsub2);
    addr += 4;

    // 0x18: VAND v9, v8, v2 - depends on v8 (RAW) - funct6=0x09
    uint32_t vand2 = (0x09 << 26) | (1 << 25) | (2 << 20) | (8 << 15) |
                     (0 << 12) | (9 << 7) | 0x57;
    core->loadInstruction(addr, vand2);
    addr += 4;

    // 0x1c: VOR v10, v8, v1 - depends on v8 (RAW, different from above)
    uint32_t vor2 = (0x0A << 26) | (1 << 25) | (1 << 20) | (8 << 15) |
                    (0 << 12) | (10 << 7) | 0x57;
    core->loadInstruction(addr, vor2);
    addr += 4;

    // === TEST BLOCK 3: WAR Dependencies (0x20-0x2f) ===
    // Instruction should not execute before previous reads v1

    // 0x20: VADD v11, v1, v2 - funct6=0x00
    uint32_t vadd3 = (0x00 << 26) | (1 << 25) | (2 << 20) | (1 << 15) |
                     (0 << 12) | (11 << 7) | 0x57;
    core->loadInstruction(addr, vadd3);
    addr += 4;

    // 0x24: VADD v1, v3, v4 - WAR: writes v1 that previous instruction reads
    uint32_t vadd_war = (0x00 << 26) | (1 << 25) | (4 << 20) | (3 << 15) |
                        (0 << 12) | (1 << 7) | 0x57;
    core->loadInstruction(addr, vadd_war);
    addr += 4;

    // 0x28: VSUB v12, v1, v11 - should use new v1 value
    uint32_t vsub3 = (0x02 << 26) | (1 << 25) | (11 << 20) | (1 << 15) |
                     (0 << 12) | (12 << 7) | 0x57;
    core->loadInstruction(addr, vsub3);
    addr += 4;

    // 0x2c: VAND v13, v11, v2 - independent - funct6=0x09
    uint32_t vand3 = (0x09 << 26) | (1 << 25) | (2 << 20) | (11 << 15) |
                     (0 << 12) | (13 << 7) | 0x57;
    core->loadInstruction(addr, vand3);
    addr += 4;

    // === TEST BLOCK 4: WAW Dependencies (0x30-0x3f) ===
    // Multiple writes to same register - only last one visible

    // 0x30: VADD v14, v1, v2 - funct6=0x00
    uint32_t vadd4 = (0x00 << 26) | (1 << 25) | (2 << 20) | (1 << 15) |
                     (0 << 12) | (14 << 7) | 0x57;
    core->loadInstruction(addr, vadd4);
    addr += 4;

    // 0x34: VSUB v14, v3, v4 - WAW: overwrites v14 - funct6=0x02
    uint32_t vsub_waw = (0x02 << 26) | (1 << 25) | (4 << 20) | (3 << 15) |
                        (0 << 12) | (14 << 7) | 0x57;
    core->loadInstruction(addr, vsub_waw);
    addr += 4;

    // 0x38: VOR v14, v5, v6 - WAW: overwrites v14 again - funct6=0x0A
    uint32_t vor_waw = (0x0A << 26) | (1 << 25) | (6 << 20) | (5 << 15) |
                       (0 << 12) | (14 << 7) | 0x57;
    core->loadInstruction(addr, vor_waw);
    addr += 4;

    // 0x3c: VAND v15, v14, v1 - uses final v14 value - funct6=0x09
    uint32_t vand4 = (0x09 << 26) | (1 << 25) | (1 << 20) | (14 << 15) |
                     (0 << 12) | (15 << 7) | 0x57;
    core->loadInstruction(addr, vand4);
    addr += 4;

    // === TEST BLOCK 5: Multi-issue stress test (0x40-0x5f) ===
    // Multiple independent instructions to test 4-lane issue

    // 0x40-0x4f: 4 independent operations per cycle
    // First set
    uint32_t vadd5 = (0x00 << 26) | (1 << 25) | (2 << 20) | (1 << 15) |
                     (0 << 12) | (16 << 7) | 0x57;
    uint32_t vand5 = (0x09 << 26) | (1 << 25) | (2 << 20) | (1 << 15) |
                     (0 << 12) | (17 << 7) | 0x57;
    uint32_t vor5 = (0x0A << 26) | (1 << 25) | (2 << 20) | (1 << 15) |
                    (0 << 12) | (18 << 7) | 0x57;
    uint32_t vsub5 = (0x02 << 26) | (1 << 25) | (1 << 20) | (2 << 15) |
                     (0 << 12) | (19 << 7) | 0x57;
    core->loadInstruction(addr, vadd5);
    addr += 4;
    core->loadInstruction(addr, vand5);
    addr += 4;
    core->loadInstruction(addr, vor5);
    addr += 4;
    core->loadInstruction(addr, vsub5);
    addr += 4;

    // Second set with different source registers
    uint32_t vadd6 = (0x00 << 26) | (1 << 25) | (3 << 20) | (4 << 15) |
                     (0 << 12) | (20 << 7) | 0x57;
    uint32_t vand6 = (0x09 << 26) | (1 << 25) | (5 << 20) | (6 << 15) |
                     (0 << 12) | (21 << 7) | 0x57;
    uint32_t vor6 = (0x0A << 26) | (1 << 25) | (4 << 20) | (3 << 15) |
                    (0 << 12) | (22 << 7) | 0x57;
    uint32_t vsub6 = (0x02 << 26) | (1 << 25) | (6 << 20) | (5 << 15) |
                     (0 << 12) | (23 << 7) | 0x57;
    core->loadInstruction(addr, vadd6);
    addr += 4;
    core->loadInstruction(addr, vand6);
    addr += 4;
    core->loadInstruction(addr, vor6);
    addr += 4;
    core->loadInstruction(addr, vsub6);
    addr += 4;

    // Fill rest with NOPs
    while (addr < 256) {
      core->loadInstruction(addr, 0x13);  // ADDI x0, x0, 0 (NOP)
      addr += 4;
    }

    std::cout << "  Loaded " << (addr / 4) << " instructions (mixed NOP/vec)\n"
              << "  Test Blocks:" << std::endl;
    std::cout << "    Block 1 (0x0-0xf):   Independent ALU ops (4 instr)"
              << std::endl;
    std::cout << "    Block 2 (0x10-0x1f): RAW dependency chains (4 instr)"
              << std::endl;
    std::cout << "    Block 3 (0x20-0x2f): WAR hazards (4 instr)" << std::endl;
    std::cout << "    Block 4 (0x30-0x3f): WAW hazards (4 instr)" << std::endl;
    std::cout << "    Block 5 (0x40-0x5f): Multi-issue stress test (8 instr)"
              << std::endl;
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

    // Initialize input vector registers with comprehensive test data
    // v1-v6: Base operands
    // v7-v15: Will be overwritten by instructions
    // v16-v23: Output registers for stress test

    auto vrf = rvv->getVRF();
    if (vrf) {
      // Base operand vectors
      std::vector<uint8_t> v1_data = {10, 20, 30, 40, 50, 60, 70, 80};
      std::vector<uint8_t> v2_data = {5, 6, 7, 8, 9, 10, 11, 12};
      std::vector<uint8_t> v3_data = {1, 2, 3, 4, 5, 6, 7, 8};
      std::vector<uint8_t> v4_data = {2, 3, 4, 5, 6, 7, 8, 9};
      std::vector<uint8_t> v5_data = {15, 15, 15, 15, 15, 15, 15, 15};
      std::vector<uint8_t> v6_data = {0x0F, 0x0F, 0x0F, 0x0F,
                                      0x0F, 0x0F, 0x0F, 0x0F};

      vrf->write(1, v1_data);
      vrf->write(2, v2_data);
      vrf->write(3, v3_data);
      vrf->write(4, v4_data);
      vrf->write(5, v5_data);
      vrf->write(6, v6_data);

      std::cout << "\n  Vector Register File initialized:" << std::endl;
      std::cout << "    v1 = [10, 20, 30, 40, 50, 60, 70, 80]" << std::endl;
      std::cout << "    v2 = [5, 6, 7, 8, 9, 10, 11, 12]" << std::endl;
      std::cout << "    v3 = [1, 2, 3, 4, 5, 6, 7, 8]" << std::endl;
      std::cout << "    v4 = [2, 3, 4, 5, 6, 7, 8, 9]" << std::endl;
      std::cout << "    v5 = [15, 15, 15, 15, 15, 15, 15, 15]" << std::endl;
      std::cout << "    v6 = [0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F]"
                << std::endl;
    }

    std::cout << "\n  Expected Results Summary:" << std::endl;
    std::cout << "    Block 1 - Independent ops (no dependencies):"
              << std::endl;
    std::cout << "      v3=VADD(v1,v2)  → [15, 26, 37, 48, 59, 70, 81, 92]"
              << std::endl;
    std::cout << "      v4=VAND(v1,v2)  → bitwise AND results" << std::endl;
    std::cout << "      v5=VOR(v1,v2)   → bitwise OR results" << std::endl;
    std::cout << "      v6=VSUB(v2,v1)  → [-5, -14, -23, -32, ...]"
              << std::endl;
    std::cout << "    Block 2 - RAW chains (sequential execution):"
              << std::endl;
    std::cout << "      v7=VADD(v1,v2)  → [15, 26, 37, 48, ...]" << std::endl;
    std::cout << "      v8=VSUB(v7,v1)  → [5, 6, 7, 8, ...]" << std::endl;
    std::cout << "      v9=VAND(v8,v2)  → bitwise AND of result" << std::endl;
    std::cout << "      v10=VOR(v8,v1)  → bitwise OR of result" << std::endl;
    std::cout << "    Block 3 - WAR hazards (order-critical):" << std::endl;
    std::cout << "      Must preserve v1 value for v11 before updating v1"
              << std::endl;
    std::cout << "    Block 4 - WAW hazards (only last write visible):"
              << std::endl;
    std::cout << "      v14 final = VOR(v5,v6) (first two writes discarded)"
              << std::endl;
    std::cout << "    Block 5 - Multi-issue stress (high throughput):"
              << std::endl;
    std::cout << "      v16-v23: Multiple independent operations" << std::endl;
    std::cout << std::endl;
  }

  /**
   * @brief Run event-driven simulation with detailed timing trace
   *
   * Captures:
   * - Cycle-by-cycle pipeline activity
   * - ROB occupancy and utilization
   * - Stall events and their causes
   * - Multi-issue effectiveness
   */
  void runSimulation(EventScheduler& scheduler, RVVBackend& rvv) {
    std::cout
        << "Simulation: Event-driven execution with detailed timing trace\n"
        << std::endl;

    std::cout << "Initial pending events: " << scheduler.getPendingEventCount()
              << std::endl;

    const uint64_t MAX_CYCLES = 300;
    uint64_t cycles = 0;

    // Statistics tracking
    uint64_t total_decode = 0, total_dispatch = 0;
    uint64_t max_rob_occupancy = 0;
    uint32_t prev_decode = 0, prev_dispatch = 0, prev_execute = 0,
             prev_retire = 0;

    // Print trace header
    std::cout
        << "\n╔═════╦══════╦════════╦═════════╦═════════╦═════════╦═════════╗"
        << std::endl;
    std::cout
        << "║Cycle║Events│ROB Occ.║ Decode  ║Dispatch ║ Execute ║ Retire  ║"
        << std::endl;
    std::cout
        << "╠═════╬══════╬════════╬═════════╬═════════╬═════════╬═════════╣"
        << std::endl;

    while (scheduler.getPendingEventCount() > 0 && cycles < MAX_CYCLES) {
      uint64_t events_before = scheduler.getTotalEventCount();
      scheduler.runFor(1);

      // Get current stats
      uint64_t curr_decode = rvv.getDecodeCount();
      uint64_t curr_dispatch = rvv.getDispatchCount();
      uint64_t curr_execute = rvv.getExecuteCount();
      uint64_t curr_retire = rvv.getRetireCount();

      // Estimate ROB occupancy
      uint64_t rob_occupancy = (curr_dispatch - curr_retire);
      max_rob_occupancy = std::max(max_rob_occupancy, rob_occupancy);

      // Show if stage has activity
      std::string decode_str =
          (curr_decode > prev_decode)
              ? (std::to_string(curr_decode - prev_decode) + "*")
              : " ";
      std::string dispatch_str =
          (curr_dispatch > prev_dispatch)
              ? (std::to_string(curr_dispatch - prev_dispatch) + "*")
              : " ";
      std::string execute_str =
          (curr_execute > prev_execute)
              ? (std::to_string(curr_execute - prev_execute) + "*")
              : " ";
      std::string retire_str =
          (curr_retire > prev_retire)
              ? (std::to_string(curr_retire - prev_retire) + "*")
              : " ";

      // Print trace if activity or first 30 cycles
      if (scheduler.getTotalEventCount() > events_before || cycles < 30 ||
          curr_dispatch > prev_dispatch) {
        std::cout << "║ " << std::setw(3) << cycles << " ║ " << std::setw(4)
                  << scheduler.getTotalEventCount() << " ║ " << std::setw(6)
                  << rob_occupancy << " ║ " << std::setw(4) << std::left
                  << decode_str << "  ║ " << std::setw(4) << std::left
                  << dispatch_str << "  ║ " << std::setw(4) << std::left
                  << execute_str << "  ║ " << std::setw(4) << std::left
                  << retire_str << "  ║" << std::endl;

        prev_decode = curr_decode;
        prev_dispatch = curr_dispatch;
        prev_execute = curr_execute;
        prev_retire = curr_retire;
      }

      cycles++;
    }

    std::cout
        << "╚═════╩══════╩════════╩═════════╩═════════╩═════════╩═════════╝"
        << std::endl;
    std::cout << "\nSimulation Summary:" << std::endl;
    std::cout << "  Total Cycles: " << cycles << std::endl;
    std::cout << "  Total Events Processed: " << scheduler.getTotalEventCount()
              << std::endl;
    std::cout << "  Max ROB Occupancy: " << max_rob_occupancy << std::endl;
    std::cout << "\nLegend:" << std::endl;
    std::cout << "  Events = Pending events in scheduler" << std::endl;
    std::cout << "  ROB Occ. = Reorder buffer occupancy (dispatch-retire)"
              << std::endl;
    std::cout << "  N* = N instructions processed this cycle" << std::endl;
    std::cout << std::endl;
  }

  /**
   * @brief Show execution results and comprehensive statistics
   *
   * Analyzes:
   * - Pipeline throughput (IPC)
   * - Decode/dispatch/execute/retire bandwidth utilization
   * - ROB efficiency
   * - Hazard impact on performance
   */
  void showResults(RVVBackend& rvv) {
    std::cout << "═════════════════════════════════════════════════════════════"
              << std::endl;
    std::cout << "EXECUTION RESULTS AND ANALYSIS\n" << std::endl;

    // Gather statistics
    uint64_t decode_count = rvv.getDecodeCount();
    uint64_t dispatch_count = rvv.getDispatchCount();
    uint64_t execute_count = rvv.getExecuteCount();
    uint64_t retire_count = rvv.getRetireCount();
    uint64_t stall_count = rvv.getStallCount();

    std::cout << "1. PIPELINE STATISTICS" << std::endl;
    std::cout << "   ─────────────────────────────────────────" << std::endl;
    std::cout << "   Decode Count:       " << decode_count << " instructions"
              << std::endl;
    std::cout << "   Dispatch Count:     " << dispatch_count << " instructions"
              << std::endl;
    std::cout << "   Execute Count:      " << execute_count << " instructions"
              << std::endl;
    std::cout << "   Retire Count:       " << retire_count << " instructions"
              << std::endl;
    std::cout << "   Stall Count:        " << stall_count << " cycles"
              << std::endl;

    // Calculate throughput metrics
    if (decode_count > 0) {
      std::cout << "\n2. BANDWIDTH ANALYSIS" << std::endl;
      std::cout << "   ─────────────────────────────────────────" << std::endl;
      double decode_ratio = (double)dispatch_count / (double)decode_count;
      double dispatch_ratio = (double)execute_count / (double)dispatch_count;
      double retire_ratio = (double)retire_count / (double)execute_count;

      std::cout << std::fixed << std::setprecision(2);
      std::cout << "   Dispatch/Decode Rate:    " << decode_ratio
                << " (loss: " << (1.0 - decode_ratio) * 100 << "%)"
                << std::endl;
      std::cout << "   Execute/Dispatch Rate:   " << dispatch_ratio
                << " (loss: " << (1.0 - dispatch_ratio) * 100 << "%)"
                << std::endl;
      std::cout << "   Retire/Execute Rate:     " << retire_ratio
                << " (loss: " << (1.0 - retire_ratio) * 100 << "%)"
                << std::endl;

      if (dispatch_count > 0) {
        double effective_issue = (double)dispatch_count / 4.0;  // 4-wide issue
        std::cout << "   Effective Issue Width:   " << effective_issue
                  << "/4 lanes (" << effective_issue * 25.0 << "% utilization)"
                  << std::endl;
      }
    }

    // Vector configuration
    std::cout << "\n3. VECTOR CONFIGURATION" << std::endl;
    std::cout << "   ─────────────────────────────────────────" << std::endl;
    auto config = rvv.getConfigState();
    std::cout << "   VL (Vector Length):      " << config.vl << " elements"
              << std::endl;
    std::cout << "   VSTART:                  " << config.vstart << std::endl;
    std::cout << "   SEW (Selected Elem Width): "
              << static_cast<int>(config.sew) << " (8-bit)" << std::endl;
    std::cout << "   LMUL:                    " << static_cast<int>(config.lmul)
              << " (LMUL=1)" << std::endl;
    std::cout << "   MA (Mask Agnostic):      " << (config.ma ? "YES" : "NO")
              << std::endl;
    std::cout << "   TA (Tail Agnostic):      " << (config.ta ? "YES" : "NO")
              << std::endl;
    std::cout << "   VILL (Illegal):          "
              << (config.vill ? "ILLEGAL" : "VALID") << std::endl;
    std::cout << "   vtype CSR:               0x" << std::hex
              << config.getVtype() << std::dec << std::endl;

    // Dependency analysis
    std::cout << "\n4. DEPENDENCY ANALYSIS" << std::endl;
    std::cout << "   ─────────────────────────────────────────" << std::endl;
    std::cout << "   Test Block 1 (Independent ops):" << std::endl;
    std::cout << "     ✓ All 4 instructions should execute in parallel"
              << std::endl;
    std::cout << "     ✓ Expected: 1 decode cycle, 1 dispatch, 1 execute"
              << std::endl;
    std::cout << "\n   Test Block 2 (RAW dependencies):" << std::endl;
    std::cout << "     ✓ Sequential execution chain (v7→v8→v9, v7→v10)"
              << std::endl;
    std::cout << "     ✓ May stall dispatch/execute due to dependencies"
              << std::endl;
    std::cout << "\n   Test Block 3 (WAR hazards):" << std::endl;
    std::cout << "     ✓ v1 write must occur after all prior v1 reads"
              << std::endl;
    std::cout << "     ✓ Dispatch should serialize these instructions"
              << std::endl;
    std::cout << "\n   Test Block 4 (WAW hazards):" << std::endl;
    std::cout << "     ✓ Multiple writes to v14 - only last value visible"
              << std::endl;
    std::cout << "     ✓ ROB handles write ordering and discards stale data"
              << std::endl;
    std::cout << "\n   Test Block 5 (Multi-issue stress):" << std::endl;
    std::cout << "     ✓ 8 independent instructions back-to-back" << std::endl;
    std::cout << "     ✓ Tests sustained throughput with 4-lane issue"
              << std::endl;

    // ROB utilization
    std::cout << "\n5. ROB UTILIZATION" << std::endl;
    std::cout << "   ─────────────────────────────────────────" << std::endl;
    std::cout << "   ROB Capacity:            256 entries" << std::endl;
    std::cout << "   Max Occupancy:           (estimated from trace)"
              << std::endl;
    std::cout
        << "   Utilization:             Depends on pipeline depth & stalls"
        << std::endl;
    std::cout << "   WAW/WAR Resolution:      Handled by ROB aliasing"
              << std::endl;

    // Execution summary
    std::cout << "\n6. EXECUTION SUMMARY" << std::endl;
    std::cout << "   ─────────────────────────────────────────" << std::endl;
    std::cout << "   Total Instructions: " << retire_count << " retired"
              << std::endl;

    if (retire_count > 0) {
      std::cout << "   Instructions Retired:    " << retire_count << std::endl;
    }

    std::cout << "   Status: " << (rvv.isIdle() ? "✓ COMPLETE" : "✗ INCOMPLETE")
              << std::endl;

    std::cout << "\n7. ADVANCED METRICS" << std::endl;
    std::cout << "   ─────────────────────────────────────────" << std::endl;
    std::cout << "   Stripmining Expansions:  "
              << rvv.getStripminingExpansions() << std::endl;
    std::cout << "   Total UOPs Generated:    " << rvv.getTotalUopsGenerated()
              << std::endl;
    std::cout << "   Max UOPs/Cycle:          "
              << rvv.getMaxUopsPerCycleAchieved() << std::endl;
    std::cout << "   Current UOP Queue Size:  " << rvv.getCurrentUopQueueSize()
              << std::endl;

    std::cout << "\n" << std::string(61, '=') << std::endl;
    std::cout
        << "\nNote: Detailed trace analysis available in data/rvv_alu_trace.log"
        << std::endl;
    std::cout << "Run: cat data/rvv_alu_trace.log | grep -E "
                 "'Decode|Dispatch|Execute|Retire'"
              << std::endl;
    std::cout << "     to filter specific pipeline stages." << std::endl;

    std::cout << "\nKey Observations:" << std::endl;
    std::cout << "  • Independent instructions should execute in parallel"
              << std::endl;
    std::cout << "  • Dependent chains will show dispatch serialization"
              << std::endl;
    std::cout << "  • WAR/WAW hazards may cause additional stalls" << std::endl;
    std::cout << "  • Multi-issue block tests sustained 4-lane throughput"
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
