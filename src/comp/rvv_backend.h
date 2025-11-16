#ifndef RVV_BACKEND_H
#define RVV_BACKEND_H

#include <cstdint>
#include <memory>
#include <queue>
#include <sstream>
#include <string>
#include <vector>

#include "../event.h"
#include "../packet.h"
#include "../port.h"
#include "../scheduler.h"
#include "../tick.h"
#include "../trace.h"
#include "component.h"
#include "pipeline.h"
#include "rvv_alu.h"
#include "rvv_dispatch.h"
#include "rvv_dvu.h"
#include "rvv_interface.h"
#include "rvv_regfile.h"
#include "rvv_retire.h"
#include "rvv_rob.h"

namespace Architecture {

/**
 * @brief RVV Backend Packet
 *
 * Data packet flowing through RVV backend pipeline
 */
struct RVVBackendPacket : public DataPacket {
  RVVUop uop;
  std::vector<uint8_t> result_data;
  std::vector<bool> byte_enable;
  bool execution_complete = false;
  uint64_t rob_index = 0;

  RVVBackendPacket() = default;
  RVVBackendPacket(const RVVUop& u) : uop(u) {}

  std::string toString() const {
    return "RVVBackendPacket(inst_id=" + std::to_string(uop.inst_id) + ")";
  }

  std::shared_ptr<DataPacket> clone() const override {
    return cloneWithVectors<RVVBackendPacket>([this](RVVBackendPacket* p) {
      p->uop = uop;
      p->result_data = result_data;
      p->byte_enable = byte_enable;
      p->execution_complete = execution_complete;
      p->rob_index = rob_index;
    });
  }
};

/**
 * @brief RVV Backend (RISC-V Vector Backend)
 *
 * Pipeline-based vector execution backend with stages:
 * 1. Decode: Instruction → Micro-op conversion
 * 2. Dispatch: Hazard detection, ROB allocation
 * 3. Execute: ALU/DIV operations (parallel)
 * 4. Retire: Writeback + WAW resolution
 *
 * Architecture:
 * - Multi-issue: up to 6 decode, 4 dispatch, 4 ALU/DIV parallel, 4 retire per
 * cycle
 * - Out-of-order execution with in-order retirement (ROB-based)
 * - Write forwarding through ROB for hazard resolution
 * - Multi-port flop-based vector register file
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
   * @param vlen Vector length in bits (128/256/512)
   */
  RVVBackend(const std::string& name, EventDriven::EventScheduler& scheduler,
             uint64_t period = 1, uint32_t vlen = 128)
      : Pipeline(name, scheduler, period, 4),  // 4 main stages
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
    // Create internal components
    alu_ = std::make_unique<RVVVectorALU>(name + "_alu", scheduler, vlen);
    dvu_ = std::make_unique<RVVVectorDVU>(name + "_dvu", scheduler, vlen);
    vrf_ = std::make_unique<RVVVectorRegisterFile>(name + "_vrf", scheduler, 1,
                                                   vlen);
    rob_ = std::make_unique<RVVReorderBuffer>(name + "_rob", scheduler, period,
                                              256, vlen);
    retire_ = std::make_unique<RVVRetireStage>(name + "_retire", scheduler,
                                               vlen, num_retire_ports_);

