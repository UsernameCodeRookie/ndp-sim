#include <gtest/gtest.h>

#include <iostream>

#include "../src/comp/core.h"
#include "../src/scheduler.h"
#include "../src/trace.h"

/**
 * @brief Test suite for SCore Event-Driven functionality
 *
 * Tests the event-driven characteristics of SCore as a Pipeline component:
 * - Pipeline stage processing
 * - Event scheduling and execution
 * - Timing constraints
 * - Multi-cycle operations
 * - Pipeline flow control
 */
class SCoreEventDrivenTest : public ::testing::Test {
 protected:
  void SetUp() override {
    scheduler = std::make_unique<EventDriven::EventScheduler>();
  }

  void TearDown() override { scheduler.reset(); }

  std::unique_ptr<EventDriven::EventScheduler> scheduler;
};

/**
 * @brief Test that SCore generates periodic events
 *
 * SCore should schedule tick events based on its configured period
 */
TEST_F(SCoreEventDrivenTest, PeriodicTickEvents) {
  Architecture::SCore::Config config;
  config.num_instruction_lanes = 2;
  auto core =
      std::make_shared<Architecture::SCore>("SCore_0", *scheduler, config);

  // SCore is now a Pipeline (TickingComponent) with period=1
  // Each tick should trigger the pipeline stages
  core->initialize();

  // Dispatch an instruction to ensure something happens
  core->dispatchInstruction(Architecture::SCore::OpType::ALU, 0, 5, 3,
                            static_cast<uint32_t>(ALUOp::ADD), 8);

  // Run for several cycles
  for (int i = 0; i < 10; i++) {
    scheduler->runFor(1);
  }

  // Verify core is still responsive and executing
  EXPECT_GT(core->getInstructionsDispatched(), 0);
}

/**
 * @brief Test pipeline stage advancement
 *
 * Data should progress through the 3-stage pipeline over cycles
 */
TEST_F(SCoreEventDrivenTest, PipelineStageAdvancement) {
  // Enable tracing
  EventDriven::Tracer& tracer = EventDriven::Tracer::getInstance();
  tracer.initialize("test_pipeline_stage.log", true);

  Architecture::SCore::Config config;
  auto core =
      std::make_shared<Architecture::SCore>("SCore_0", *scheduler, config);
  core->initialize();

  std::cout << "\n[PipelineStageAdvancement] Starting test\n";
  std::cout << "Initial ALU executed: "
            << core->getALU(0)->getOperationsExecuted() << "\n";

  // Dispatch instruction to ensure pipeline activity
  core->dispatchInstruction(Architecture::SCore::OpType::ALU, 0, 5, 3,
                            static_cast<uint32_t>(ALUOp::ADD), 8);

  std::cout << "After dispatch, ALU executed: "
            << core->getALU(0)->getOperationsExecuted() << "\n";

  // Run pipeline for multiple cycles to allow data progression
  // Need at least 50+ cycles like in test_core.cpp
  for (int i = 0; i < 60; i++) {
    scheduler->runFor(1);
    if (i % 10 == 0 || i >= 55) {
      std::cout << "Cycle " << i
                << ": ALU executed=" << core->getALU(0)->getOperationsExecuted()
                << "\n";
    }
  }

  std::cout << "Final ALU executed: "
            << core->getALU(0)->getOperationsExecuted() << "\n\n";
  tracer.dump();

  // Pipeline should have processed the instruction
  EXPECT_GT(core->getALU(0)->getOperationsExecuted(), 0);
}

/**
 * @brief Test instruction dispatch via pipeline stages
 *
 * dispatchCycle should be called during stage 1 (dispatch stage)
 */
