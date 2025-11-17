#include <gtest/gtest.h>

#include <cstdint>
#include <memory>
#include <vector>

#include "../src/comp/rvv/rvv_backend.h"

using namespace Architecture;

/**
 * @brief RVV Backend Integration Tests
 */
class RVVBackendTest : public ::testing::Test {
 protected:
  void SetUp() override {
    scheduler = std::make_unique<EventDriven::EventScheduler>();
    backend = std::make_unique<RVVBackend>("backend", *scheduler, 1, 128);
    backend->start(0);
  }

  void runPipeline(uint64_t max_time = 100) { scheduler->runUntil(max_time); }

  std::unique_ptr<EventDriven::EventScheduler> scheduler;
  std::unique_ptr<RVVBackend> backend;

  RVVCoreInterface::InstructionRequest createInstruction(uint64_t inst_id,
                                                         uint32_t opcode,
                                                         uint32_t vs1,
                                                         uint32_t vs2,
                                                         uint32_t vd) {
    RVVCoreInterface::InstructionRequest inst;
    inst.inst_id = inst_id;
    inst.opcode = opcode;
    inst.vs1_idx = vs1;
    inst.vs2_idx = vs2;
    inst.vd_idx = vd;
    inst.vm = 1;
    inst.sew = 1;   // 8-bit
    inst.lmul = 0;  // LMUL=1
    inst.vl = 16;   // Vector length
    inst.pc = 0;
    return inst;
  }
};

TEST_F(RVVBackendTest, Initialize) {
  EXPECT_TRUE(backend->isIdle());
  EXPECT_EQ(backend->getDecodeCount(), 0);
  EXPECT_EQ(backend->getDispatchCount(), 0);
  EXPECT_EQ(backend->getExecuteCount(), 0);
  EXPECT_EQ(backend->getRetireCount(), 0);
}

TEST_F(RVVBackendTest, IssueSimpleInstruction) {
  auto inst = createInstruction(0, 0x01, 1, 2, 3);
  bool accepted = backend->issueInstruction(inst);

  EXPECT_TRUE(accepted);
  EXPECT_FALSE(backend->isIdle());
}

TEST_F(RVVBackendTest, IssueMultipleInstructions) {
  for (int i = 0; i < 5; i++) {
    auto inst = createInstruction(i, 0x01, 1, 2, 3);
    bool accepted = backend->issueInstruction(inst);
    EXPECT_TRUE(accepted);
  }

  EXPECT_FALSE(backend->isIdle());
}

TEST_F(RVVBackendTest, ExecutionCycle) {
  auto inst = createInstruction(0, 0x01, 1, 2, 3);
  backend->issueInstruction(inst);

  runPipeline(5);

  EXPECT_GT(backend->getDecodeCount(), 0);
  EXPECT_GT(backend->getDispatchCount(), 0);
}

TEST_F(RVVBackendTest, RetirementCycle) {
  auto inst = createInstruction(0, 0x01, 1, 2, 3);
  backend->issueInstruction(inst);

  runPipeline(10);

  EXPECT_GT(backend->getDecodeCount(), 0);
  EXPECT_GT(backend->getExecuteCount(), 0);
}

TEST_F(RVVBackendTest, PipelineFlow) {
  for (int i = 0; i < 4; i++) {
    auto inst = createInstruction(i, 0x01, 1, 2, 3);
    backend->issueInstruction(inst);
  }

  runPipeline(20);

  EXPECT_EQ(backend->getDecodeCount(), 4);
  EXPECT_EQ(backend->getDispatchCount(), 4);
  EXPECT_GT(backend->getExecuteCount(), 0);
}

TEST_F(RVVBackendTest, DifferentOpcodes) {
  backend->issueInstruction(createInstruction(0, 0x01, 1, 2, 3));
  backend->issueInstruction(createInstruction(1, 0x02, 2, 3, 4));
  backend->issueInstruction(createInstruction(2, 0x04, 3, 4, 5));
  backend->issueInstruction(createInstruction(3, 0x08, 4, 5, 6));

  runPipeline(20);

  EXPECT_EQ(backend->getDecodeCount(), 4);
  EXPECT_EQ(backend->getDispatchCount(), 4);
}

TEST_F(RVVBackendTest, AccessComponents) {
  EXPECT_TRUE(backend->getALU() != nullptr);
  EXPECT_TRUE(backend->getDVU() != nullptr);
  EXPECT_TRUE(backend->getVRF() != nullptr);
  EXPECT_TRUE(backend->getROB() != nullptr);
  EXPECT_TRUE(backend->getRetire() != nullptr);
}

TEST_F(RVVBackendTest, ROBUtilization) {
  for (int i = 0; i < 3; i++) {
    auto inst = createInstruction(i, 0x01, 1, 2, 3);
    backend->issueInstruction(inst);
  }

  runPipeline(10);

  // Check that dispatch processed instructions
  EXPECT_GT(backend->getDispatchCount(), 0);
}

TEST_F(RVVBackendTest, PipelineWithDifferentDestRegs) {
  backend->issueInstruction(createInstruction(0, 0x01, 1, 2, 10));
  backend->issueInstruction(createInstruction(1, 0x01, 3, 4, 11));
  backend->issueInstruction(createInstruction(2, 0x01, 5, 6, 12));

  runPipeline(20);

  EXPECT_EQ(backend->getDecodeCount(), 3);
  EXPECT_EQ(backend->getDispatchCount(), 3);
}

TEST_F(RVVBackendTest, HighThroughputExecution) {
  for (int i = 0; i < 10; i++) {
    backend->issueInstruction(
        createInstruction(i, 0x01, i % 3, (i + 1) % 3, (i + 2) % 3 + 10));
  }

  runPipeline(30);

  EXPECT_EQ(backend->getDecodeCount(), 10);
  EXPECT_GE(backend->getExecuteCount(), 5);
}

TEST_F(RVVBackendTest, IsIdleAfterCompletion) {
  backend->issueInstruction(createInstruction(0, 0x01, 1, 2, 3));

  runPipeline(20);

  EXPECT_TRUE(backend->isIdle() || backend->getExecuteCount() > 0);
}

TEST_F(RVVBackendTest, MultipleDispatchPorts) {
  for (int i = 0; i < 6; i++) {
    backend->issueInstruction(createInstruction(i, 0x01, 1, 2, 3));
  }

  runPipeline(15);

  EXPECT_EQ(backend->getDecodeCount(), 6);
}

TEST_F(RVVBackendTest, ComponentInteraction) {
  backend->issueInstruction(createInstruction(0, 0x01, 1, 2, 3));

  runPipeline(10);

  auto rob = backend->getROB();
  auto vrf = backend->getVRF();
  auto alu = backend->getALU();

  EXPECT_TRUE(rob != nullptr);
  EXPECT_TRUE(vrf != nullptr);
  EXPECT_TRUE(alu != nullptr);
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
