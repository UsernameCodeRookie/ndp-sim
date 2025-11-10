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
 * Connection with back pressure support using ready/valid handshake protocol
 * - valid: Source indicates data is available
 * - ready: Destination indicates it can accept data
 * - Data transfer occurs when both ready and valid are high
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
        source_valid_(false),
        dest_ready_(true),
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
  bool isSourceValid() const { return source_valid_; }
  bool isDestReady() const { return dest_ready_; }
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
   */
  void propagate() override {
    // Phase 1: Check source valid - try to enqueue data from source ports
    source_valid_ = false;
    for (auto& src_port : src_ports_) {
      if (src_port->hasData() && canAcceptData()) {
        auto data = src_port->read();  // Consume data
        if (data) {
          data_buffer_.push(data);
          source_valid_ = true;
          if (verbose_) {
            std::cout << "[" << scheduler_.getCurrentTime() << "] " << name_
                      << ": Enqueued data, buffer=" << data_buffer_.size()
                      << "/" << buffer_size_ << std::endl;
          }
        }
      }
    }

    // Phase 2: Check destination ready - try to dequeue data to destination
    dest_ready_ = true;
    for (auto& dst_port : dst_ports_) {
      // Check if destination already has data (not ready)
      if (dst_port->hasData()) {
        dest_ready_ = false;
      }
    }

    // Phase 3: Perform handshake if both valid and ready
    if (hasDataToSend() && dest_ready_) {
      auto data = data_buffer_.front();
      data_buffer_.pop();

      // Handle latency
      if (latency_ > 0) {
        auto dst_ports_copy = dst_ports_;
        auto data_copy = data;
        auto propagate_event = std::make_shared<EventDriven::LambdaEvent>(
            scheduler_.getCurrentTime() + latency_,
            [dst_ports_copy, data_copy](EventDriven::EventScheduler&) {
              for (auto& dst_port : dst_ports_copy) {
                dst_port->setData(data_copy->clone());
              }
            },
            0, name_ + "_PropagateDelayed");
        scheduler_.schedule(propagate_event);
      } else {
        // Immediate propagation
        for (auto& dst_port : dst_ports_) {
          dst_port->setData(data->clone());
        }
      }

      transfers_++;
      if (verbose_) {
        std::cout << "[" << scheduler_.getCurrentTime() << "] " << name_
                  << ": Transferred data, buffer=" << data_buffer_.size() << "/"
                  << buffer_size_ << std::endl;
      }
    } else if (hasDataToSend() && !dest_ready_) {
      // Stall: have data but destination not ready
      stalls_++;
      if (verbose_) {
        std::cout << "[" << scheduler_.getCurrentTime() << "] " << name_
                  << ": Stalled (dest not ready)" << std::endl;
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
    std::cout << "Utilization: "
              << (transfers_ + stalls_ > 0
                      ? (100.0 * transfers_ / (transfers_ + stalls_))
                      : 0.0)
              << "%" << std::endl;
  }

  // Enable/disable verbose output
  void setVerbose(bool verbose) { verbose_ = verbose; }

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
        0, name_ + "_Propagate");

    scheduler_.schedule(propagate_event);
  }

  uint64_t period_;     // Propagation period
  bool enabled_;        // Connection enabled flag
  size_t buffer_size_;  // Internal buffer size
  std::queue<std::shared_ptr<DataPacket>> data_buffer_;  // FIFO buffer
  bool source_valid_;     // Source has valid data
  bool dest_ready_;       // Destination is ready
  uint64_t transfers_;    // Successful transfers count
  uint64_t stalls_;       // Stall cycles count
  bool verbose_ = false;  // Verbose output flag
};

}  // namespace Architecture

#endif  // READY_VALID_CONNECTION_H
