#ifndef RVV_ROB_H
#define RVV_ROB_H

#include <cstdint>
#include <deque>
#include <memory>
#include <string>
#include <vector>

#include "../../component.h"
#include "../../event.h"
#include "../../packet.h"
#include "../../port.h"
#include "../../scheduler.h"
#include "../../tick.h"
#include "../../trace.h"

namespace Architecture {

/**
 * @brief ROB Entry Status
 *
 * Tracks the state of an instruction in the reorder buffer
 */
struct ROBEntry {
  uint64_t rob_index;                // ROB slot index
  uint64_t inst_id;                  // Instruction unique ID
  uint64_t uop_id;                   // Micro-op ID
  uint32_t dest_reg;                 // Destination register (v0-v31)
  bool dest_valid;                   // Destination is valid (not XXX format)
  uint8_t dest_type;                 // 0=VRF, 1=XRF
  std::vector<uint8_t> result_data;  // Computed result (when ready)
  std::vector<bool> byte_enable;     // Which bytes are valid
  std::vector<bool> vxsat_flag;      // Saturation flags per byte
  bool execution_complete = false;
  bool write_complete = false;
  bool retired = false;
  uint64_t dispatch_cycle;
  uint64_t complete_cycle;
  uint64_t retire_cycle;
  bool trap_flag = false;
  uint32_t trap_code = 0;

  ROBEntry() = default;
  ROBEntry(uint64_t idx, uint64_t id, uint64_t uop_id_, uint32_t dest,
           bool dest_v, uint8_t dtype, uint32_t vlen)
      : rob_index(idx),
        inst_id(id),
        uop_id(uop_id_),
        dest_reg(dest),
        dest_valid(dest_v),
        dest_type(dtype),
        result_data(vlen / 8, 0),
        byte_enable(vlen / 8, false),
        vxsat_flag(vlen / 8, false),
        dispatch_cycle(0) {}
};

/**
 * @brief RVV Reorder Buffer (ROB)
 *
 * Maintains instruction execution order and writeback sequencing
 * Features:
 * - Circular buffer for in-order retirement
 * - Tracks instruction completion status
 * - Manages write forwarding to dependents
 * - Supports up to 4 simultaneous retirements per cycle
 * - WAW hazard handling through byte-enable masking
 */
class RVVReorderBuffer : public TickingComponent {
 public:
  /**
   * @brief Constructor
   *
   * @param name Component name
   * @param scheduler Event scheduler
   * @param period Clock period
   * @param rob_size ROB depth (typically 128)
   * @param vlen Vector length in bits
   * @param num_retire_ports Number of retire ports (typically 4)
   */
  RVVReorderBuffer(const std::string& name,
                   EventDriven::EventScheduler& scheduler, uint64_t period,
                   size_t rob_size = 128, uint32_t vlen = 128,
                   size_t num_retire_ports = 4)
      : TickingComponent(name, scheduler, period),
        rob_size_(rob_size),
        vlen_(vlen),
        num_retire_ports_(num_retire_ports),
        head_(0),
        tail_(0),
        size_(0),
        entries_(rob_size),
        dispatched_count_(0),
        completed_count_(0),
        retired_count_(0) {
    // Create ports
    addPort("dispatch_in", PortDirection::INPUT);
    addPort("complete_in", PortDirection::INPUT);
    addPort("retire_out", PortDirection::OUTPUT);
  }

  /**
   * @brief Enqueue instruction into ROB
   *
   * @param inst_id Instruction ID
   * @param uop_id Micro-op ID
   * @param dest_reg Destination register
   * @param dest_valid True if destination is used
   * @param dest_type 0=VRF, 1=XRF
   * @return ROB index if successful, -1 if full
   */
  int64_t enqueue(uint64_t inst_id, uint64_t uop_id, uint32_t dest_reg,
                  bool dest_valid, uint8_t dest_type) {
    if (isFull()) {
      return -1;
    }

    uint64_t rob_idx = tail_;
    entries_[tail_] = ROBEntry(tail_, inst_id, uop_id, dest_reg, dest_valid,
                               dest_type, vlen_);
    entries_[tail_].dispatch_cycle = current_cycle_;

    tail_ = (tail_ + 1) % rob_size_;
    size_++;
    dispatched_count_++;

    return static_cast<int64_t>(rob_idx);
  }

  /**
   * @brief Mark instruction execution complete with results
   *
   * @param rob_index ROB entry index
   * @param result_data Computed result
   * @param byte_enable Which bytes are valid
   * @param vxsat_flags Saturation flags
   * @return True if successful
   */
  bool markComplete(uint64_t rob_index, const std::vector<uint8_t>& result_data,
                    const std::vector<bool>& byte_enable = {},
                    const std::vector<bool>& vxsat_flags = {}) {
    if (rob_index >= rob_size_ || entries_[rob_index].retired) {
      return false;
    }

    entries_[rob_index].result_data = result_data;
    entries_[rob_index].byte_enable =
        byte_enable.empty() ? std::vector<bool>(vlen_ / 8, true) : byte_enable;

    if (!vxsat_flags.empty()) {
      entries_[rob_index].vxsat_flag = vxsat_flags;
    }

    entries_[rob_index].execution_complete = true;
    entries_[rob_index].complete_cycle = current_cycle_;
    completed_count_++;

    return true;
  }

