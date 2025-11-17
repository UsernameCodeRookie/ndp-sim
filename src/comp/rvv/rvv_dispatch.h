#ifndef RVV_DISPATCH_H
#define RVV_DISPATCH_H

#include <algorithm>
#include <cstdint>
#include <iostream>
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
#include "../../stage.h"
#include "../../tick.h"
#include "../../trace.h"

namespace Architecture {

/**
 * @brief RVV Vector Instruction - before decoding
 */
struct RVVInstruction {
  uint64_t pc;
  uint32_t opcode;
  uint32_t vs1_idx;  // Source 1 register
  uint32_t vs2_idx;  // Source 2 register
  uint32_t vd_idx;   // Destination register
  uint8_t vm;        // Mask enable bit
  uint8_t sew;       // Selected element width (bytes)
  uint16_t vl;       // Vector length in elements
  uint8_t lmul;      // Multiplier for vector length
  uint64_t inst_id;  // Instruction unique ID for ROB tracking

  RVVInstruction() = default;
  RVVInstruction(uint64_t p, uint32_t op, uint32_t v1, uint32_t v2,
                 uint32_t vd_, uint8_t vm_, uint8_t sew_, uint16_t vl_,
                 uint8_t lmul_, uint64_t id)
      : pc(p),
        opcode(op),
        vs1_idx(v1),
        vs2_idx(v2),
        vd_idx(vd_),
        vm(vm_),
        sew(sew_),
        vl(vl_),
        lmul(lmul_),
        inst_id(id) {}
};

/**
 * @brief RVV Micro-operation (after decoding)
 *
 * Single vector instruction can expand to multiple uops based on:
 * - Vector length (VLEN register grouping)
 * - Register grouping constraints (LMUL)
 */
struct RVVUop {
  uint64_t pc;
  uint32_t opcode;
  uint32_t vs1_idx;
  uint32_t vs2_idx;
  uint32_t vd_idx;
  uint8_t vm;
  uint8_t sew;         // Selected element width (bytes)
  uint16_t vl;         // Total vector length
  uint8_t lmul;        // Vector register grouping
  uint32_t uop_index;  // Which micro-op in sequence (0-N)
  uint32_t uop_count;  // Total number of uops for this instruction
  uint64_t inst_id;    // Parent instruction ID
  uint64_t uop_id;     // Unique micro-op ID
  uint64_t rob_index;  // ROB entry index (assigned during dispatch)
  bool rob_index_valid = false;

  RVVUop() = default;
  RVVUop(const RVVInstruction& inst, uint32_t idx, uint32_t cnt,
         uint64_t uop_id_)
      : pc(inst.pc),
        opcode(inst.opcode),
        vs1_idx(inst.vs1_idx),
        vs2_idx(inst.vs2_idx),
        vd_idx(inst.vd_idx),
        vm(inst.vm),
        sew(inst.sew),
        vl(inst.vl),
        lmul(inst.lmul),
        uop_index(idx),
        uop_count(cnt),
        inst_id(inst.inst_id),
        uop_id(uop_id_) {}
};

/**
 * @brief RVV Backend Packet (output from dispatch stage)
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
 * @brief ROB Entry Status for forwarding
 *
 * Tracks completion status of instructions in ROB
 */
struct ROBEntryStatus {
  uint64_t rob_index;
  uint32_t dest_reg;          // Destination register
  std::vector<uint8_t> data;  // Forwarded data (when ready)
  bool data_ready = false;
  uint64_t inst_id;

  ROBEntryStatus() = default;
  ROBEntryStatus(uint64_t idx, uint32_t dest, uint64_t id)
      : rob_index(idx), dest_reg(dest), inst_id(id) {}
};

/**
 * @brief ROB Forwarding Buffer - per execution unit
 *
 * Stores pending writes from ROB for write forwarding to dispatch
 */
class ROBForwardingBuffer {
 public:
  explicit ROBForwardingBuffer(size_t capacity = 8, uint32_t vlen = 128)
      : capacity_(capacity), vlen_(vlen), buffer_(capacity) {}

  /**
   * @brief Enqueue a pending ROB entry for forwarding
   */
  bool enqueue(uint64_t rob_idx, uint32_t dest_reg, uint64_t inst_id) {
    if (isFull()) {
      return false;  // Buffer full
    }

    ROBEntryStatus entry(rob_idx, dest_reg, inst_id);
    entry.data.assign(vlen_ / 8, 0);
    buffer_[write_ptr_] = entry;
    write_ptr_ = (write_ptr_ + 1) % capacity_;
    size_++;
    return true;
  }

