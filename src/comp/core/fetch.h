#ifndef CORE_FETCH_H
#define CORE_FETCH_H

#include <cstdint>
#include <memory>
#include <queue>
#include <vector>

#include "../../trace.h"
#include "../buffer.h"
#include "decode.h"

namespace Architecture {

/**
 * @brief Fetch stage for SCore instruction pipeline
 *
 * Responsible for:
 * 1. Managing program counter and next-PC prediction
 * 2. Fetching instructions from instruction buffer (i-buffer)
 * 3. Handling branch redirects and flush events
 * 4. Filling the fetch buffer with instructions
 *
 * Based on Coral NPU's UncachedFetch design
 */
class FetchStage {
 public:
  /**
   * @brief Constructor
   * @param name Component name for tracing
   * @param fetch_buffer_size Maximum size of fetch buffer
   */
  FetchStage(const std::string& name, uint32_t fetch_buffer_size = 8)
      : name_(name),
        fetch_buffer_size_(fetch_buffer_size),
        pc_(0),
        next_pc_(4),
        fault_(false),
        ibuffer_(nullptr) {}

  /**
   * @brief Fetch next instruction from i-buffer
   *
   * Reads instruction from the instruction buffer at the given PC.
   *
   * @param pc Program counter
   * @return Decoded instruction at given PC
   */
  DecodedInstruction fetchInstruction(uint32_t pc) {
    // If instruction buffer is provided, read from it
    if (ibuffer_) {
      uint32_t inst_word = ibuffer_->load(pc);
      DecodedInstruction inst = DecodeStage::decode(pc, inst_word);
      return inst;
    }

    // Default: Generate a NOP if no instruction buffer
    DecodedInstruction inst;
    inst.addr = pc;
    inst.word = 0x00000013;  // ADDI x0, x0, 0 (NOP)
    inst.op_type = DecodedInstruction::OpType::ALU;
    inst.rd = 0;
    inst.rs1 = 0;
    inst.rs2 = 0;
    inst.imm = 0;
    inst.opcode = static_cast<uint32_t>(ALUOp::ADD);
    return inst;
  }

  /**
   * @brief Perform one fetch cycle
   *
   * Fetches instruction at current PC and updates PC for next cycle
   *
   * @return Vector of (PC, word) pairs for instructions fetched this cycle
   */
  std::vector<std::pair<uint32_t, uint32_t>> fetch() {
    std::vector<std::pair<uint32_t, uint32_t>> fetched;

    if (fault_) {
      // Stop fetching if we have a fault
      return fetched;
    }

    // Fetch instruction at current PC
    DecodedInstruction inst = fetchInstruction(pc_);
    if (inst.op_type != DecodedInstruction::OpType::INVALID) {
      fetched.push_back({pc_, inst.word});
    }

    // Update PC for next cycle
    pc_ = next_pc_;
    next_pc_ += 4;  // Sequential fetch (no branch prediction yet)

    return fetched;
  }

  /**
   * @brief Handle branch redirect
   *
   * Updates PC when a branch is taken
   *
   * @param target_pc Target address for branch
   */
  void branchRedirect(uint32_t target_pc) {
    pc_ = target_pc;
    next_pc_ = target_pc + 4;
  }

  /**
   * @brief Handle instruction flush
   *
   * Clears buffered instructions and resets to given PC
   *
   * @param flush_pc PC to resume fetching from
   */
  void flushAndReset(uint32_t flush_pc) {
    pc_ = flush_pc;
    next_pc_ = flush_pc + 4;
  }

  /**
   * @brief Check if fetch is faulted
   */
  bool isFaulted() const { return fault_; }

  /**
   * @brief Set fault status
   */
  void setFault(bool fault) { fault_ = fault; }

  /**
   * @brief Get current program counter
   */
  uint32_t getPC() const { return pc_; }

  /**
   * @brief Set program counter (for initialization)
   */
  void setPC(uint32_t pc) {
    pc_ = pc;
    next_pc_ = pc + 4;
  }

  /**
   * @brief Get next PC (for prediction)
   */
  uint32_t getNextPC() const { return next_pc_; }

  /**
   * @brief Reset fetch stage state
   */
  void reset() {
    pc_ = 0;
    next_pc_ = 4;
    fault_ = false;
  }

  /**
   * @brief Set external instruction buffer
   * @param ibuf Pointer to instruction buffer (Buffer with RANDOM_ACCESS mode)
   */
  void setInstructionBuffer(std::shared_ptr<Buffer> ibuf) { ibuffer_ = ibuf; }

 private:
  std::string name_;
  uint32_t fetch_buffer_size_;
  uint32_t pc_;                      // Current program counter
  uint32_t next_pc_;                 // Next PC (for simple sequential fetch)
  bool fault_;                       // Fetch fault flag
  std::shared_ptr<Buffer> ibuffer_;  // Instruction buffer (i-buffer)
};

}  // namespace Architecture

#endif  // CORE_FETCH_H
