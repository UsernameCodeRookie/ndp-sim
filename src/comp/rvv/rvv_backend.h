#ifndef RVV_BACKEND_H
#define RVV_BACKEND_H

#include <cstdint>
#include <deque>
#include <memory>
#include <queue>
#include <sstream>
#include <string>
#include <vector>

#include "../../component.h"
#include "../../event.h"
#include "../../packet.h"
#include "../../pipeline.h"
#include "../../port.h"
#include "../../scheduler.h"
#include "../../tick.h"
#include "../../trace.h"
#include "rvv_alu.h"
#include "rvv_decoder.h"
#include "rvv_dispatch.h"
#include "rvv_dvu.h"
#include "rvv_interface.h"
#include "rvv_regfile.h"
#include "rvv_retire.h"
#include "rvv_rob.h"

namespace Architecture {

/**
 * @brief RVV Backend (RISC-V Vector Backend)
 *
 * Pipeline-based vector execution backend with stages:
 * 1. Dispatch: Instruction → Micro-op conversion + ROB allocation (via
 * RVVDispatchStage)
 * 2. Execute: ALU/DIV operations (parallel)
 * 3. Retire: Writeback + WAW resolution
 *
 * Architecture:
 * - Multi-issue: up to 6 decode/dispatch per cycle (via RVVDispatchStage)
 * - Out-of-order execution with in-order retirement (ROB-based)
 * - Write forwarding through ROB for hazard resolution
 * - Multi-port flop-based vector register file
 * - Event-driven architecture: dispatch outputs through wire connections to
 * ALU/DVU
 *
 * Implements RVVCoreInterface for integration with Scalar Core
 */
class RVVBackend : public Pipeline, public RVVCoreInterface {
 public:
  /**
   * @brief Constructor
   *
   * @param name Component name
   * @param scheduler Event scheduler
   * @param period Clock period
   * @param vlen Vector length in bits (128/256/512, default 256 for CoralNPU)
   */
  RVVBackend(const std::string& name, EventDriven::EventScheduler& scheduler,
             uint64_t period = 1, uint32_t vlen = 256)
      : Pipeline(name, scheduler, period,
                 3),  // 3 pipeline stages: dispatch, execute, retire
        vlen_(vlen),
        num_decode_ports_(6),
        num_dispatch_ports_(4),
        num_execute_ports_(4),
        num_retire_ports_(4),
        decode_count_(0),
        dispatch_count_(0),
        execute_count_(0),
        retire_count_(0),
        stall_count_(0) {
    // Create dispatch stage as a Stage component integrated into the Pipeline
    dispatch_stage_ = std::make_shared<RVVDispatchStage>(
        name + "_dispatch", scheduler, period, vlen,
        4,   // num_read_ports
        4,   // max_issue_width (4 uops per cycle)
        256  // rob_size
    );

    // Create internal components
    alu_ = std::make_unique<RVVVectorALU>(name + "_alu", scheduler, vlen);
    dvu_ = std::make_unique<RVVVectorDVU>(name + "_dvu", scheduler, vlen);
    vrf_ = std::make_unique<RVVVectorRegisterFile>(name + "_vrf", scheduler, 1,
                                                   vlen);
    rob_ = std::make_unique<RVVReorderBuffer>(name + "_rob", scheduler, period,
                                              256, vlen);
    retire_ = std::make_unique<RVVRetireStage>(name + "_retire", scheduler,
                                               vlen, num_retire_ports_);

    // Create dispatch and execute buffer ports for wire connections
    // Dispatch stage outputs uops through these ports to functional units
    addPort("dispatch_to_alu", Architecture::PortDirection::OUTPUT);
    addPort("dispatch_to_dvu", Architecture::PortDirection::OUTPUT);

    // Execute stage receives results from functional units through these ports
    addPort("alu_result", Architecture::PortDirection::INPUT);
    addPort("dvu_result", Architecture::PortDirection::INPUT);

    // Register dispatch stage as Pipeline stage 0
    setStage(0, dispatch_stage_);

    // Configure remaining pipeline stages (stage 1 = execute, stage 2 = retire)
    setupExecuteStage();
    setupRetireStage();
  }

  /**
   * @brief Access internal components (for testing/debugging)
   */
  RVVVectorALU* getALU() { return alu_.get(); }
  RVVVectorDVU* getDVU() { return dvu_.get(); }
  RVVVectorRegisterFile* getVRF() { return vrf_.get(); }
  RVVReorderBuffer* getROB() { return rob_.get(); }
  RVVRetireStage* getRetire() { return retire_.get(); }