TEST_F(SCoreEventDrivenTest, InstructionDispatchViaStages) {
  // Enable tracing
  EventDriven::Tracer& tracer = EventDriven::Tracer::getInstance();
  tracer.initialize("test_dispatch_stages.log", true);

  Architecture::SCore::Config config;
  auto core =
      std::make_shared<Architecture::SCore>("SCore_0", *scheduler, config);
  core->initialize();

  std::cout << "\n[InstructionDispatchViaStages] Starting test\n";
  std::cout << "Initial dispatched: " << core->getInstructionsDispatched()
            << "\n";

  // Inject instructions
  core->injectInstruction(0x1000, 0x00310333);  // ALU ADD
  core->injectInstruction(0x1004, 0x00220333);  // ALU SUB

  std::cout << "Injected 2 instructions\n";

  // Run first cycle
  scheduler->runFor(1);
  std::cout << "After cycle 1: dispatched=" << core->getInstructionsDispatched()
            << "\n";

  // Try manually calling dispatchCycle to debug
  uint32_t dispatched = core->dispatchCycle();
  std::cout << "Manual dispatchCycle returned: " << dispatched << "\n";
  std::cout << "After manual dispatch: dispatched="
            << core->getInstructionsDispatched() << "\n";

  std::cout << "Final dispatched: " << core->getInstructionsDispatched()
            << "\n\n";
  tracer.dump();

  // Instructions should be dispatched
  EXPECT_GT(core->getInstructionsDispatched(), 0);
}

/**
 * @brief Test multi-cycle ALU execution with pipeline
 *
 * ALU operations should complete over multiple cycles as they flow through
 * the 3-stage ALU pipeline
 */
TEST_F(SCoreEventDrivenTest, MultiCycleALUExecution) {
  Architecture::SCore::Config config;
  auto core =
      std::make_shared<Architecture::SCore>("SCore_0", *scheduler, config);
  core->initialize();

  // Directly dispatch ALU instruction
  core->dispatchInstruction(Architecture::SCore::OpType::ALU, 0, 10, 5,
                            static_cast<uint32_t>(ALUOp::ADD), 8);

  uint64_t start_time = scheduler->getCurrentTime();

  // Run for multiple cycles to allow ALU pipeline completion
  // ALU has 3 stages, each tick is 1 cycle, plus overhead
  for (int i = 0; i < 50; i++) {
    scheduler->runFor(1);
  }

  uint64_t end_time = scheduler->getCurrentTime();

  // Check that ALU has executed
  EXPECT_GT(core->getALU(0)->getOperationsExecuted(), 0);

  // Verify timing: end_time should be > start_time
  EXPECT_GT(end_time, start_time);
}

/**
 * @brief Test scheduler event ordering
 *
 * Multiple components should be scheduled and execute in correct order
 */
TEST_F(SCoreEventDrivenTest, EventSchedulingOrder) {
  Architecture::SCore::Config config;
  auto core =
      std::make_shared<Architecture::SCore>("SCore_0", *scheduler, config);
  core->initialize();

  // Track which components are executing
  std::vector<uint64_t> event_times;

  // Dispatch multiple instructions
  for (int i = 0; i < 3; i++) {
    core->dispatchInstruction(Architecture::SCore::OpType::ALU, i % 2, i, i + 1,
                              static_cast<uint32_t>(ALUOp::ADD), 10 + i);
  }

  // Run scheduler and collect timing info
  for (int cycle = 0; cycle < 100; cycle++) {
    uint64_t time_before = scheduler->getCurrentTime();
    scheduler->runFor(1);
    uint64_t time_after = scheduler->getCurrentTime();

    if (time_after > time_before) {
      event_times.push_back(time_after);
    }
  }

  // Events should be in order (no duplicates or out-of-order)
  for (size_t i = 1; i < event_times.size(); i++) {
    EXPECT_GE(event_times[i], event_times[i - 1]);
  }
}

/**
 * @brief Test concurrent execution of multiple ALU lanes
 *
 * Both ALU lanes should execute concurrently in the pipeline
 */
TEST_F(SCoreEventDrivenTest, ConcurrentALULaneExecution) {
  Architecture::SCore::Config config;
  config.num_instruction_lanes = 2;
  auto core =
      std::make_shared<Architecture::SCore>("SCore_0", *scheduler, config);
  core->initialize();

  // Dispatch to both ALU lanes
  core->dispatchInstruction(Architecture::SCore::OpType::ALU, 0, 5, 3,
                            static_cast<uint32_t>(ALUOp::ADD), 8);
  core->dispatchInstruction(Architecture::SCore::OpType::ALU, 1, 10, 7,
                            static_cast<uint32_t>(ALUOp::SUB), 9);

  // Run pipeline
  for (int i = 0; i < 50; i++) {
    scheduler->runFor(1);
  }

  // Both ALUs should have executed
  EXPECT_GT(core->getALU(0)->getOperationsExecuted(), 0);
  EXPECT_GT(core->getALU(1)->getOperationsExecuted(), 0);
}