  /**
   * @brief Mark data as ready for a ROB entry
   */
  bool markDataReady(uint64_t rob_idx, const std::vector<uint8_t>& data) {
    for (size_t i = 0; i < size_; ++i) {
      size_t idx = (read_ptr_ + i) % capacity_;
      if (buffer_[idx].rob_index == rob_idx) {
        buffer_[idx].data = data;
        buffer_[idx].data_ready = true;
        return true;
      }
    }
    return false;
  }

  /**
   * @brief Get forwarded data if available
   *
   * Returns data if ROB entry is ready, otherwise returns empty vector
   */
  std::vector<uint8_t> getForwardedData(uint64_t rob_idx) const {
    for (size_t i = 0; i < size_; ++i) {
      size_t idx = (read_ptr_ + i) % capacity_;
      if (buffer_[idx].rob_index == rob_idx && buffer_[idx].data_ready) {
        return buffer_[idx].data;
      }
    }
    return std::vector<uint8_t>();  // Not ready
  }

  /**
   * @brief Dequeue oldest entry (when instruction completes)
   */
  bool dequeue() {
    if (isEmpty()) {
      return false;
    }
    read_ptr_ = (read_ptr_ + 1) % capacity_;
    size_--;
    return true;
  }

  bool isEmpty() const { return size_ == 0; }
  bool isFull() const { return size_ >= capacity_; }
  size_t getSize() const { return size_; }
  size_t getCapacity() const { return capacity_; }

 private:
  size_t capacity_;
  uint32_t vlen_;
  std::vector<ROBEntryStatus> buffer_;
  size_t read_ptr_ = 0;
  size_t write_ptr_ = 0;
  size_t size_ = 0;
};

/**
 * @brief RVV Decode Stage
 *
 * Converts vector instructions into micro-operations (uops)
 * Each instruction can generate up to 6 uops per cycle
 *
 * Expands instructions based on vector length and register grouping
 */
class RVVDecodeStage {
 public:
  explicit RVVDecodeStage(uint32_t vlen = 128, size_t max_uops_per_cycle = 6)
      : vlen_(vlen),
        max_uops_per_cycle_(max_uops_per_cycle),
        uop_id_counter_(0) {}

  /**
   * @brief Decode instruction into uops
   *
   * @param inst Input instruction
   * @return Vector of generated uops
   */
  std::vector<RVVUop> decode(const RVVInstruction& inst) {
    std::vector<RVVUop> uops;

    // Calculate number of uops needed based on vector length and SEW
    // SEW encoding: 0=8b, 1=16b, 2=32b, 3=64b
    uint32_t sew_bits = 8 << inst.sew;  // Convert to bits

    // For simplicity: assume 1 uop can process VLEN bits of data
    // If instruction needs more elements, split into multiple uops
    uint32_t elements_per_uop =
        vlen_ / sew_bits;  // VLEN bits / element size in bits

    // LMUL: 0=1x, 1=2x, 2=4x, 3=8x
    uint32_t lmul_val = 1 << inst.lmul;
    uint32_t total_elements = inst.vl * lmul_val;  // Total elements to process
    uint32_t uop_count =
        (total_elements + elements_per_uop - 1) / elements_per_uop;

    // Limit to maximum uops generated per cycle
    uop_count = std::min(uop_count, static_cast<uint32_t>(max_uops_per_cycle_));

    // For this simple implementation, always generate at least 1 uop if
    // instruction is valid
    if (uop_count == 0 && inst.vl > 0) {
      uop_count = 1;
    }

    // Generate uops
    for (uint32_t i = 0; i < uop_count; ++i) {
      RVVUop uop(inst, i, uop_count, uop_id_counter_++);
      uops.push_back(uop);
    }

    return uops;
  }

  uint64_t getUopIdCounter() const { return uop_id_counter_; }
  void resetUopCounter() { uop_id_counter_ = 0; }

