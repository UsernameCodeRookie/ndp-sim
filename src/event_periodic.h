#ifndef EVENT_PERIODIC_H
#define EVENT_PERIODIC_H

#include <cstdint>
#include <functional>
#include <memory>

#include "event_base.h"
#include "scheduler.h"

namespace EventDriven {

/**
 * @brief Periodic event class
 *
 * Automatically repeated event (supports shared pointers)
 */
class PeriodicEvent : public Event,
                      public std::enable_shared_from_this<PeriodicEvent> {
 public:
  using Callback = std::function<void(EventScheduler&, uint64_t)>;

  PeriodicEvent(uint64_t start_time, uint64_t period, Callback callback,
                uint64_t repeat_count = 0,
                const std::string& name = "PeriodicEvent")
      : Event(start_time, 0, EventType::GENERIC, name),
        period_(period),
        callback_(std::move(callback)),
        repeat_count_(repeat_count),
        execution_count_(0) {}

  void execute(EventScheduler& scheduler) override {
    if (callback_) {
      callback_(scheduler, execution_count_);
    }

    execution_count_++;

    // If still needs to repeat
    if (repeat_count_ == 0 || execution_count_ < repeat_count_) {
      // Create new event instance for next execution
      auto next_event = std::make_shared<PeriodicEvent>(
          time_ + period_, period_, callback_, repeat_count_, name_);
      next_event->execution_count_ = execution_count_;
      scheduler.schedule(next_event);
    }
  }

  uint64_t getPeriod() const { return period_; }
  uint64_t getExecutionCount() const { return execution_count_; }

 private:
  uint64_t period_;
  Callback callback_;
  uint64_t repeat_count_;
  uint64_t execution_count_;
};

}  // namespace EventDriven

#endif  // EVENT_PERIODIC_H
