#ifndef WIRE_H
#define WIRE_H

#include <cstdint>
#include <memory>
#include <string>

#include "../tick.h"

namespace Architecture {

/**
 * @brief WireConnection class with single-level buffering
 *
 * Transfers data from source port to destination port with one level of
 * internal buffering to prevent data loss when producer outputs faster than
 * consumer reads.
 *
 * Buffer Management:
 * - current_: data available for Core to read
 * - next_: data received from ALU, waiting to become current
 * - Each cycle: next becomes current, new ALU data goes to next
 * - Prevents overwrites by buffering one value ahead
 *
 * - Supports optional latency for simulating wire delay
 */
class Wire : public TickingConnection {
 public:
  Wire(const std::string& name, EventDriven::EventScheduler& scheduler,
       uint64_t period)
      : TickingConnection(name, scheduler, period),
        transfers_(0),
        current_data_(nullptr),
        next_data_(nullptr),
        current_valid_(false),
        next_valid_(false) {}

  virtual ~Wire() = default;

  // Getters
  uint64_t getTransfers() const { return transfers_; }

  /**
   * @brief Get current buffered data available for reading
   * This is what Core reads from the wire
   */
  std::shared_ptr<DataPacket> getCurrentData() const { return current_data_; }
  bool hasCurrentData() const { return current_valid_; }

  /**
   * @brief Clear current data after Core reads it
   */
  void clearCurrentData() {
    current_data_ = nullptr;
    current_valid_ = false;
  }

  /**
   * @brief Propagate data with buffering
   *
   * Two-stage buffer protocol:
   * 1. Read new data from ALU source port
   * 2. If current buffer is empty, move to current
   * 3. Otherwise buffer in next (don't overwrite current)
   * 4. Each cycle: next moves to current when Core drains current
   *
   * This prevents data loss when ALU outputs faster than Core reads.
   */
  void propagate() override {
    if (src_ports_.empty()) return;

    // Stage 1: Move next → current if current was consumed
    if (!current_valid_ && next_valid_) {
      current_data_ = next_data_;
      current_valid_ = true;
      next_data_ = nullptr;
      next_valid_ = false;
    }

    // Stage 2: Try to read new data from ALU
    if (src_ports_[0]->hasData()) {
      auto data = src_ports_[0]->read();
      if (data && data->valid) {
        // If we have destination ports, deliver to them
        if (!dst_ports_.empty()) {
          deliverData(dst_ports_[0], data);
          transfers_++;
        } else {
          // Buffer the data: current → next → current flow
          if (!current_valid_) {
            // Current is empty, fill it immediately
            current_data_ = data;
            current_valid_ = true;
          } else if (!next_valid_) {
            // Current is full, buffer in next
            next_data_ = data;
            next_valid_ = true;
          } else {
            // Both buffers full - this shouldn't happen in normal operation
            // but we handle it by keeping current and overwriting next
            // (current is what Core is actively reading)
            next_data_ = data;
          }
        }
      }
    }
  }

  /** @brief Print connection statistics */
  void printStatistics() const {
    std::cout << "\n=== Connection Statistics: " << name_
              << " ===" << std::endl;
    std::cout << "Total transfers: " << transfers_ << std::endl;
  }

 private:
  /** @brief Deliver data to destination (with optional latency) */
  void deliverData(std::shared_ptr<Port> dst_port,
                   std::shared_ptr<DataPacket> data) {
    if (latency_ > 0) {
      auto dst_copy = dst_port;
      auto data_copy = data;
      auto event = std::make_shared<EventDriven::LambdaEvent>(
          scheduler_.getCurrentTime() + latency_,
          [dst_copy, data_copy](EventDriven::EventScheduler&) {
            dst_copy->setData(data_copy);
          },
          -1, name_ + "_Deliver");
      scheduler_.schedule(event);
    } else {
      dst_port->setData(data);
    }
  }

 private:
  uint64_t transfers_;  // Successful transfers count

  // Two-level buffer for holding data
  std::shared_ptr<DataPacket> current_data_;  // Data available for Core to read
  std::shared_ptr<DataPacket>
      next_data_;       // Buffered data waiting to become current
  bool current_valid_;  // Whether current buffer has valid data
  bool next_valid_;     // Whether next buffer has valid data
};

}  // namespace Architecture

#endif  // WIRE_H
