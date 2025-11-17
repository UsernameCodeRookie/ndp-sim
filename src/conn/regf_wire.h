#ifndef REGF_WIRE_H
#define REGF_WIRE_H

#include <cstdint>
#include <memory>
#include <string>

#include "../tick.h"

namespace Architecture {

/**
 * @brief RegisterFileWire - Specialized connection for register file writeback
 *
 * A composite wire connection that handles the complete register file write
 * protocol by connecting functional unit output ports to register file input
 * ports:
 *
 * **Source side (FU output):**
 * - rd_port: Register destination address port
 * - value_port: Data value port
 *
 * **Destination side (RF input):**
 * - addr_port: Register address input
 * - data_port: Register data input
 * - mask_port: Register mask input (for speculative writes)
 *
 * The connection automatically propagates data on each cycle, handling:
 * - Reading rd and value from source ports
 * - Writing to addr and data ports on destination
 * - Setting mask signal for masked/speculative writes
 * - Two-level buffering to prevent data loss
 */
class RegisterFileWire : public TickingConnection {
 public:
  RegisterFileWire(const std::string& name,
                   EventDriven::EventScheduler& scheduler, uint64_t period = 1)
      : TickingConnection(name, scheduler, period),
        transfers_(0),
        current_rd_(0),
        current_value_(0),
        current_valid_(false),
        next_rd_(0),
        next_value_(0),
        next_valid_(false),
        rd_port_(nullptr),
        value_port_(nullptr),
        addr_port_(nullptr),
        data_port_(nullptr),
        mask_port_(nullptr) {}

  virtual ~RegisterFileWire() = default;

  // ===========================================================================
  // Source-side binding (FU output ports)
  // ===========================================================================

  /**
   * @brief Bind the register destination port from functional unit
   * @param port Port connected to FU's rd output
   */
  void bindSrcRdPort(std::shared_ptr<Port> port) { rd_port_ = port; }

  /**
   * @brief Bind the data value port from functional unit
   * @param port Port connected to FU's value output
   */
  void bindSrcValuePort(std::shared_ptr<Port> port) { value_port_ = port; }

  // ===========================================================================
  // Destination-side binding (RF input ports)
  // ===========================================================================

  /**
   * @brief Bind the register file address input port
   * @param port Port connected to RF's write address input
   */
  void bindDstAddrPort(std::shared_ptr<Port> port) { addr_port_ = port; }

  /**
   * @brief Bind the register file data input port
   * @param port Port connected to RF's write data input
   */
  void bindDstDataPort(std::shared_ptr<Port> port) { data_port_ = port; }

  /**
   * @brief Bind the register file mask input port (optional)
   * @param port Port connected to RF's write mask input
   */
  void bindDstMaskPort(std::shared_ptr<Port> port) { mask_port_ = port; }

  // ===========================================================================
  // Access methods
  // ===========================================================================

  /**
   * @brief Get total number of successful transfers
   */
  uint64_t getTransfers() const { return transfers_; }

  /**
   * @brief Check if current buffer has valid data
   */
  bool hasCurrentData() const { return current_valid_; }

  /**
   * @brief Get current register destination
   */
  uint32_t getCurrentRd() const { return current_rd_; }

  /**
   * @brief Get current data value
   */
  uint32_t getCurrentValue() const { return current_value_; }

  /**
   * @brief Clear current data after destination processes it
   */
  void clearCurrentData() {
    current_rd_ = 0;
    current_value_ = 0;
    current_valid_ = false;
  }

  // ===========================================================================
  // Connection protocol
  // ===========================================================================

  /**
   * @brief Start the connection
   *
   * Validates that both source ports (rd and value) are bound.
   * Destination ports binding is optional (if not bound, data is just
   * buffered).
   */
  void start(uint64_t start_time = 0) {
    if (!rd_port_ || !value_port_) {
      throw std::runtime_error(
          "RegisterFileWire " + name_ +
          ": rd_port and value_port must be bound before starting. "
          "Use bindSrcRdPort() and bindSrcValuePort().");
    }
    enabled_ = true;
    schedule(start_time);
  }

