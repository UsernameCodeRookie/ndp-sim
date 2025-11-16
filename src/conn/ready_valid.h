#ifndef READY_VALID_H
#define READY_VALID_H

#include <cstdint>
#include <memory>
#include <queue>
#include <string>
#include <vector>

#include "event.h"
#include "packet.h"
#include "scheduler.h"
#include "tick.h"

namespace Architecture {

/**
 * @brief ReadyValidConnection class - Composite connection for ready-valid
 * protocol
 *
 * This is a composite connection that manages the complete ready-valid
 * handshake:
 * - Data channel: transferred from source to destination via
 * src_ports_/dst_ports_
 * - Valid signal: bound to a fixed port indicating source has valid data
 * - Ready signal: bound to a fixed port indicating destination can accept data
 *
 * **Framework Constraint**: Both ready_port and valid_port MUST be bound before
 * start().
 *
 * The connection implements backpressure internally:
 * - Checks ready_port value == 1 before transferring data to destination
 * - Checks valid_port value == 1 before enqueueing data from source
 * - Uses internal buffer to decouple source and destination timing
 */
class ReadyValidConnection : public TickingConnection {
 public:
  ReadyValidConnection(const std::string& name,
                       EventDriven::EventScheduler& scheduler, uint64_t period,
                       size_t buffer_size = 2)
      : TickingConnection(name, scheduler, period),
        buffer_size_(buffer_size),
        transfers_(0),
        stalls_(0),
        ready_port_(nullptr),
        valid_port_(nullptr) {}

  virtual ~ReadyValidConnection() = default;

  /**
   * @brief Bind a port as the ready signal port
   * The ready signal indicates whether the destination can accept data
   */
  void bindReadyPort(std::shared_ptr<Port> port) { ready_port_ = port; }

  /**
   * @brief Bind a port as the valid signal port
   * The valid signal indicates whether the source has valid data
   */
  void bindValidPort(std::shared_ptr<Port> port) { valid_port_ = port; }

  // Start the connection
  void start(uint64_t start_time = 0) {
    // Validate that ready and valid ports are bound (framework constraint)
    if (!ready_port_ || !valid_port_) {
      throw std::runtime_error(
          "ReadyValidConnection " + name_ +
          ": ready_port and valid_port must be bound before starting. "
          "Use bindReadyPort() and bindValidPort().");
    }
    enabled_ = true;
    schedule(start_time);
  }

  // Getters
  uint64_t getPeriod() const { return period_; }
  bool isEnabled() const { return enabled_; }
  size_t getBufferOccupancy() const { return data_buffer_.size(); }
  size_t getBufferSize() const { return buffer_size_; }
  uint64_t getTransfers() const { return transfers_; }
  uint64_t getStalls() const { return stalls_; }

  /**
   * @brief Propagate data with ready/valid handshake
   *
   * Two-phase protocol:
   * 1. Enqueue new data from source if valid (first consume pending data)
   * 2. Transfer buffered data to destination if ready
   */
  void propagate() override {
    // Phase 1: Enqueue new data from source if valid
    tryEnqueueFromSource();

    // Phase 2: Transfer buffered data to destination if ready
    tryTransferToDestination();
  }

  /** @brief Print connection statistics */
  void printStatistics() const {
    std::cout << "\n=== Connection Statistics: " << name_
              << " ===" << std::endl;
    std::cout << "Total transfers: " << transfers_ << std::endl;
    std::cout << "Total stalls: " << stalls_ << std::endl;
    std::cout << "Buffer occupancy: " << data_buffer_.size() << "/"
              << buffer_size_ << std::endl;
    if (transfers_ + stalls_ > 0) {
      std::cout << "Utilization: "
                << (100.0 * transfers_ / (transfers_ + stalls_)) << "%"
                << std::endl;
    }
  }

 private:
  /** @brief Check if buffer can accept more data */
  bool canAcceptData() const { return data_buffer_.size() < buffer_size_; }

  /** @brief Check if buffer has data to send */
  bool hasDataToSend() const { return !data_buffer_.empty(); }

  /** @brief Read boolean signal from port (for ready/valid signals) */
  bool readSignal(std::shared_ptr<Port> port) const {
    auto data = std::dynamic_pointer_cast<BoolDataPacket>(port->getData());
    return data && data->value;
  }

  /** @brief Phase 2: Transfer buffered data to destination if ready */
  void tryTransferToDestination() {
    if (dst_ports_.empty() || !hasDataToSend()) return;

    // Check if destination port is free (no data currently present)
    if (dst_ports_[0]->hasData()) {
      // Destination can't accept now
      return;
    }

    bool is_ready = readSignal(ready_port_);

    if (is_ready) {
      deliverData(dst_ports_[0], data_buffer_.front());
      data_buffer_.pop();
      transfers_++;

      TRACE_EVENT(scheduler_.getCurrentTime(), name_, "CONN_TRANSFER",
                  "ready=1 valid=1, buffer=" << data_buffer_.size() << "/"
                                             << buffer_size_
                                             << " transfers=" << transfers_);
    }
  }

  /** @brief Phase 2: Enqueue data from source if valid */
  void tryEnqueueFromSource() {
    if (src_ports_.empty()) return;

    bool is_valid = readSignal(valid_port_);

    if (is_valid && canAcceptData()) {
      auto data = src_ports_[0]->read();
      if (data && data->valid) {
        data_buffer_.push(data);
        TRACE_EVENT(scheduler_.getCurrentTime(), name_, "CONN_ENQUEUE",
                    "valid=1 ready=1, buffer=" << data_buffer_.size() << "/"
                                               << buffer_size_);
      }
    } else if (is_valid && !canAcceptData()) {
      TRACE_EVENT(scheduler_.getCurrentTime(), name_, "CONN_BACK_PRESSURE",
                  "Buffer full, back pressure applied");
    }
  }

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

 protected:
  size_t buffer_size_;                                   // Internal buffer size
  std::queue<std::shared_ptr<DataPacket>> data_buffer_;  // FIFO buffer
  uint64_t transfers_;  // Successful transfers count
  uint64_t stalls_;     // Stall cycles count

  // Ready-Valid protocol ports (must be bound before starting)
  std::shared_ptr<Port> ready_port_;  // Port for ready signal
  std::shared_ptr<Port> valid_port_;  // Port for valid signal
};

}  // namespace Architecture

#endif  // READY_VALID_H