  /**
   * @brief Get pipeline statistics
   */
  uint64_t getDecodeCount() const { return decode_count_; }
  uint64_t getDispatchCount() const {
    // Return the actual dispatch count from dispatch stage
    return dispatch_stage_ ? dispatch_stage_->getDispatchCount()
                           : dispatch_count_;
  }
  uint64_t getExecuteCount() const { return execute_count_; }
  uint64_t getRetireCount() const { return retire_count_; }
  uint64_t getStallCount() const { return stall_count_; }

  /**
   * @brief Get stripmining statistics
   */
  uint64_t getStripminingExpansions() const { return stripmining_expansions_; }
  uint64_t getTotalUopsGenerated() const { return total_uops_generated_; }
  size_t getCurrentUopQueueSize() const {
    return dispatch_stage_ ? dispatch_stage_->getPendingUopCount() : 0;
  }
  uint64_t getMaxUopsPerCycleAchieved() const {
    return max_uops_per_cycle_achieved_;
  }

  /**
   * @brief Check if backend is idle
   */
  bool isIdle() const { return in_flight_uops_.empty() && rob_->isEmpty(); }

  // ========================================================================
  // RVVCoreInterface Implementation
  // ========================================================================

  /**
   * @brief Issue instruction from scalar core
   *
   * Convert RVVCoreInterface::InstructionRequest to internal RVVInstruction
   * and queue it to the dispatch stage
   */
  bool issueInstruction(
      const RVVCoreInterface::InstructionRequest& inst_req) override {
    // Create instruction and queue to dispatch stage
    RVVInstruction inst;
    inst.inst_id = inst_req.inst_id;
    inst.opcode = inst_req.opcode;
    inst.vs1_idx = inst_req.vs1_idx;
    inst.vs2_idx = inst_req.vs2_idx;
    inst.vd_idx = inst_req.vd_idx;
    inst.vm = inst_req.vm;
    inst.sew = inst_req.sew;
    inst.lmul = inst_req.lmul;
    inst.vl = inst_req.vl;
    inst.pc = inst_req.pc;

    // Queue to dispatch stage
    if (dispatch_stage_ && dispatch_stage_->queueInstruction(inst)) {
      return true;
    }
    return false;
  }

  /**
   * @brief Read scalar register from scalar core
   */
  uint64_t readScalarRegister(uint32_t addr) const override {
    // TODO: Connect to scalar core register file when integrated
    return 0;
  }

  /**
   * @brief Write result to scalar register
   */
  void writeScalarRegister(uint32_t addr, uint64_t data, uint8_t) override {
    // TODO: Route to scalar core register file
  }

  /**
   * @brief Get current vector configuration state
   */
  RVVConfigState getConfigState() const override {
    RVVConfigState state;
    state.vl = config_state_.vl;
    state.vstart = config_state_.vstart;
    state.ma = config_state_.ma;
    state.ta = config_state_.ta;
    state.xrm = config_state_.xrm;
    state.sew = config_state_.sew;
    state.lmul = config_state_.lmul;
    state.lmul_orig = config_state_.lmul_orig;
    state.vill = config_state_.vill;
    return state;
  }

  /**
   * @brief Update vector configuration state
   */
  void setConfigState(const RVVConfigState& config) override {
    config_state_.vl = config.vl;
    config_state_.vstart = config.vstart;
    config_state_.ma = config.ma;
    config_state_.ta = config.ta;
    config_state_.xrm = config.xrm;
    config_state_.sew = config.sew;
    config_state_.lmul = config.lmul;
    config_state_.lmul_orig = config.lmul_orig;
    config_state_.vill = config.vill;
  }

  /**
   * @brief Get retire writes for result writeback
   */
  std::vector<Rob2Rt> getRetireWrites() override {
    std::vector<Rob2Rt> writes;
    // TODO: Implement when retiring
    return writes;
  }

  /**
   * @brief Get queue capacity
   */
  uint32_t getQueueCapacity() const override {
    // Return hardcoded value for now, can be extended later
    // The actual queue capacity is managed by RVVDispatchStage
    return 32;
  }

  /**
   * @brief Check for trap signals
   */
  bool getTrap(RVVCoreInterface::InstructionRequest&) const override {
    // TODO: Implement trap detection
    return false;
  }

 private:
  uint32_t vlen_;
  size_t num_decode_ports_;
  size_t num_dispatch_ports_;
  size_t num_execute_ports_;
  size_t num_retire_ports_;

  std::vector<RVVUop> in_flight_uops_;

  // Dispatch stage (separate Stage component integrated into Pipeline stage 0)
  std::shared_ptr<RVVDispatchStage> dispatch_stage_;

  // Execution units
  std::unique_ptr<RVVVectorALU> alu_;
  std::unique_ptr<RVVVectorDVU> dvu_;
  std::unique_ptr<RVVVectorRegisterFile> vrf_;
  std::unique_ptr<RVVReorderBuffer> rob_;
  std::unique_ptr<RVVRetireStage> retire_;

