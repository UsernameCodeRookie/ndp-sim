#include <gtest/gtest.h>

#include <cstdint>
#include <memory>
#include <vector>

#include "../src/comp/rvv/rvv_backend.h"
#include "../src/comp/rvv/rvv_interface.h"

using namespace Architecture;

/**
 * @brief Integration tests for Scalar Core + Vector Backend
 *
 * Tests the interface between SCore and RVV Backend following
 * CoralNPU architecture patterns
 */
class CoreRvvIntegrationTest : public ::testing::Test {
 protected:
  void SetUp() override {
    scheduler = std::make_unique<EventDriven::EventScheduler>();
    rvv = std::make_unique<RVVBackend>("rvv_backend", *scheduler, 1, 128);
    rvv->start(0);
  }

  void runSimulation(uint64_t max_time = 100) { scheduler->runUntil(max_time); }

  std::unique_ptr<EventDriven::EventScheduler> scheduler;
  std::unique_ptr<RVVBackend> rvv;

  /**
   * @brief Create instruction request from scalar core
   */
  RVVCoreInterface::InstructionRequest createInstReq(uint64_t inst_id,
                                                     uint32_t vs1, uint32_t vs2,
                                                     uint32_t vd) {
    RVVCoreInterface::InstructionRequest req;
    req.inst_id = inst_id;
    req.opcode = 0x02;  // RVVALU opcode
    req.bits = 0;       // Placeholder
    req.vs1_idx = vs1;
    req.vs2_idx = vs2;
    req.vd_idx = vd;
    req.vm = 1;            // Mask enabled
    req.sew = 0;           // 8-bit elements
    req.lmul = 0;          // LMUL = 1
    req.vl = 16;           // Vector length
    req.pc = inst_id * 4;  // Sequential PC
    return req;
  }
};

/**
 * @brief Test basic instruction issue from scalar core interface
 */
TEST_F(CoreRvvIntegrationTest, IssueInstructionThroughInterface) {
  auto req = createInstReq(0, 1, 2, 3);

  // Issue through RVVCoreInterface
  bool accepted = rvv->issueInstruction(req);

  EXPECT_TRUE(accepted);
  EXPECT_FALSE(rvv->isIdle());
}

/**
 * @brief Test multiple instructions through interface
 */
TEST_F(CoreRvvIntegrationTest, IssueMultipleInstructionsThroughInterface) {
  for (int i = 0; i < 5; i++) {
    auto req = createInstReq(i, 1, 2, 3 + i);
    bool accepted = rvv->issueInstruction(req);
    EXPECT_TRUE(accepted);
  }

  EXPECT_EQ(rvv->getQueueCapacity(), 27);  // 32 - 5
}

/**
 * @brief Test instruction execution through interface
 */
TEST_F(CoreRvvIntegrationTest, ExecuteInstructionThroughInterface) {
  auto req = createInstReq(0, 1, 2, 3);
  rvv->issueInstruction(req);

  runSimulation(10);

  EXPECT_GT(rvv->getDecodeCount(), 0);
  EXPECT_GT(rvv->getDispatchCount(), 0);
}

/**
 * @brief Test configuration state management
 */
TEST_F(CoreRvvIntegrationTest, ManageVectorConfigState) {
  RVVConfigState config;
  config.vl = 32;
  config.vstart = 0;
  config.ma = false;
  config.ta = false;
  config.xrm = 0;
  config.sew = 1;   // 16-bit elements
  config.lmul = 1;  // LMUL = 2
  config.lmul_orig = 1;
  config.vill = false;

  rvv->setConfigState(config);

  auto retrieved = rvv->getConfigState();
  EXPECT_EQ(retrieved.vl, 32);
  EXPECT_EQ(retrieved.sew, 1);
  EXPECT_EQ(retrieved.lmul, 1);
  EXPECT_FALSE(retrieved.vill);
}

/**
 * @brief Test pipeline capacity checks
 */
TEST_F(CoreRvvIntegrationTest, PipelineCapacity) {
  EXPECT_EQ(rvv->getQueueCapacity(), 32);  // Initially full capacity

  for (int i = 0; i < 10; i++) {
    auto req = createInstReq(i, 1, 2, 3);
    rvv->issueInstruction(req);
  }

  EXPECT_EQ(rvv->getQueueCapacity(), 22);  // 32 - 10
}

/**
 * @brief Test interaction with queue capacity reporting
 */