 private:
  uint32_t vlen_;
  size_t max_uops_per_cycle_;
  uint64_t uop_id_counter_;
};

/**
 * @brief RAW (Read-After-Write) Hazard Detector
 *
 * Checks if uop has data dependencies on ROB entries
 */
class RawHazardDetector {
 public:
  /**
   * @brief Check for RAW hazard with ROB entries
   *
   * @param uop Uop to check
   * @param rob_entries Active ROB entries
   * @param forward_buffer Forwarding buffer
   * @return Pair<has_hazard, all_data_ready>
   */
  static std::pair<bool, bool> checkRawHazard(
      const RVVUop& uop, const std::vector<ROBEntryStatus>& rob_entries,
      const ROBForwardingBuffer& forward_buffer) {
    bool has_hazard = false;
    bool all_data_ready = true;

    // Check vs1 dependency
    if (uop.vs1_idx < 32) {
      for (const auto& entry : rob_entries) {
        if (entry.dest_reg == uop.vs1_idx) {
          has_hazard = true;
          if (!entry.data_ready &&
              forward_buffer.getForwardedData(entry.rob_index).empty()) {
            all_data_ready = false;
          }
          break;
        }
      }
    }

    // Check vs2 dependency
    if (has_hazard && all_data_ready) {
      if (uop.vs2_idx < 32) {
        for (const auto& entry : rob_entries) {
          if (entry.dest_reg == uop.vs2_idx) {
            has_hazard = true;
            if (!entry.data_ready &&
                forward_buffer.getForwardedData(entry.rob_index).empty()) {
              all_data_ready = false;
            }
            break;
          }
        }
      }
    }

    // Return: has_raw_hazard, can_forward
    if (has_hazard && all_data_ready) {
      return {true, true};  // Can forward
    }
    return {has_hazard, has_hazard && all_data_ready};
  }
};

/**
 * @brief Structure Hazard Detector
 *
 * Checks if enough read ports available for operand access
 */
class StructureHazardDetector {
 public:
  /**
   * @brief Check structure hazard for uops
   *
   * @param uops Uops to check
   * @param num_read_ports Available VRF read ports
   * @return True if structure hazard detected
   */
  static bool checkStructureHazard(const std::vector<RVVUop>& uops,
                                   size_t num_read_ports = 4) {
    // Count required read ports
    size_t required_ports = 0;

    for (const auto& uop : uops) {
      // Each uop can need 1-3 read ports (vs1, vs2, vd/mask)
      if (uop.vs1_idx < 32) required_ports++;
      if (uop.vs2_idx < 32 && uop.vs2_idx != uop.vs1_idx) required_ports++;
      if (uop.vd_idx < 32 && uop.vd_idx != uop.vs1_idx &&
          uop.vd_idx != uop.vs2_idx)
        required_ports++;
    }

    return required_ports > num_read_ports;
  }
};

/**
 * @brief RVV Dispatch Stage
 *
 * Manages uop dispatch with:
 * - Instruction decoding (stripmining for LMUL > 1)
 * - VRF read port allocation
 * - RAW hazard detection and stalling
 * - Write forwarding from ROB
 * - Multi-port uop issue (up to 2-4 uops per cycle)
 *
 * Inherits from Stage to be integrated as a Pipeline stage.
 * The process() method is called each cycle by the Pipeline.
 */
class RVVDispatchStage : public Stage {
 public:
  /**
   * @brief Constructor
   *
   * @param name Component name
   * @param scheduler Event scheduler
   * @param period Clock period (not used, but kept for compatibility)
   * @param vlen Vector length in bits
   * @param num_read_ports VRF read ports (typically 4)
   * @param max_issue_width Max uops per cycle
   * @param rob_size ROB depth
   */
  RVVDispatchStage(const std::string& name,
                   EventDriven::EventScheduler& scheduler, uint64_t period,
                   uint32_t vlen = 128, size_t num_read_ports = 4,
                   size_t max_issue_width = 2, size_t rob_size = 128)
      : Stage(name, scheduler),
        vlen_(vlen),
        num_read_ports_(num_read_ports),
        max_issue_width_(max_issue_width),
        rob_size_(rob_size),
        decode_stage_(vlen, 6),
        forwarding_buffer_(8, vlen),
        uops_dispatched_(0),
        uops_stalled_(0),
        raw_hazard_stalls_(0),
        struct_hazard_stalls_(0) {
    // Create ports
    addPort("in_inst", PortDirection::INPUT);
    addPort("out_uop", PortDirection::OUTPUT);
    addPort("vrf_read_req", PortDirection::OUTPUT);
  }

