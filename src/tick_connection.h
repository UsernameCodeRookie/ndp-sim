#ifndef TICK_CONNECTION_H
#define TICK_CONNECTION_H

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "connection.h"
#include "event_lambda.h"
#include "packet.h"
#include "scheduler.h"

namespace Architecture {

/**
 * @brief TickingConnection class
 *
 * A connection that propagates data on each clock cycle
 * Automatically schedules propagate events at regular intervals
 */
class TickingConnection : public Connection {
 public:
  TickingConnection(const std::string& name,
                    EventDriven::EventScheduler& scheduler, uint64_t period)
      : Connection(name, scheduler), period_(period), enabled_(true) {}

  virtual ~TickingConnection() = default;

  // Start the ticking connection
  void start(uint64_t start_time = 0) {
    enabled_ = true;
    schedulePropagate(start_time);
  }

  // Stop the ticking connection
  void stop() { enabled_ = false; }

  // Get period
  uint64_t getPeriod() const { return period_; }
  bool isEnabled() const { return enabled_; }

  // Propagate data from source ports to destination ports
  void propagate() override {
    // Collect data from all source ports (consume the data)
    std::vector<std::shared_ptr<DataPacket>> data_to_send;

    for (auto& src_port : src_ports_) {
      if (src_port->hasData()) {
        // Use read() to consume the data from source port
        auto data = src_port->read();
        if (data) {
          data_to_send.push_back(data);
        }
      }
    }

    // If there is data to send, propagate to destination ports
    if (!data_to_send.empty()) {
      // Handle latency
      if (latency_ > 0) {
        // Schedule delayed propagation
        // Explicitly capture copies to avoid lifetime issues
        auto dst_ports_copy = dst_ports_;
        auto propagate_event = std::make_shared<EventDriven::LambdaEvent>(
            scheduler_.getCurrentTime() + latency_,
            [dst_ports_copy, data_to_send](EventDriven::EventScheduler&) {
              for (auto& dst_port : dst_ports_copy) {
                // For simplicity, broadcast first available data
                // More complex routing logic can be implemented in derived
                // classes
                if (!data_to_send.empty()) {
                  dst_port->setData(data_to_send[0]->clone());
                }
              }
            },
            0, name_ + "_PropagateDelayed");
        scheduler_.schedule(propagate_event);
      } else {
        // Immediate propagation
        for (auto& dst_port : dst_ports_) {
          if (!data_to_send.empty()) {
            dst_port->setData(data_to_send[0]->clone());
          }
        }
      }
    }
  }

 protected:
  void schedulePropagate(uint64_t time) {
    if (!enabled_) return;

    auto propagate_event = std::make_shared<EventDriven::LambdaEvent>(
        time,
        [this](EventDriven::EventScheduler& sched) {
          if (!enabled_) return;

          // Execute propagate logic
          propagate();

          // Schedule next propagate
          schedulePropagate(sched.getCurrentTime() + period_);
        },
        0, name_ + "_Propagate");

    scheduler_.schedule(propagate_event);
  }

  uint64_t period_;  // Propagate period (cycle time)
  bool enabled_;     // Whether the connection is active
};

}  // namespace Architecture

#endif  // TICK_CONNECTION_H
