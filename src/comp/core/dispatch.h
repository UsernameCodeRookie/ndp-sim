#ifndef CORE_DISPATCH_H
#define CORE_DISPATCH_H

#include <cstdint>
#include <memory>
#include <vector>

#include "../../trace.h"
#include "../bru.h"
#include "decode.h"

namespace Architecture {

/**
 * @brief Dispatch controller for SCore
 *
 * Implements Coral NPU instruction dispatch rules:
 * 1. In-order dispatch (don't skip instructions)
 * 2. Scoreboard hazard detection (RAW, WAW)
 * 3. Resource constraints (MLU/DVU/LSU = 1 per cycle)
 * 4. Control flow (don't dispatch past branches)
 * 5. Special instruction constraints (CSR/FENCE in slot 0 only)
 */
class DispatchStage {
 public:
  /**
   * @brief Constructor
   * @param name Component name for tracing
   * @param num_registers Number of registers for scoreboard
   * @param num_lanes Number of dispatch lanes
   */
  DispatchStage(const std::string& name, uint32_t num_registers,
                uint32_t num_lanes)
      : name_(name),
        num_lanes_(num_lanes),
        scoreboard_(num_registers, false),
        mlu_busy_(false),
        dvu_busy_(false),
        lsu_busy_(false) {}

  /**
   * @brief Check if instruction can be dispatched
   *
   * Implements hazard detection and resource constraints
   */
  bool canDispatch(const DecodedInstruction& inst, uint32_t lane) {
    // Special instructions (CSR, FENCE) only in slot 0
    if (lane > 0 && isSpecialInstruction(inst)) {
      return false;
    }

    // Check RAW hazard (instruction reads operand written by pending
    // instruction)
    if (inst.rs1 != 0 && scoreboard_[inst.rs1]) {
      return false;
    }
    if (inst.rs2 != 0 && scoreboard_[inst.rs2]) {
      return false;
    }

    // Check resource constraints
    switch (inst.op_type) {
      case DecodedInstruction::OpType::MLU:
        if (mlu_busy_) return false;
        break;
      case DecodedInstruction::OpType::DVU:
        if (dvu_busy_) return false;
        break;
      case DecodedInstruction::OpType::LSU:
        if (lsu_busy_) return false;
        break;
      default:
        break;
    }

    return true;
  }

  /**
   * @brief Clear resource usage trackers for next cycle
   */
  void clearResourceTrackers() {
    mlu_busy_ = false;
    dvu_busy_ = false;
    lsu_busy_ = false;
  }

  /**
   * @brief Mark resource as busy
   */
  void setResourceBusy(const DecodedInstruction& inst) {
    switch (inst.op_type) {
      case DecodedInstruction::OpType::MLU:
        mlu_busy_ = true;
        break;
      case DecodedInstruction::OpType::DVU:
        dvu_busy_ = true;
        break;
      case DecodedInstruction::OpType::LSU:
        lsu_busy_ = true;
        break;
      default:
        break;
    }
  }

  /**
   * @brief Update scoreboard with destination register
   */
  void updateScoreboard(uint32_t rd) {
    if (rd != 0 && rd < scoreboard_.size()) {
      scoreboard_[rd] = true;
    }
  }

  /**
   * @brief Clear scoreboard entry after instruction retirement
   */
  void retireRegister(uint32_t rd) {
    if (rd != 0 && rd < scoreboard_.size()) {
      scoreboard_[rd] = false;
    }
  }

  /**
   * @brief Check if instruction is a control flow instruction
   */
  static bool isControlFlowInstruction(const DecodedInstruction& inst) {
    return inst.op_type == DecodedInstruction::OpType::BRU ||
           (inst.op_type == DecodedInstruction::OpType::CSR &&
            inst.opcode == static_cast<uint32_t>(BruOp::ECALL)) ||
           (inst.op_type == DecodedInstruction::OpType::CSR &&
            inst.opcode == static_cast<uint32_t>(BruOp::MRET));
  }

  /**
   * @brief Check if instruction is special (CSR, FENCE, etc)
   */
  static bool isSpecialInstruction(const DecodedInstruction& inst) {
    return inst.op_type == DecodedInstruction::OpType::CSR ||
           inst.op_type == DecodedInstruction::OpType::FENCE;
  }

  /**
   * @brief Get current scoreboard state
   */
  const std::vector<bool>& getScoreboard() const { return scoreboard_; }

  /**
   * @brief Reset dispatch controller state
   */
  void reset() {
    std::fill(scoreboard_.begin(), scoreboard_.end(), false);
    mlu_busy_ = false;
    dvu_busy_ = false;
    lsu_busy_ = false;
  }

 private:
  std::string name_;
  uint32_t num_lanes_;
  std::vector<bool> scoreboard_;  // Register dependency tracking
  bool mlu_busy_;
  bool dvu_busy_;
  bool lsu_busy_;
};

}  // namespace Architecture

#endif  // CORE_DISPATCH_H
