#ifndef BRU_H
#define BRU_H

#include <cstring>
#include <memory>
#include <string>

#include "../packet.h"
#include "../port.h"
#include "../tick.h"
#include "../trace.h"
#include "pipeline.h"

/**
 * @brief BRU operation types
 *
 * Branch and Jump operations based on Coral NPU design
 */
enum class BruOp {
  // Branch operations
  BEQ,   // Branch if equal
  BNE,   // Branch if not equal
  BLT,   // Branch if less than (signed)
  BGE,   // Branch if greater than or equal (signed)
  BLTU,  // Branch if less than unsigned
  BGEU,  // Branch if greater than or equal unsigned

  // Jump operations
  JAL,   // Jump and link (unconditional)
  JALR,  // Jump and link register

  // System operations
  ECALL,   // Environment call (exception)
  EBREAK,  // Breakpoint
  MRET,    // Machine mode exception return
  WFI,     // Wait for interrupt
  FAULT    // Fault/exception handler
};

/**
 * @brief BRU command packet
 *
 * Decode stage command containing operation, PC, and target information
 */
class BruCommandPacket : public Architecture::DataPacket {
 public:
  BruCommandPacket(uint32_t pc, uint32_t target, BruOp op,
                   uint32_t rs1_data = 0, uint32_t rs2_data = 0, int rd = 0)
      : pc_(pc),
        target_(target),
        op_(op),
        rs1_data_(rs1_data),
        rs2_data_(rs2_data),
        rd_(rd) {}

  uint32_t getPC() const { return pc_; }
  uint32_t getPC4() const { return pc_ + 4; }
  uint32_t getTarget() const { return target_; }
  BruOp getOperation() const { return op_; }
  uint32_t getRS1Data() const { return rs1_data_; }
  uint32_t getRS2Data() const { return rs2_data_; }
  int getRegisterDestination() const { return rd_; }

  void setPC(uint32_t pc) { pc_ = pc; }
  void setTarget(uint32_t target) { target_ = target; }
  void setOperation(BruOp op) { op_ = op; }
  void setRS1Data(uint32_t data) { rs1_data_ = data; }
  void setRS2Data(uint32_t data) { rs2_data_ = data; }
  void setRegisterDestination(int rd) { rd_ = rd; }

  std::shared_ptr<Architecture::DataPacket> clone() const override {
    auto cloned = std::make_shared<BruCommandPacket>(pc_, target_, op_,
                                                     rs1_data_, rs2_data_, rd_);
    cloned->setTimestamp(timestamp_);
    cloned->setValid(valid_);
    return cloned;
  }

 private:
  uint32_t pc_;        // Program counter
  uint32_t target_;    // Branch target address
  BruOp op_;           // Operation
  uint32_t rs1_data_;  // Register source 1 data
  uint32_t rs2_data_;  // Register source 2 data
  int rd_;             // Register destination
};

/**
 * @brief BRU result packet
 *
 * Result containing target address and branch taken signal
 */
class BruResultPacket : public Architecture::DataPacket {
 public:
  BruResultPacket(uint32_t target, bool taken, bool link_valid = false,
                  uint32_t link_data = 0)
      : target_(target),
        taken_(taken),
        link_valid_(link_valid),
        link_data_(link_data) {}

  uint32_t getTarget() const { return target_; }
  bool isTaken() const { return taken_; }
  bool isLinkValid() const { return link_valid_; }
  uint32_t getLinkData() const { return link_data_; }

  void setTarget(uint32_t target) { target_ = target; }
  void setTaken(bool taken) { taken_ = taken; }
  void setLinkValid(bool valid) { link_valid_ = valid; }
  void setLinkData(uint32_t data) { link_data_ = data; }

  std::shared_ptr<Architecture::DataPacket> clone() const override {
    auto cloned = std::make_shared<BruResultPacket>(target_, taken_,
                                                    link_valid_, link_data_);
    cloned->setTimestamp(timestamp_);
    cloned->setValid(valid_);
    return cloned;
  }

 private:
  uint32_t target_;     // Branch target address
  bool taken_;          // Whether branch is taken
  bool link_valid_;     // Whether link register is valid
  uint32_t link_data_;  // Link register data (return address)
};

/**
 * @brief Branch Resolution Unit (BRU) Component
 *
 * Implements branch instruction resolution, target calculation, and control
 * flow redirection based on Coral NPU design.
 *
 * Architecture:
 * - Stage 0 (Decode): Parse command, set up comparison operands
 * - Stage 1 (Compare): Perform comparison operations
 * - Stage 2 (Execute): Determine branch outcome and target
 *
 * Operations supported:
 * - Branch instructions (BEQ, BNE, BLT, BGE, BLTU, BGEU)
 * - Jump instructions (JAL, JALR)
 * - System instructions (ECALL, EBREAK, MRET, WFI)
 */
class BruComponent : public PipelineComponent {
 public:
  /**
   * @brief Constructor
   * @param name Component name
   * @param scheduler Event scheduler reference
   * @param period Tick period
   */
  BruComponent(const std::string& name, EventDriven::EventScheduler& scheduler,
               uint64_t period = 1)
      : PipelineComponent(name, scheduler, period, 3),
        branches_resolved_(0),
        branches_taken_(0),
        branches_mispredicted_(0),
        system_exceptions_(0) {
    // Setup stage functions for BRU pipeline
    setupStageFunctions();
  }

  /**
   * @brief Tick function (inherited from PipelineComponent)
   * Uses pipeline's tick mechanism for stage advancement
   */
  void tick() override { PipelineComponent::tick(); }