  /**
   * @brief Queue instruction for decoding
   */
  bool queueInstruction(const RVVInstruction& inst) {
    if (instruction_queue_.size() >= 16) {
      return false;  // Queue full
    }
    instruction_queue_.push(inst);
    return true;
  }

  /**
   * @brief Get dispatch count
   */
  uint64_t getDispatchCount() const { return uops_dispatched_; }

  /**
   * @brief Dispatch uop to execution unit
   *
   * Checks hazards and returns operands from VRF or forwarding buffer
   *
   * @param uop Uop to dispatch (will be modified with ROB index)
   * @param vs1_data Data from VRF (optional)
   * @param vs2_data Data from VRF (optional)
   * @return True if dispatch successful
   */
  bool dispatchUop(RVVUop& uop, const std::vector<uint8_t>& /* vs1_data */ = {},
                   const std::vector<uint8_t>& /* vs2_data */ = {}) {
    // Check RAW hazard
    auto [has_raw, can_forward] = RawHazardDetector::checkRawHazard(
        uop, active_rob_entries_, forwarding_buffer_);

    if (has_raw && !can_forward) {
      raw_hazard_stalls_++;
      return false;  // Stall due to RAW
    }

    // Check structure hazard (would check actual VRF read ports in real impl)
    if (pending_dispatches_.size() >= max_issue_width_) {
      struct_hazard_stalls_++;
      return false;
    }

    // Dispatch successful
    // Assign ROB index
    uop.rob_index = next_rob_index_++;
    uop.rob_index_valid = true;
    pending_dispatches_.push_back(uop);
    uops_dispatched_++;

    return true;
  }

  /**
   * @brief Get pending uops for this cycle
   */
  const std::vector<RVVUop>& getPendingDispatches() const {
    return pending_dispatches_;
  }

  /**
   * @brief Clear dispatched uops for next cycle
   */
  void clearDispatches() { pending_dispatches_.clear(); }

  /**
   * @brief Update ROB entry with completion data
   */
  void updateRobEntry(uint64_t rob_idx, uint32_t /* dest_reg */,
                      const std::vector<uint8_t>& data) {
    // Update in forwarding buffer
    forwarding_buffer_.markDataReady(rob_idx, data);

    // Update in active ROB entries
    for (auto& entry : active_rob_entries_) {
      if (entry.rob_index == rob_idx) {
        entry.data = data;
        entry.data_ready = true;
        break;
      }
    }
  }

  /**
   * @brief Retire instruction from ROB
   */
  void retireInstruction(uint64_t rob_idx) {
    // Remove from active ROB entries
    auto it = std::find_if(
        active_rob_entries_.begin(), active_rob_entries_.end(),
        [rob_idx](const ROBEntryStatus& e) { return e.rob_index == rob_idx; });

    if (it != active_rob_entries_.end()) {
      active_rob_entries_.erase(it);
    }

    forwarding_buffer_.dequeue();
  }

  /**
   * @brief Get statistics
   */
  uint64_t getUopsDispatched() const { return uops_dispatched_; }
  uint64_t getUopsStalled() const { return uops_stalled_; }
  uint64_t getRawHazardStalls() const { return raw_hazard_stalls_; }
  uint64_t getStructHazardStalls() const { return struct_hazard_stalls_; }

  /**
   * @brief Get pending uop count
   */
  size_t getPendingUopCount() const {
    size_t count = 0;
    for (const auto& q : uop_queues_) {
      count += q.size();
    }
    return count;
  }

  /**
   * @brief Reset statistics
   */
  void resetStatistics() {
    uops_dispatched_ = 0;
    uops_stalled_ = 0;
    raw_hazard_stalls_ = 0;
    struct_hazard_stalls_ = 0;
  }

