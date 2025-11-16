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
  BruCommandPacket(uint32_t pc = 0, uint32_t target = 0, BruOp op = BruOp::BEQ,
                   uint32_t rs1_data = 0, uint32_t rs2_data = 0, int rd = 0)
      : pc(pc),
        target(target),
        op(op),
        rs1_data(rs1_data),
        rs2_data(rs2_data),
        rd(rd) {}

  std::shared_ptr<Architecture::DataPacket> clone() const override {
    return cloneImpl<BruCommandPacket>(pc, target, op, rs1_data, rs2_data, rd);
  }

  uint32_t pc;        // Program counter
  uint32_t target;    // Branch target address
  BruOp op;           // Operation
  uint32_t rs1_data;  // Register source 1 data
  uint32_t rs2_data;  // Register source 2 data
  int rd;             // Register destination
};

/**
 * @brief BRU result packet
 *
 * Result containing target address and branch taken signal
 */
class BruResultPacket : public Architecture::DataPacket {
 public:
  BruResultPacket(uint32_t target = 0, bool taken = false,
                  bool link_valid = false, uint32_t link_data = 0)
      : target(target),
        taken(taken),
        link_valid(link_valid),
        link_data(link_data) {}

  std::shared_ptr<Architecture::DataPacket> clone() const override {
    return cloneImpl<BruResultPacket>(target, taken, link_valid, link_data);
  }

  uint32_t target;     // Branch target address
  bool taken;          // Whether branch is taken
  bool link_valid;     // Whether link register is valid
  uint32_t link_data;  // Link register data (return address)
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
class BranchUnit : public Pipeline {
 public:
  /**
   * @brief Constructor
   * @param name Component name
   * @param scheduler Event scheduler reference
   * @param period Tick period
   */
  BranchUnit(const std::string& name, EventDriven::EventScheduler& scheduler,
             uint64_t period = 1)
      : Pipeline(name, scheduler, period, 3),
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
  void tick() override { Pipeline::tick(); }

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

      uint32_t rs1 = cmd->rs1_data;
      uint32_t rs2 = cmd->rs2_data;
      uint32_t pc = cmd->pc;
      uint32_t target = cmd->target;

      // Perform comparisons
      bool eq = (rs1 == rs2);
      bool lt_signed = (static_cast<int32_t>(rs1) < static_cast<int32_t>(rs2));
      bool lt_unsigned = (rs1 < rs2);

      // Determine if branch is taken based on operation
      bool taken = evaluateBranch(cmd->op, eq, lt_signed, lt_unsigned);

      // Calculate actual target
      uint32_t branch_target = calculateTarget(cmd->op, pc, target, rs1);

      // Create result packet
      int rd = cmd->rd;
      bool link_valid =
          (cmd->op == BruOp::JAL || cmd->op == BruOp::JALR) && (rd != 0);
      uint32_t link_data = pc + 4;

      auto result = std::make_shared<BruResultPacket>(branch_target, taken,
                                                      link_valid, link_data);
      result->timestamp = data->timestamp;

      return std::static_pointer_cast<Architecture::DataPacket>(result);
    });

    // Stage 2: Output result
    setStageFunction(2, [this](std::shared_ptr<Architecture::DataPacket> data) {
      auto result = std::dynamic_pointer_cast<BruResultPacket>(data);
      if (!result) return data;

      // Update statistics
      branches_resolved_++;
      if (result->taken) {
        branches_taken_++;
      }

      // Send to output port
      auto output_port = getPort("out");
      if (output_port) {
        output_port->write(
            std::static_pointer_cast<Architecture::DataPacket>(result));
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