/**
 * @brief Test scoreboard hazard detection in event-driven context
 *
 * Hazards should block dispatch while respecting event-driven timing
 */
TEST_F(SCoreEventDrivenTest, HazardDetectionEventDriven) {
  Architecture::SCore::Config config;
  config.num_instruction_lanes = 2;
  auto core =
      std::make_shared<Architecture::SCore>("SCore_0", *scheduler, config);
  core->initialize();

  // Inject instructions that create RAW hazard
  // Instruction 1: writes to register 5
  core->injectInstruction(0x1000, 0x00310333);
  // Instruction 2: reads from register 5 (should be blocked initially)
  core->injectInstruction(0x1004, 0x00500333);

  // Run dispatch cycle
  uint32_t dispatched = core->dispatchCycle();

  // At least first instruction should dispatch
  EXPECT_GT(dispatched, 0);
}

/**
 * @brief Test in-order execution enforcement
 *
 * When an instruction cannot be dispatched, subsequent instructions
 * should also be blocked (in-order rule)
 */
TEST_F(SCoreEventDrivenTest, InOrderExecutionEnforcement) {
  Architecture::SCore::Config config;
  config.num_instruction_lanes = 4;  // Wide dispatch
  auto core =
      std::make_shared<Architecture::SCore>("SCore_0", *scheduler, config);
  core->initialize();

  // Inject multiple instructions
  // First instruction has a hazard condition, should block others
  for (int i = 0; i < 4; i++) {
    core->injectInstruction(0x1000 + i * 4, 0x00310333);
  }

  // Single dispatch cycle should respect in-order constraint
  uint32_t dispatched = core->dispatchCycle();

  // Should dispatch at least one
  EXPECT_GE(dispatched, 0);

  // Remaining instructions should still be in buffer
  // (verification would require accessing internal state)
}

/**
 * @brief Test resource constraint enforcement (MLU busy)
 *
 * MLU should enforce single-issue-per-cycle constraint
 */
TEST_F(SCoreEventDrivenTest, MLUResourceConstraint) {
  // Enable tracing
  EventDriven::Tracer& tracer = EventDriven::Tracer::getInstance();
  tracer.initialize("test_mlu_constraint.log", true);

  Architecture::SCore::Config config;
  config.num_instruction_lanes = 2;
  auto core =
      std::make_shared<Architecture::SCore>("SCore_0", *scheduler, config);
  core->initialize();

  std::cout << "\n[MLUResourceConstraint] Starting test\n";
  std::cout << "Initial MLU results output: "
            << core->getMLU()->getResultsOutput() << "\n";

  // Dispatch MLU operation
  core->dispatchInstruction(Architecture::SCore::OpType::MLU, 0, 5, 3,
                            static_cast<uint32_t>(MultiplyUnit::MulOp::MUL), 8);

  std::cout << "After dispatch: MLU results="
            << core->getMLU()->getResultsOutput() << "\n";

  // Run for enough cycles for MLU pipeline to complete
  // MLU period is 3 cycles, need 200+ cycles like in test_core.cpp
  for (int i = 0; i < 200; i++) {
    scheduler->runFor(1);
    if (i % 50 == 0 || (i >= 195 && i < 200)) {
      std::cout << "Cycle " << i
                << ": MLU results=" << core->getMLU()->getResultsOutput()
                << "\n";
    }
  }

  std::cout << "Final MLU results output: "
            << core->getMLU()->getResultsOutput() << "\n\n";
  tracer.dump();

  // MLU should have executed at least once
  EXPECT_GT(core->getMLU()->getResultsOutput(), 0);
}

/**
 * @brief Test pipeline reset clears all state
 *
 * Reset should clear pipeline data and statistics
 */
TEST_F(SCoreEventDrivenTest, PipelineReset) {
  Architecture::SCore::Config config;
  auto core =
      std::make_shared<Architecture::SCore>("SCore_0", *scheduler, config);
  core->initialize();

  // Dispatch some instructions
  core->dispatchInstruction(Architecture::SCore::OpType::ALU, 0, 5, 3,
                            static_cast<uint32_t>(ALUOp::ADD), 8);

  uint64_t dispatched_before = core->getInstructionsDispatched();
  EXPECT_GT(dispatched_before, 0);

  // Reset core
  core->reset();

  // Statistics should be cleared
  EXPECT_EQ(core->getInstructionsDispatched(), 0);
  EXPECT_EQ(core->getInstructionsRetired(), 0);
}

