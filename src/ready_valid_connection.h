#ifndef READY_VALID_CONNECTION_H
#define READY_VALID_CONNECTION_H

#include <cstdint>
#include <memory>
#include <queue>
#include <string>
#include <vector>

#include "connection.h"
#include "event.h"
#include "packet.h"
#include "scheduler.h"

namespace Architecture {

/**
 * @brief ReadyValidConnection class
 *
 * Connection with back pressure support using ready/valid handshake protocol.
 * This connection manages a complete ready-valid interface with:
 * - Data transfer from source to destination
 * - Valid signal indicating data availability
 * - Ready signal providing back pressure
 *
 * The connection expects exactly ONE source port and ONE destination port.
 * It automatically manages the handshaking protocol internally.
 */
class ReadyValidConnection : public Connection {
 public:
  ReadyValidConnection(const std::string& name,
                       EventDriven::EventScheduler& scheduler, uint64_t period,
                       size_t buffer_size = 2)
      : Connection(name, scheduler),
        period_(period),
        enabled_(true),
        buffer_size_(buffer_size),
        transfers_(0),
        stalls_(0) {}

  virtual ~ReadyValidConnection() = default;

  // Start the connection
  void start(uint64_t start_time = 0) {
    enabled_ = true;
    schedulePropagate(start_time);
  }

  // Stop the connection
  void stop() { enabled_ = false; }

  // Getters
  uint64_t getPeriod() const { return period_; }
  bool isEnabled() const { return enabled_; }
  size_t getBufferOccupancy() const { return data_buffer_.size(); }
  size_t getBufferSize() const { return buffer_size_; }
  uint64_t getTransfers() const { return transfers_; }
  uint64_t getStalls() const { return stalls_; }

  /**
   * @brief Check if connection can accept data (ready signal)
   */
  bool canAcceptData() const { return data_buffer_.size() < buffer_size_; }

  /**
   * @brief Check if connection has data to send (valid signal)
   */
  bool hasDataToSend() const { return !data_buffer_.empty(); }

  /**
   * @brief Propagate data with ready/valid handshake
   *
   * Ready-Valid Protocol:
   * - valid: indicates source has valid data
   * - ready: indicates destination can accept data
   * - Transfer happens when both valid and ready are true
   *
   * Implementation:
   * 1. Check if destination is ready (port is empty)
   * 2. If ready and buffer has data, transfer to destination
   * 3. Check if source has valid data (port has data)
   * 4. If valid and buffer has space, enqueue from source
   */
  void propagate() override {
    bool source_has_valid = false;
    bool dest_is_ready = false;

    // Phase 1: Check destination ready and try to transfer
    if (dst_ports_.size() > 0) {
      auto dst_port = dst_ports_[0];
      dest_is_ready = !dst_port->hasData();  // Ready if port is empty

      if (dest_is_ready && hasDataToSend()) {
        // Ready-Valid handshake: both ready and valid
        auto data = data_buffer_.front();
        data_buffer_.pop();

        if (latency_ > 0) {
          // Schedule delayed delivery
          auto dst_port_copy = dst_port;
          auto data_copy = data;
          auto deliver_event = std::make_shared<EventDriven::LambdaEvent>(
              scheduler_.getCurrentTime() + latency_,
              [dst_port_copy, data_copy](EventDriven::EventScheduler&) {
                dst_port_copy->setData(data_copy);
              },
              -1, name_ + "_Deliver");  // Priority -1 (before components)
          scheduler_.schedule(deliver_event);
        } else {
          // Immediate delivery
          dst_port->setData(data);
        }

        transfers_++;
        TRACE_EVENT(scheduler_.getCurrentTime(), name_, "CONN_TRANSFER",
                    "ready=1 valid=1, buffer=" << data_buffer_.size() << "/"
                                               << buffer_size_
                                               << " transfers=" << transfers_);
      } else if (!dest_is_ready && hasDataToSend()) {
        // Stall: valid but not ready
        stalls_++;
        TRACE_EVENT(scheduler_.getCurrentTime(), name_, "CONN_STALL",
                    "ready=0 valid=1, buffer=" << data_buffer_.size() << "/"
                                               << buffer_size_
                                               << " stalls=" << stalls_);
      }
    }

    // Phase 2: Check source valid and try to enqueue
    if (src_ports_.size() > 0) {
      auto src_port = src_ports_[0];
      source_has_valid = src_port->hasData();

      if (source_has_valid && canAcceptData()) {
        // Source has valid data and buffer has space
        auto data = src_port->read();
        if (data && data->isValid()) {
          data_buffer_.push(data);
          TRACE_EVENT(scheduler_.getCurrentTime(), name_, "CONN_ENQUEUE",
                      "valid=1 ready=1, buffer=" << data_buffer_.size() << "/"
                                                 << buffer_size_);
        }
      } else if (source_has_valid && !canAcceptData()) {
        // Buffer full, source must wait
        TRACE_EVENT(scheduler_.getCurrentTime(), name_, "CONN_BACK_PRESSURE",
                    "Buffer full, back pressure applied");
      }
    }
  }

  /**
   * @brief Print statistics
   */
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

 protected:
  void schedulePropagate(uint64_t time) {
    if (!enabled_) return;

    auto propagate_event = std::make_shared<EventDriven::LambdaEvent>(
        time,
        [this](EventDriven::EventScheduler& sched) {
          if (!enabled_) return;

          propagate();

          // Schedule next propagate
          schedulePropagate(sched.getCurrentTime() + period_);
        },
        1, name_ + "_Propagate");  // Priority 1 (higher than components' 0)

    scheduler_.schedule(propagate_event);
  }

  uint64_t period_;     // Propagation period
  bool enabled_;        // Connection enabled flag
  size_t buffer_size_;  // Internal buffer size
  std::queue<std::shared_ptr<DataPacket>> data_buffer_;  // FIFO buffer
  uint64_t transfers_;  // Successful transfers count
  uint64_t stalls_;     // Stall cycles count
};

}  // namespace Architecture

#endif  // READY_VALID_CONNECTION_H