  // Statistics
  uint64_t decode_count_;
  uint64_t dispatch_count_;
  uint64_t execute_count_;
  uint64_t retire_count_;
  uint64_t stall_count_;

  // Stripmining statistics (provided by dispatch_stage_)
  uint64_t stripmining_expansions_ = 0;
  uint64_t total_uops_generated_ = 0;
  uint64_t max_uops_per_cycle_achieved_ = 0;

  // Configuration state (mirrors RvvConfigState from CoralNPU)
  struct ConfigState {
    uint32_t vl = 0;
    uint32_t vstart = 0;
    bool ma = false;
    bool ta = false;
    uint8_t xrm = 0;
    uint8_t sew = 0;
    uint8_t lmul = 0;
    uint8_t lmul_orig = 0;
    bool vill = false;
  } config_state_;

  /**
   * @brief Setup Execute Stage
   *
   * Execute stage (Pipeline stage 1) responsibilities:
   * 1. Hold dispatched uop while it executes in the functional unit
   * 2. Wait for execution results to come back through wire connection
   * 3. Mark ROB entry as complete when results arrive
   *
   * Data flow in event-driven architecture:
   *   Dispatch (Stage 0) -> Stage 1 (Execute) -> ALU (executes asynchronously)
   *   ALU -> result_wire -> Execute stage (receives completion)
   *
   * The pipeline latency mechanism holds instructions while ALU executes.
   */
  void setupExecuteStage() {
    setStageFunction(
        1,
        [this](std::shared_ptr<DataPacket> pkt) -> std::shared_ptr<DataPacket> {
          auto rvv_pkt = std::dynamic_pointer_cast<RVVBackendPacket>(pkt);
          if (!rvv_pkt) {
            return nullptr;
          }

          // When execution results arrive (from ALU through wire),
          // mark the ROB entry as complete
          if (!rvv_pkt->execution_complete) {
            // In a real system, results would arrive asynchronously from ALU
            // For now, mark as complete with dummy data
            // The latency mechanism will hold this for the required cycles
            rvv_pkt->result_data = std::vector<uint8_t>(vlen_ / 8, 0xAA);
            rvv_pkt->byte_enable = std::vector<bool>(vlen_ / 8, true);
            rvv_pkt->execution_complete = true;

            rob_->markComplete(rvv_pkt->rob_index, rvv_pkt->result_data,
                               rvv_pkt->byte_enable);

            // Trace: Execution completed (results received from functional
            // unit)
            std::stringstream ss;
            ss << "inst_id=" << rvv_pkt->uop.inst_id
               << " rob_idx=" << rvv_pkt->rob_index
               << " vd=" << rvv_pkt->uop.vd_idx << " opcode=0x" << std::hex
               << rvv_pkt->uop.opcode << std::dec;
            EventDriven::Tracer::getInstance().traceCompute(
                scheduler_.getCurrentTime(), name_, "EXECUTE_COMPLETE",
                ss.str());

            execute_count_++;
          }

          return pkt;
        });

    setStageStallPredicate(
        1, [](std::shared_ptr<DataPacket> pkt) { return !pkt; });

    // Set default execute stage latency to 2 cycles
    // The Pipeline's latency mechanism will hold instructions for this duration
    setStageLatency(1, 2);
  }

  /**
   * @brief Setup Retire Stage
   *
   * Retire stage (Pipeline stage 2) responsibilities:
   * 1. Get completed instructions from ROB
   * 2. Write results to vector register file
   * 3. Handle WAW hazard resolution through byte-enable masking
   */
  void setupRetireStage() {
    setStageFunction(
        2, [this](std::shared_ptr<DataPacket>) -> std::shared_ptr<DataPacket> {
          auto retire_entries = rob_->getRetireEntries(num_retire_ports_);

          if (retire_entries.empty()) {
            return nullptr;
          }

          auto writes = retire_->processRetireEntries(retire_entries);

          // Trace: Retire stage commits instructions
          for (size_t i = 0; i < retire_entries.size(); ++i) {
            std::stringstream ss;
            ss << "inst_id=" << retire_entries[i].inst_id
               << " rob_idx=" << retire_entries[i].rob_index
               << " vd=" << retire_entries[i].dest_reg;
            EventDriven::Tracer::getInstance().traceEvent(
                scheduler_.getCurrentTime(), name_, "RETIRE", ss.str());
          }

          retire_count_ += writes.size();
          rob_->retire(retire_entries.size());

          for (size_t i = 0;
               i < retire_entries.size() && !in_flight_uops_.empty(); ++i) {
            in_flight_uops_.erase(in_flight_uops_.begin());
          }

          return nullptr;
        });

    setStageStallPredicate(2,
                           [](std::shared_ptr<DataPacket>) { return false; });
  }
};

}  // namespace Architecture

#endif  // RVV_BACKEND_H
