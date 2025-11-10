#ifndef TICK_COMPONENT_H
#define TICK_COMPONENT_H

#include <cstdint>
#include <memory>
#include <string>

#include "component.h"
#include "event_lambda.h"
#include "scheduler.h"

namespace Architecture {

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
  void start(uint64_t start_time = 0) { scheduleTick(start_time); }

  // Stop the ticking component
  void stop() { enabled_ = false; }

  // Get tick period
  uint64_t getPeriod() const { return period_; }
  uint64_t getTickCount() const { return tick_count_; }

  // Tick function to be implemented by derived classes
  virtual void tick() = 0;

 protected:
  void scheduleTick(uint64_t time) {
    if (!enabled_) return;

    auto tick_event = std::make_shared<EventDriven::LambdaEvent>(
        time,
        [this](EventDriven::EventScheduler& sched) {
          if (!enabled_) return;

          // Execute tick logic
          tick();
          tick_count_++;

          // Schedule next tick
          scheduleTick(sched.getCurrentTime() + period_);
        },
        0, name_ + "_Tick");

    scheduler_.schedule(tick_event);
  }

  uint64_t period_;      // Tick period (cycle time)
  uint64_t tick_count_;  // Number of ticks executed
};

}  // namespace Architecture

#endif  // TICK_COMPONENT_H
