#ifndef TICK_H
#define TICK_H

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "component.h"
#include "connection.h"
#include "event.h"
#include "packet.h"
#include "scheduler.h"
#include "trace.h"

namespace Architecture {

/**
 * @brief Event priority levels for scheduling
 */
constexpr int EVENT_PRIORITY_COMPONENT = 0;  // Component ticks
constexpr int EVENT_PRIORITY_CONNECTION =
    1;  // Connection propagates (before components)

/**
 * @brief TickingComponent class
 *
 * A component that operates on a periodic tick cycle
 * Automatically schedules tick events at regular intervals
 */
class TickingComponent : public Component {
 public:
  TickingComponent(const std::string& name,
                   EventDriven::EventScheduler& scheduler, uint64_t period)
      : Component(name, scheduler), period_(period), tick_count_(0) {}

  virtual ~TickingComponent() = default;

  // Start the ticking component
  void start(uint64_t start_time = 0) { schedule(start_time); }

  // Stop the ticking component
  void stop() { enabled_ = false; }

  // Get tick period
  uint64_t getPeriod() const { return period_; }
  uint64_t getTickCount() const { return tick_count_; }

  // Tick function to be implemented by derived classes
  virtual void tick() = 0;

 protected:
  void schedule(uint64_t time) {
    if (!enabled_) return;

    auto tick_event = std::make_shared<EventDriven::LambdaEvent>(
        time,
        [this](EventDriven::EventScheduler& sched) {
          if (!enabled_) return;

          // Trace tick event
          EventDriven::Tracer::getInstance().traceTick(
              sched.getCurrentTime(), name_,
              "tick_count=" + std::to_string(tick_count_));

          // Execute tick logic
          tick();
          tick_count_++;

          // Schedule next tick
          schedule(sched.getCurrentTime() + period_);
        },
        EVENT_PRIORITY_COMPONENT, name_ + "_Tick");

    scheduler_.schedule(tick_event);
  }

  uint64_t period_;      // Tick period (cycle time)
  uint64_t tick_count_;  // Number of ticks executed
};

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
    schedule(start_time);
  }

  // Stop the ticking connection
  void stop() { enabled_ = false; }

  // Get period
  uint64_t getPeriod() const { return period_; }
  bool isEnabled() const { return enabled_; }

  // Propagate data from source ports to destination ports
  virtual void propagate() {};

 protected:
  void schedule(uint64_t time) {
    if (!enabled_) return;

    auto propagate_event = std::make_shared<EventDriven::LambdaEvent>(
        time,
        [this](EventDriven::EventScheduler& sched) {
          if (!enabled_) return;

          // Trace propagate event
          EventDriven::Tracer::getInstance().tracePropagate(
              sched.getCurrentTime(), name_,
              "src_ports=" + std::to_string(src_ports_.size()) +
                  " dst_ports=" + std::to_string(dst_ports_.size()));

          // Execute propagate logic
          propagate();

          // Schedule next propagate
          schedule(sched.getCurrentTime() + period_);
        },
        EVENT_PRIORITY_CONNECTION, name_ + "_Propagate");

    scheduler_.schedule(propagate_event);
  }

  uint64_t period_;  // Propagate period (cycle time)
  bool enabled_;     // Whether the connection is active
};

}  // namespace Architecture

#endif  // TICK_H