 public:
  /**
   * @brief Process method called by Pipeline each cycle
   *
   * Performs decode and dispatch operations:
   * - Decodes instructions to uops (up to 6 per cycle)
   * - Dispatches uops with hazard checking (up to 2-4 per cycle)
   * - Returns pending dispatches as data packets
   *
   * @param data Input data (not used in dispatch stage)
   * @return Packet containing dispatched uop or nullptr
   */
  std::shared_ptr<DataPacket> process(
      std::shared_ptr<DataPacket> data) override {
    // Unused in dispatch stage
    (void)data;

    // If we have pending dispatches from last cycle, return the next one
    if (!pending_dispatches_.empty()) {
      auto uop = pending_dispatches_.front();
      pending_dispatches_.erase(pending_dispatches_.begin());

      auto pkt = std::make_shared<RVVBackendPacket>(uop);
      pkt->timestamp = scheduler_.getCurrentTime();
      pkt->rob_index = uop.rob_index;

      // Trace: Dispatch
      std::stringstream ss;
      ss << "inst_id=" << uop.inst_id << " rob_idx=" << uop.rob_index
         << " vd=" << uop.vd_idx << " opcode=0x" << std::hex << uop.opcode
         << std::dec;
      EventDriven::Tracer::getInstance().traceEvent(
          scheduler_.getCurrentTime(), name_, "DISPATCH", ss.str());

      return pkt;
    }

    // Stage 1: Decode instructions -> uops (max 6 uops per cycle)
    std::vector<RVVUop> decoded_uops;
    while (!instruction_queue_.empty() && decoded_uops.size() < 6) {
      RVVInstruction inst = instruction_queue_.front();
      instruction_queue_.pop();

      auto uops = decode_stage_.decode(inst);

      for (const auto& uop : uops) {
        if (decoded_uops.size() < 6) {
          decoded_uops.push_back(uop);
        }
      }
    }

    // Add to dispatch queue
    for (const auto& uop : decoded_uops) {
      dispatch_queue_.push(uop);
    }

    // Stage 2: Dispatch uops (max 2-4 per cycle)
    size_t dispatched_this_cycle = 0;

    while (!dispatch_queue_.empty() &&
           dispatched_this_cycle < max_issue_width_) {
      RVVUop uop = dispatch_queue_.front();

      // Check hazards
      auto [has_raw, can_forward] = RawHazardDetector::checkRawHazard(
          uop, active_rob_entries_, forwarding_buffer_);

      if (has_raw && !can_forward) {
        // Stall - don't issue
        uops_stalled_++;
        break;  // Stall entire dispatch
      }

      // Check structure hazard
      if (StructureHazardDetector::checkStructureHazard(pending_dispatches_,
                                                        num_read_ports_)) {
        struct_hazard_stalls_++;
        break;  // Can't issue more uops this cycle
      }

      dispatch_queue_.pop();
      uop.rob_index = next_rob_index_++;
      uop.rob_index_valid = true;
      pending_dispatches_.push_back(uop);
      dispatched_this_cycle++;
      uops_dispatched_++;

      // Add to active ROB entries
      ROBEntryStatus rob_entry(uop.rob_index, uop.vd_idx, uop.inst_id);
      active_rob_entries_.push_back(rob_entry);
    }

    // Return first dispatched uop if available
    if (!pending_dispatches_.empty()) {
      auto uop = pending_dispatches_.front();
      pending_dispatches_.erase(pending_dispatches_.begin());

      auto pkt = std::make_shared<RVVBackendPacket>(uop);
      pkt->timestamp = scheduler_.getCurrentTime();
      pkt->rob_index = uop.rob_index;

      // Trace: Dispatch
      std::stringstream ss;
      ss << "inst_id=" << uop.inst_id << " rob_idx=" << uop.rob_index
         << " vd=" << uop.vd_idx << " opcode=0x" << std::hex << uop.opcode
         << std::dec;
      EventDriven::Tracer::getInstance().traceEvent(
          scheduler_.getCurrentTime(), name_, "DISPATCH", ss.str());

      return pkt;
    }

    return nullptr;
  }

 private:
  // Configuration
  uint32_t vlen_;
  size_t num_read_ports_;
  size_t max_issue_width_;
  size_t rob_size_;

  // Pipeline stages
  RVVDecodeStage decode_stage_;
  std::queue<RVVInstruction> instruction_queue_;
  std::queue<RVVUop> dispatch_queue_;
  std::vector<std::queue<RVVUop>>
      uop_queues_;  // Per-funcunit queues (ALU, MUL, DIV, LSU)

  // Dispatch state
  std::vector<RVVUop> pending_dispatches_;
  uint64_t next_rob_index_ = 0;

  // ROB and forwarding
  std::vector<ROBEntryStatus> active_rob_entries_;
  ROBForwardingBuffer forwarding_buffer_;

  // Statistics
  uint64_t uops_dispatched_;
  uint64_t uops_stalled_;
  uint64_t raw_hazard_stalls_;
  uint64_t struct_hazard_stalls_;
};

}  // namespace Architecture

#endif  // RVV_DISPATCH_H