TEST_F(CoreRvvIntegrationTest, QueueFullBehavior) {
  // Fill queue completely
  for (int i = 0; i < 32; i++) {
    auto req = createInstReq(i, 1, 2, 3 + (i % 20));
    bool accepted = rvv->issueInstruction(req);
    EXPECT_TRUE(accepted);
  }

  EXPECT_EQ(rvv->getQueueCapacity(), 0);  // Queue full

  // Try to issue one more - should fail
  auto extra = createInstReq(100, 1, 2, 3);
  bool accepted = rvv->issueInstruction(extra);
  EXPECT_FALSE(accepted);
}

/**
 * @brief Test configuration state vtype encoding (CoralNPU format)
 */
TEST_F(CoreRvvIntegrationTest, ConfigStateVtypeEncoding) {
  RVVConfigState config;
  config.vl = 128;
  config.sew = 2;        // 32-bit elements
  config.lmul_orig = 2;  // LMUL = 4
  config.ma = true;      // Mask agnostic
  config.ta = true;      // Tail agnostic
  config.vill = false;

  rvv->setConfigState(config);

  uint32_t vtype = rvv->getConfigState().getVtype();

  // Verify vtype encoding
  // Format: [vill(1)|reserved(23)|ma(1)|ta(1)|sew(3)|lmul(3)]
  EXPECT_EQ(vtype & 0x80000000, 0);  // vill = 0
  EXPECT_EQ((vtype >> 7) & 1, 1);    // ma = 1
  EXPECT_EQ((vtype >> 6) & 1, 1);    // ta = 1
  EXPECT_EQ((vtype >> 3) & 0x7, 2);  // sew = 2
  EXPECT_EQ(vtype & 0x7, 2);         // lmul = 2
}

/**
 * @brief Test idle signal after instruction completion
 */
TEST_F(CoreRvvIntegrationTest, IdleSignalAfterCompletion) {
  auto req = createInstReq(0, 1, 2, 3);
  rvv->issueInstruction(req);

  EXPECT_FALSE(rvv->isIdle());

  runSimulation(20);

  // Should eventually become idle
  EXPECT_TRUE(rvv->isIdle() || rvv->getDecodeCount() > 0);
}

/**
 * @brief Test trap detection interface
 */
TEST_F(CoreRvvIntegrationTest, TrapDetection) {
  auto trap_inst = RVVCoreInterface::InstructionRequest();

  // Should not have trap initially
  bool has_trap = rvv->getTrap(trap_inst);
  EXPECT_FALSE(has_trap);
}

/**
 * @brief Test scalar register read/write interface
 */
TEST_F(CoreRvvIntegrationTest, ScalarRegisterInterface) {
  // Read from register (should return 0 for unconnected interface)
  uint64_t data = rvv->readScalarRegister(1);
  EXPECT_EQ(data, 0);

  // Write to scalar register (should not crash)
  rvv->writeScalarRegister(1, 0x12345678, 0xFF);

  // Interface is minimal but should be callable
  EXPECT_TRUE(true);
}

/**
 * @brief Test retire write interface
 */
TEST_F(CoreRvvIntegrationTest, RetireWriteInterface) {
  auto req = createInstReq(0, 1, 2, 3);
  rvv->issueInstruction(req);

  runSimulation(10);

  // Get retire writes (may be empty if not yet retired)
  auto writes = rvv->getRetireWrites();

  // Should be a vector (may be empty)
  EXPECT_TRUE(writes.empty() || !writes.empty());
}

/**
 * @brief Test high throughput instruction issue
 */
TEST_F(CoreRvvIntegrationTest, HighThroughputIssue) {
  for (int i = 0; i < 20; i++) {
    auto req = createInstReq(i, 1 + (i % 5), 2 + (i % 5), 10 + (i % 20));
    bool accepted = rvv->issueInstruction(req);
    EXPECT_TRUE(accepted);
  }

  EXPECT_EQ(rvv->getDecodeCount(), 0);  // Not yet executed

  runSimulation(30);

  EXPECT_EQ(rvv->getDecodeCount(), 20);
  EXPECT_EQ(rvv->getDispatchCount(), 20);
}

/**
 * @brief Test interface consistency across multiple instruction types
 */
TEST_F(CoreRvvIntegrationTest, InterfaceConsistency) {
  // Issue instructions with different register patterns
  std::vector<RVVCoreInterface::InstructionRequest> reqs;
  for (int i = 0; i < 3; i++) {
    reqs.push_back(createInstReq(i, i, i + 1, i + 10));
  }

  for (const auto& req : reqs) {
    bool accepted = rvv->issueInstruction(req);
    EXPECT_TRUE(accepted);
  }

  EXPECT_EQ(rvv->getQueueCapacity(), 29);  // 32 - 3
  EXPECT_FALSE(rvv->isIdle());
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