  /**
   * @brief Propagate data through the connection
   *
   * Two-phase protocol:
   * 1. Move next → current if current was consumed by destination
   * 2. Read new (rd, value) pair from source ports
   * 3. Buffer data (current → next flow) or deliver to destination
   */
  void propagate() override {
    // Phase 1: Move next → current if current was consumed
    if (!current_valid_ && next_valid_) {
      current_rd_ = next_rd_;
      current_value_ = next_value_;
      current_valid_ = true;
      next_rd_ = 0;
      next_value_ = 0;
      next_valid_ = false;
    }

    // Phase 2: Read new (rd, value) pair from source ports
    if (rd_port_ && value_port_) {
      bool rd_has_data = rd_port_->hasData();
      bool value_has_data = value_port_->hasData();

      auto rd_packet =
          rd_has_data
              ? std::dynamic_pointer_cast<IntDataPacket>(rd_port_->read())
              : nullptr;
      auto value_packet =
          value_has_data
              ? std::dynamic_pointer_cast<IntDataPacket>(value_port_->read())
              : nullptr;

      if (rd_packet && value_packet && rd_packet->valid &&
          value_packet->valid) {
        uint32_t new_rd = static_cast<uint32_t>(rd_packet->value);
        uint32_t new_value = static_cast<uint32_t>(value_packet->value);

        TRACE_EVENT(scheduler_.getCurrentTime(), name_, "REGF_WIRE_READ_SRC",
                    "rd=" << new_rd << " value=" << new_value);

        // Deliver or buffer the data
        if (addr_port_ && data_port_) {
          // Destination ports are bound, try to deliver directly
          deliverToDestination(new_rd, new_value);
          transfers_++;

          TRACE_EVENT(scheduler_.getCurrentTime(), name_, "REGF_WIRE_DELIVER",
                      "rd=" << new_rd << " value=" << new_value);
        } else {
          // No destination ports, just buffer the data
          if (!current_valid_) {
            current_rd_ = new_rd;
            current_value_ = new_value;
            current_valid_ = true;
          } else if (!next_valid_) {
            next_rd_ = new_rd;
            next_value_ = new_value;
            next_valid_ = true;
          } else {
            // Both buffers full, overwrite next (current is active)
            next_rd_ = new_rd;
            next_value_ = new_value;
          }
        }

        TRACE_EVENT(scheduler_.getCurrentTime(), name_, "REGF_WIRE_PROPAGATE",
                    "rd=" << new_rd << " value=" << new_value);
      }
    }
  }

  /**
   * @brief Print connection statistics
   */
  void printStatistics() const {
    std::cout << "\n=== RegisterFileWire Statistics: " << name_
              << " ===" << std::endl;
    std::cout << "Total transfers: " << transfers_ << std::endl;
    std::cout << "Current valid: " << (current_valid_ ? "yes" : "no")
              << std::endl;
    if (current_valid_) {
      std::cout << "Current rd=" << current_rd_ << " value=" << current_value_
                << std::endl;
    }
    std::cout << "Next valid: " << (next_valid_ ? "yes" : "no") << std::endl;
  }

 private:
  /**
   * @brief Deliver (rd, value) pair to destination ports
   * @param rd Register destination address
   * @param value Data to write
   */
  void deliverToDestination(uint32_t rd, uint32_t value) {
    // Create packets for addr and data ports
    auto addr_packet = std::make_shared<IntDataPacket>(rd);
    auto data_packet = std::make_shared<IntDataPacket>(value);

    // Deliver with latency if configured
    if (latency_ > 0) {
      auto addr_copy = addr_packet;
      auto data_copy = data_packet;
      auto addr_port_copy = addr_port_;
      auto data_port_copy = data_port_;

      auto event = std::make_shared<EventDriven::LambdaEvent>(
          scheduler_.getCurrentTime() + latency_,
          [addr_port_copy, data_port_copy, addr_copy,
           data_copy](EventDriven::EventScheduler&) {
            addr_port_copy->setData(addr_copy);
            data_port_copy->setData(data_copy);
          },
          -1, name_ + "_Deliver");
      scheduler_.schedule(event);
    } else {
      addr_port_->setData(addr_packet);
      data_port_->setData(data_packet);
    }

    // Set mask port if bound (unmasked write = false)
    if (mask_port_) {
      auto mask_packet = std::make_shared<BoolDataPacket>(false);
      mask_port_->setData(mask_packet);
    }
  }

 private:
  // Counters
  uint64_t transfers_;

  // Two-level buffer (current and next)
  uint32_t current_rd_;
  uint32_t current_value_;
  bool current_valid_;

  uint32_t next_rd_;
  uint32_t next_value_;
  bool next_valid_;

  // Source side ports (FU outputs)
  std::shared_ptr<Port> rd_port_;     // FU's rd output
  std::shared_ptr<Port> value_port_;  // FU's value output

  // Destination side ports (RF inputs)
  std::shared_ptr<Port> addr_port_;  // RF's address input
  std::shared_ptr<Port> data_port_;  // RF's data input
  std::shared_ptr<Port> mask_port_;  // RF's mask input (optional)
};

}  // namespace Architecture

#endif  // REGF_WIRE_H
