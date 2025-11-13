#ifndef CREDIT_H
#define CREDIT_H

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
 * @brief CreditConnection class - Composite connection for credit-based
 * flow-control
 *
 * This connection implements a credit-based flow-control scheme in which the
 * destination publishes the number of credits (free buffer slots) it has to
 * source components via a credit port (IntDataPacket). The connection reads
 * credits and maintains an internal counter to avoid over-committing.
 *
 * Behavior:
 * - On propagate(), update internal credits from credit_port_ if available.
 * - Phase 1: Deliver any buffered data to destination ports if they can
 *   accept it (destination port has no data).
 * - Phase 2: Enqueue data from source ports only if credits_ > 0 and there is
 *   buffer space. Decrement internal credits_ on enqueue.
 */
class CreditConnection : public Connection {
 public:
  CreditConnection(const std::string& name,
                   EventDriven::EventScheduler& scheduler, uint64_t period,
                   size_t buffer_size = 2)
      : Connection(name, scheduler),
        period_(period),
        enabled_(true),
        buffer_size_(buffer_size),
        transfers_(0),
        stalls_(0),
        credits_(0),
        credit_port_(nullptr) {}

  virtual ~CreditConnection() = default;

  /** Bind a port as the credit port (destination -> source) */
  void bindCreditPort(std::shared_ptr<Port> port) { credit_port_ = port; }

  void start(uint64_t start_time = 0) {
    if (!credit_port_) {
      throw std::runtime_error("CreditConnection " + name_ +
                               ": credit_port must be bound before starting. "
                               "Use bindCreditPort().");
    }
    enabled_ = true;
    schedulePropagate(start_time);
  }

  void stop() { enabled_ = false; }

  uint64_t getPeriod() const { return period_; }
  bool isEnabled() const { return enabled_; }
  size_t getBufferOccupancy() const { return data_buffer_.size(); }
  size_t getBufferSize() const { return buffer_size_; }
  uint64_t getTransfers() const { return transfers_; }
  uint64_t getStalls() const { return stalls_; }
  int64_t getCredits() const { return credits_; }

  void propagate() override {
    // Update internal credit counter from credit port (if available)
    updateCreditsFromPort();

    tryTransferToDestination();
    tryEnqueueFromSource();
  }

  void printStatistics() const {
    std::cout << "\n=== Connection Statistics: " << name_
              << " ===" << std::endl;
    std::cout << "Total transfers: " << transfers_ << std::endl;
    std::cout << "Total stalls: " << stalls_ << std::endl;
    std::cout << "Buffer occupancy: " << data_buffer_.size() << "/"
              << buffer_size_ << std::endl;
    std::cout << "Credits (last seen): " << credits_ << std::endl;
  }

 private:
  bool canAcceptData() const { return data_buffer_.size() < buffer_size_; }
  bool hasDataToSend() const { return !data_buffer_.empty(); }

  int64_t readCredits(std::shared_ptr<Port> port) const {
    if (!port) return -1;
    auto data = std::dynamic_pointer_cast<IntDataPacket>(port->getData());
    if (data && data->isValid()) {
      return static_cast<int64_t>(data->getValue());
    }
    return -1;
  }

  void updateCreditsFromPort() {
    if (!credit_port_) return;
    auto new_credits = readCredits(credit_port_);
    if (new_credits >= 0) {
      credits_ = new_credits;
    }
  }

  void tryTransferToDestination() {
    if (dst_ports_.empty() || !hasDataToSend()) return;

    // Only deliver if destination port is free (no data currently present)
    if (dst_ports_[0]->hasData()) {
      // Destination can't accept now; don't count this as a stall directly
      return;
    }

    deliverData(dst_ports_[0], data_buffer_.front());
    data_buffer_.pop();
    transfers_++;

    TRACE_EVENT(scheduler_.getCurrentTime(), name_, "CONN_TRANSFER",
                "credits=" << credits_ << " buffer=" << data_buffer_.size()
                           << "/" << buffer_size_
                           << " transfers=" << transfers_);
  }

  void tryEnqueueFromSource() {
    if (src_ports_.empty()) return;

    // Read credit snapshot - but prefer to use internal credits_ counter
    // We need credits_ > 0 in order to accept new data
    if (credits_ <= 0 && !canAcceptData()) {
      stalls_++;
      TRACE_EVENT(
          scheduler_.getCurrentTime(), name_, "CONN_STALL",
          "credits=0 buffer=" << data_buffer_.size() << "/" << buffer_size_);
      return;
    }

    // If there is already data in the src port, try to read and enqueue
    if (src_ports_[0]->hasData() && canAcceptData() && credits_ > 0) {
      auto data = src_ports_[0]->read();
      if (data && data->isValid()) {
        data_buffer_.push(data);
        // Decrement local credit counter (avoid overcommit between updates)
        credits_--;

        TRACE_EVENT(scheduler_.getCurrentTime(), name_, "CONN_ENQUEUE",
                    "credits=" << credits_ << " buffer=" << data_buffer_.size()
                               << "/" << buffer_size_);
      }
    } else if (src_ports_[0]->hasData() && !canAcceptData()) {
      stalls_++;
      TRACE_EVENT(scheduler_.getCurrentTime(), name_, "CONN_BACK_PRESSURE",
                  "Buffer full, back pressure applied");
    } else if (src_ports_[0]->hasData() && credits_ <= 0) {
      stalls_++;
      TRACE_EVENT(scheduler_.getCurrentTime(), name_, "CONN_NO_CREDIT",
                  "Credit unavailable, cannot enqueue");
    }
  }

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

  uint64_t period_;
  bool enabled_;
  size_t buffer_size_;  // Internal FIFO buffer size
  std::queue<std::shared_ptr<DataPacket>> data_buffer_;
  uint64_t transfers_;  // Successful transfers
  uint64_t stalls_;     // Stall cycles

  int64_t credits_;                    // Internal cached credit counter
  std::shared_ptr<Port> credit_port_;  // Port for credit values (IntDataPacket)
};

}  // namespace Architecture

#endif  // CREDIT_H