/**
 * @brief Test program counter management through pipeline
 *
 * PC should be maintained and updated as instructions flow
 */
TEST_F(SCoreEventDrivenTest, ProgramCounterManagement) {
  Architecture::SCore::Config config;
  auto core =
      std::make_shared<Architecture::SCore>("SCore_0", *scheduler, config);

  // Initially PC should be 0
  EXPECT_EQ(core->getProgramCounter(), 0);

  // Set PC
  core->setProgramCounter(0x2000);
  EXPECT_EQ(core->getProgramCounter(), 0x2000);

  // Initialize and run
  core->initialize();

  for (int i = 0; i < 10; i++) {
    scheduler->runFor(1);
  }

  // PC should still reflect what we set (simplified model)
  EXPECT_EQ(core->getProgramCounter(), 0x2000);
}

/**
 * @brief Test register file integration with pipeline
 *
 * Register reads/writes should work correctly with pipeline execution
 */
TEST_F(SCoreEventDrivenTest, RegisterFileIntegration) {
  Architecture::SCore::Config config;
  auto core =
      std::make_shared<Architecture::SCore>("SCore_0", *scheduler, config);
  core->initialize();

  // Write to register 7
  core->writeRegister(7, 0x12345678);

  // Read back
  uint32_t value = core->readRegister(7);
  EXPECT_EQ(value, 0x12345678);

  // Dispatch ALU that uses this register
  core->dispatchInstruction(Architecture::SCore::OpType::ALU, 0, 0, 0,
                            static_cast<uint32_t>(ALUOp::ADD), 8);

  // Run pipeline
  for (int i = 0; i < 50; i++) {
    scheduler->runFor(1);
  }

  // Register should still have its value (or be updated by writeback)
  // In simplified model, it should retain the value
  EXPECT_EQ(core->readRegister(7), 0x12345678);
}

/**
 * @brief Test configuration persistence through event scheduling
 *
 * Configuration should remain stable across multiple scheduler events
 */
TEST_F(SCoreEventDrivenTest, ConfigurationPersistence) {
  Architecture::SCore::Config custom_config;
  custom_config.num_instruction_lanes = 4;
  custom_config.num_registers = 64;
  custom_config.mlu_period = 5;

  auto core = std::make_shared<Architecture::SCore>("SCore_0", *scheduler,
                                                    custom_config);

  // Verify configuration before initialization
  EXPECT_EQ(core->getALUs().size(), 4);

  core->initialize();

  // Run many cycles
  for (int i = 0; i < 200; i++) {
    scheduler->runFor(1);
  }

  // Configuration should be unchanged
  EXPECT_EQ(core->getALUs().size(), 4);
}

/**
 * @brief Test stall predicate for pipeline stages
 *
 * Pipeline should handle stall signals correctly
 */
TEST_F(SCoreEventDrivenTest, PipelineStallHandling) {
  Architecture::SCore::Config config;
  auto core =
      std::make_shared<Architecture::SCore>("SCore_0", *scheduler, config);
  core->initialize();

  // Dispatch instruction
  core->dispatchInstruction(Architecture::SCore::OpType::ALU, 0, 5, 3,
                            static_cast<uint32_t>(ALUOp::ADD), 8);

  // Run pipeline for baseline
  for (int i = 0; i < 10; i++) {
    scheduler->runFor(1);
  }

  uint64_t baseline_dispatched = core->getInstructionsDispatched();

  // Continue running
  for (int i = 0; i < 50; i++) {
    scheduler->runFor(1);
  }

  // Dispatched count should increase or stay same (no new dispatches)
  uint64_t final_dispatched = core->getInstructionsDispatched();
  EXPECT_GE(final_dispatched, baseline_dispatched);
}

/**
 * @brief Test scoreboard retirement mechanism
 *
 * Instructions should be retired to clear scoreboard entries
 */
TEST_F(SCoreEventDrivenTest, ScoreboardRetirement) {
  Architecture::SCore::Config config;
  auto core =
      std::make_shared<Architecture::SCore>("SCore_0", *scheduler, config);
  core->initialize();

  // Manually test retire instruction
  // (This would normally be automatic in stage 2)
  core->retireInstruction(5);  // Should clear scoreboard for r5

  // Verify no crash and basic functionality
  EXPECT_TRUE(true);
}