    // Configure pipeline stages
    setupDecodeStage();
    setupDispatchStage();
    setupExecuteStage();
    setupRetireStage();
  }

  /**
   * @brief Issue instruction to backend (old interface, for compatibility)
   */
  bool issueInstruction(const RVVInstruction& inst) {
    if (pending_instructions_.size() >= 32) {
      return false;
    }
    pending_instructions_.push(inst);
    return true;
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
  uint64_t getDispatchCount() const { return dispatch_count_; }
  uint64_t getExecuteCount() const { return execute_count_; }
  uint64_t getRetireCount() const { return retire_count_; }
  uint64_t getStallCount() const { return stall_count_; }

  /**
   * @brief Check if backend is idle
   */
  bool isIdle() const {
    return pending_instructions_.empty() && in_flight_uops_.empty() &&
           rob_->isEmpty();
  }

  // ========================================================================
  // RVVCoreInterface Implementation
  // ========================================================================

  /**
   * @brief Issue instruction from scalar core
   *
   * Convert RVVCoreInterface::InstructionRequest to internal RVVInstruction
   */
  bool issueInstruction(
      const RVVCoreInterface::InstructionRequest& inst_req) override {
    if (pending_instructions_.size() >= 32) {
      return false;
    }

    RVVInstruction inst;
    inst.inst_id = inst_req.inst_id;
    inst.opcode = inst_req.opcode;
    // Note: RVVInstruction doesn't have 'bits' field, fields are already
    // extracted
    inst.vs1_idx = inst_req.vs1_idx;
    inst.vs2_idx = inst_req.vs2_idx;
    inst.vd_idx = inst_req.vd_idx;
    inst.vm = inst_req.vm;
    inst.sew = inst_req.sew;
    inst.lmul = inst_req.lmul;
    inst.vl = inst_req.vl;
    inst.pc = inst_req.pc;

    pending_instructions_.push(inst);
    return true;
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
    return static_cast<uint32_t>(32 - pending_instructions_.size());
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

  std::queue<RVVInstruction> pending_instructions_;
  std::vector<RVVUop> in_flight_uops_;

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
   * @brief Setup Decode Stage
   */
  void setupDecodeStage() {
    setStageFunction(
        0,
        [this](std::shared_ptr<DataPacket> pkt) -> std::shared_ptr<DataPacket> {
          if (!pkt && pending_instructions_.empty()) {
            return nullptr;
          }

          if (!pkt) {
            // Create new packet from pending instruction
            const auto& inst = pending_instructions_.front();
            auto new_pkt = std::make_shared<RVVBackendPacket>();
            new_pkt->uop = RVVUop(inst, 0, 1, decode_count_);
            pending_instructions_.pop();

            // Trace: Decode stage processes instruction
            std::stringstream ss;
            ss << "inst_id=" << new_pkt->uop.inst_id << " opcode=0x" << std::hex
               << inst.opcode << std::dec << " vd=" << inst.vd_idx
               << " vs1=" << inst.vs1_idx << " vs2=" << inst.vs2_idx
               << " vl=" << inst.vl;
            EventDriven::Tracer::getInstance().traceInstruction(
                scheduler_.getCurrentTime(), name_, "DECODE", ss.str());

            decode_count_++;
            return new_pkt;
          }

          return pkt;
        });

    setStageStallPredicate(0, [this](std::shared_ptr<DataPacket>) {
      return pending_instructions_.empty();
    });
  }

  /**
   * @brief Setup Dispatch Stage
   */
  void setupDispatchStage() {
    setStageFunction(
        1,
        [this](std::shared_ptr<DataPacket> pkt) -> std::shared_ptr<DataPacket> {
          auto rvv_pkt = std::dynamic_pointer_cast<RVVBackendPacket>(pkt);
          if (!rvv_pkt) {
            return nullptr;
          }

          auto rob_idx =
              rob_->enqueue(rvv_pkt->uop.inst_id, rvv_pkt->uop.uop_id,
                            rvv_pkt->uop.vd_idx, true, 0);

          if (rob_idx < 0) {
            stall_count_++;
            return nullptr;
          }

          rvv_pkt->rob_index = rob_idx;

          // Trace: Dispatch stage allocates ROB entry
          std::stringstream ss;
          ss << "inst_id=" << rvv_pkt->uop.inst_id << " rob_idx=" << rob_idx
             << " vd=" << rvv_pkt->uop.vd_idx;
          EventDriven::Tracer::getInstance().traceEvent(
              scheduler_.getCurrentTime(), name_, "DISPATCH", ss.str());

          dispatch_count_++;
          in_flight_uops_.push_back(rvv_pkt->uop);
          return rvv_pkt;
        });

    setStageStallPredicate(
        1, [this](std::shared_ptr<DataPacket>) { return rob_->isFull(); });
  }

  /**
   * @brief Setup Execute Stage
   *
   * Uses the Pipeline's latency mechanism to hold instructions in the execute
   * stage for the appropriate number of cycles before advancing to retire.
   */
  void setupExecuteStage() {
    setStageFunction(
        2,
        [this](std::shared_ptr<DataPacket> pkt) -> std::shared_ptr<DataPacket> {
          auto rvv_pkt = std::dynamic_pointer_cast<RVVBackendPacket>(pkt);
          if (!rvv_pkt) {
            return nullptr;
          }

          // Mark execution complete - the pipeline latency mechanism will
          // hold this data in the stage for the required cycles
          if (!rvv_pkt->execution_complete) {
            rvv_pkt->result_data = std::vector<uint8_t>(vlen_ / 8, 0xAA);
            rvv_pkt->byte_enable = std::vector<bool>(vlen_ / 8, true);
            rvv_pkt->execution_complete = true;

            rob_->markComplete(rvv_pkt->rob_index, rvv_pkt->result_data,
                               rvv_pkt->byte_enable);

            // Trace: Execute stage completes instruction
            std::stringstream ss;
            ss << "inst_id=" << rvv_pkt->uop.inst_id
               << " rob_idx=" << rvv_pkt->rob_index << " latency="
               << RVVVectorALU::getOpcodeLatency(rvv_pkt->uop.opcode)
               << " cycles";
            EventDriven::Tracer::getInstance().traceCompute(
                scheduler_.getCurrentTime(), name_, "EXECUTE", ss.str());

            execute_count_++;
          }

          return pkt;
        });

    setStageStallPredicate(
        2, [](std::shared_ptr<DataPacket> pkt) { return !pkt; });

    // Set default execute stage latency to 2 cycles
    // The Pipeline's latency mechanism will hold instructions for this duration
    setStageLatency(2, 2);
  }

  /**
   * @brief Setup Retire Stage
   */
  void setupRetireStage() {
    setStageFunction(
        3, [this](std::shared_ptr<DataPacket>) -> std::shared_ptr<DataPacket> {
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

    setStageStallPredicate(3,
                           [](std::shared_ptr<DataPacket>) { return false; });
  }
};

}  // namespace Architecture

#endif  // RVV_BACKEND_H