  /**
   * @brief Mark instruction as having trap
   */
  bool setTrap(uint64_t rob_index, uint32_t trap_code) {
    if (rob_index >= rob_size_) {
      return false;
    }

    entries_[rob_index].trap_flag = true;
    entries_[rob_index].trap_code = trap_code;
    entries_[rob_index].execution_complete = true;

    return true;
  }

  /**
   * @brief Get oldest entry ready for retirement
   *
   * @return Pointer to ROB entry if ready, nullptr otherwise
   */
  const ROBEntry* getRetireEntry() const {
    if (isEmpty()) {
      return nullptr;
    }

    const ROBEntry& entry = entries_[head_];
    if (entry.execution_complete && !entry.retired) {
      return &entry;
    }

    return nullptr;
  }

  /**
   * @brief Get multiple entries ready for retirement (in-order)
   *
   * @param count Maximum number of entries to retrieve
   * @return Vector of ROB entries ready to retire
   */
  std::vector<ROBEntry> getRetireEntries(size_t count) {
    std::vector<ROBEntry> result;

    size_t temp_head = head_;
    for (size_t i = 0; i < count && i < size_; ++i) {
      const ROBEntry& entry = entries_[temp_head];

      if (!entry.execution_complete || entry.retired) {
        break;  // Stop at first non-ready or already-retired entry
      }

      result.push_back(entry);
      temp_head = (temp_head + 1) % rob_size_;
    }

    return result;
  }

  /**
   * @brief Retire entries (mark as retired and remove from ROB)
   *
   * @param count Number of entries to retire
   * @return Actual number of entries retired
   */
  size_t retire(size_t count) {
    size_t retired = 0;

    for (size_t i = 0; i < count && !isEmpty(); ++i) {
      ROBEntry& entry = entries_[head_];

      if (!entry.execution_complete) {
        break;  // Can't retire incomplete instruction
      }

      entry.retired = true;
      entry.retire_cycle = current_cycle_;
      retired_count_++;

      head_ = (head_ + 1) % rob_size_;
      size_--;
      retired++;
    }

    return retired;
  }

  /**
   * @brief Check if ROB is full
   */
  bool isFull() const { return size_ >= rob_size_; }

  /**
   * @brief Check if ROB is empty
   */
  bool isEmpty() const { return size_ == 0; }

  /**
   * @brief Get ROB occupancy
   */
  size_t getSize() const { return size_; }

  /**
   * @brief Get ROB capacity
   */
  size_t getCapacity() const { return rob_size_; }

  /**
   * @brief Get number of pending (non-retired) instructions
   */
  size_t getPendingCount() const { return size_; }

  /**
   * @brief Get number of instructions ready to retire
   */
  size_t getReadyToRetireCount() const {
    size_t count = 0;
    size_t temp_head = head_;

    for (size_t i = 0; i < size_; ++i) {
      if (entries_[temp_head].execution_complete &&
          !entries_[temp_head].retired) {
        count++;
      } else {
        break;  // In-order, so stop at first non-ready
      }
      temp_head = (temp_head + 1) % rob_size_;
    }

    return count;
  }

  /**
   * @brief Get statistics
   */
  uint64_t getDispatchedCount() const { return dispatched_count_; }
  uint64_t getCompletedCount() const { return completed_count_; }
  uint64_t getRetiredCount() const { return retired_count_; }

  /**
   * @brief Reset statistics
   */
  void resetStatistics() {
    dispatched_count_ = 0;
    completed_count_ = 0;
    retired_count_ = 0;
  }

  /**
   * @brief Dump ROB state for debugging
   */
  std::string dumpState() const {
    std::string result;
    result += "ROB State: head=" + std::to_string(head_) +
              " tail=" + std::to_string(tail_) +
              " size=" + std::to_string(size_) + "\n";

    for (size_t i = 0; i < size_; ++i) {
      size_t idx = (head_ + i) % rob_size_;
      const ROBEntry& entry = entries_[idx];
      result += "  [" + std::to_string(idx) +
                "] inst_id=" + std::to_string(entry.inst_id) +
                " dest_reg=" + std::to_string(entry.dest_reg) +
                " complete=" + (entry.execution_complete ? "1" : "0") +
                " retired=" + (entry.retired ? "1" : "0") + "\n";
    }

    return result;
  }

 protected:
  /**
   * @brief Tick handler - no cycle-by-cycle processing needed
   */
  void tick() override {
    current_cycle_++;
    // ROB operations are typically combinational or pipelined elsewhere
  }

 private:
  size_t rob_size_;
  uint32_t vlen_;
  size_t num_retire_ports_;

  size_t head_;  // Points to oldest entry
  size_t tail_;  // Points to next free slot
  size_t size_;  // Current occupancy

  std::vector<ROBEntry> entries_;
  uint64_t current_cycle_ = 0;

  // Statistics
  uint64_t dispatched_count_;
  uint64_t completed_count_;
  uint64_t retired_count_;
};

}  // namespace Architecture

#endif  // RVV_ROB_H
