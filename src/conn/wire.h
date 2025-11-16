#ifndef WIRE_H
#define WIRE_H

#include <cstdint>
#include <memory>
#include <string>

#include "../tick.h"

namespace Architecture {

/**
 * @brief WireConnection class - Simplest connection for single port to single
 * port
 *
 * This is the most basic connection that directly transfers data from a single
 * source port to a single destination port on each clock cycle.
 *
 * - No buffering: data flows directly through
 * - No handshaking: no ready/valid signals
 * - No flow control: always transfers if data is available
 * - Supports optional latency for simulating wire delay
 */
class WireConnection : public TickingConnection {
 public:
  WireConnection(const std::string& name,
                 EventDriven::EventScheduler& scheduler, uint64_t period)
      : TickingConnection(name, scheduler, period), transfers_(0) {}

  virtual ~WireConnection() = default;

  // Getters
  uint64_t getTransfers() const { return transfers_; }

  /**
   * @brief Propagate data directly from source to destination
   *
   * Simple protocol:
   * 1. Read data from source port (if available)
   * 2. Write data to destination port
   */
  void propagate() override {
    if (src_ports_.empty() || dst_ports_.empty()) return;

    // Check if source port has data
    if (src_ports_[0]->hasData()) {
      auto data = src_ports_[0]->read();
      if (data && data->valid) {
        deliverData(dst_ports_[0], data);
        transfers_++;

        TRACE_EVENT(scheduler_.getCurrentTime(), name_, "WIRE_TRANSFER",
                    "transfers=" << transfers_);
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
};

}  // namespace Architecture

#endif  // WIRE_H