  /**
   * @brief Get number of branches resolved
   */
  uint64_t getBranchesResolved() const { return branches_resolved_; }

  /**
   * @brief Get number of branches taken
   */
  uint64_t getBranchesTaken() const { return branches_taken_; }

  /**
   * @brief Get number of mispredictions
   */
  uint64_t getBranchesMispredicted() const { return branches_mispredicted_; }

  /**
   * @brief Get number of system exceptions handled
   */
  uint64_t getSystemExceptions() const { return system_exceptions_; }

  /**
   * @brief Reset statistics
   */
  void resetStatistics() {
    branches_resolved_ = 0;
    branches_taken_ = 0;
    branches_mispredicted_ = 0;
    system_exceptions_ = 0;
  }

 private:
  uint64_t branches_resolved_;
  uint64_t branches_taken_;
  uint64_t branches_mispredicted_;
  uint64_t system_exceptions_;

  /**
   * @brief Setup pipeline stage functions
   */
  void setupStageFunctions() {
    // Stage 0: Decode and prepare operands
    setStageFunction(0, [this](std::shared_ptr<Architecture::DataPacket> data) {
      auto cmd = std::dynamic_pointer_cast<BruCommandPacket>(data);
      if (!cmd) return data;

      // Store command for later stages
      current_cmd_ = cmd;
      return data;
    });

    // Stage 1: Perform comparisons and target calculation
    setStageFunction(1, [this](std::shared_ptr<Architecture::DataPacket> data) {
      auto cmd = std::dynamic_pointer_cast<BruCommandPacket>(data);
      if (!cmd) return data;

      uint32_t rs1 = cmd->getRS1Data();
      uint32_t rs2 = cmd->getRS2Data();
      uint32_t pc = cmd->getPC();
      uint32_t target = cmd->getTarget();

      // Perform comparisons
      bool eq = (rs1 == rs2);
      bool lt_signed = (static_cast<int32_t>(rs1) < static_cast<int32_t>(rs2));
      bool lt_unsigned = (rs1 < rs2);

      // Determine if branch is taken based on operation
      bool taken =
          evaluateBranch(cmd->getOperation(), eq, lt_signed, lt_unsigned);

      // Calculate actual target
      uint32_t branch_target =
          calculateTarget(cmd->getOperation(), pc, target, rs1);

      // Create result packet
      int rd = cmd->getRegisterDestination();
      bool link_valid = (cmd->getOperation() == BruOp::JAL ||
                         cmd->getOperation() == BruOp::JALR) &&
                        (rd != 0);
      uint32_t link_data = cmd->getPC4();

      auto result = std::make_shared<BruResultPacket>(branch_target, taken,
                                                      link_valid, link_data);
      result->setTimestamp(data->getTimestamp());

      return std::static_pointer_cast<Architecture::DataPacket>(result);
    });

    // Stage 2: Output result
    setStageFunction(2, [this](std::shared_ptr<Architecture::DataPacket> data) {
      auto result = std::dynamic_pointer_cast<BruResultPacket>(data);
      if (!result) return data;

      // Update statistics
      branches_resolved_++;
      if (result->isTaken()) {
        branches_taken_++;
      }

      // Send to output port
      auto output_port = getPort("out");
      if (output_port) {
        output_port->write(result);
      }

      return data;  // Return the original data packet type
    });
  }

  /**
   * @brief Evaluate branch condition
   * @param op Branch operation
   * @param eq RS1 == RS2
   * @param lt_signed RS1 < RS2 (signed)
   * @param lt_unsigned RS1 < RS2 (unsigned)
   * @return true if branch should be taken
   */
  bool evaluateBranch(BruOp op, bool eq, bool lt_signed, bool lt_unsigned) {
    switch (op) {
      case BruOp::BEQ:
        return eq;
      case BruOp::BNE:
        return !eq;
      case BruOp::BLT:
        return lt_signed;
      case BruOp::BGE:
        return !lt_signed;
      case BruOp::BLTU:
        return lt_unsigned;
      case BruOp::BGEU:
        return !lt_unsigned;
      case BruOp::JAL:
      case BruOp::JALR:
        return true;  // Unconditional jumps
      case BruOp::ECALL:
      case BruOp::EBREAK:
      case BruOp::MRET:
      case BruOp::WFI:
        system_exceptions_++;
        return true;  // System operations take effect
      case BruOp::FAULT:
        system_exceptions_++;
        return true;  // Fault handler execution
      default:
        return false;
    }
  }

  /**
   * @brief Calculate branch target address
   * @param op Branch operation
   * @param pc Current program counter
   * @param target Calculated target from decoder
   * @param rs1 Register source 1 value (for JALR)
   * @return Actual branch target address
   */
  uint32_t calculateTarget(BruOp op, uint32_t pc, uint32_t target,
                           uint32_t rs1) {
    switch (op) {
      case BruOp::JAL:
      case BruOp::BEQ:
      case BruOp::BNE:
      case BruOp::BLT:
      case BruOp::BGE:
      case BruOp::BLTU:
      case BruOp::BGEU:
        return target;  // Target computed by decoder
      case BruOp::JALR:
        return rs1 & 0xFFFFFFFE;  // JALR: (RS1 & ~1)
      case BruOp::ECALL:
      case BruOp::EBREAK:
      case BruOp::WFI:
        return pc + 4;  // Continue to next instruction
      case BruOp::MRET:
        return target;  // MEPC from CSR
      case BruOp::FAULT:
        return target;  // MTVEC from fault handler
      default:
        return target;
    }
  }

  std::shared_ptr<BruCommandPacket> current_cmd_;
};

#endif  // BRU_H
