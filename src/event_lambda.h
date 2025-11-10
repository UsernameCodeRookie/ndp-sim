#ifndef EVENT_LAMBDA_H
#define EVENT_LAMBDA_H

#include <functional>
#include <memory>

#include "event_base.h"

namespace EventDriven {

// Forward declaration
class EventScheduler;

/**
 * @brief Lambda event class
 *
 * Allows creating events using lambda functions or function objects
 */
class LambdaEvent : public Event {
 public:
  using Callback = std::function<void(EventScheduler&)>;

  LambdaEvent(uint64_t time, Callback callback, int priority = 0,
              const std::string& name = "LambdaEvent")
      : Event(time, priority, EventType::GENERIC, name),
        callback_(std::move(callback)) {}

  void execute(EventScheduler& scheduler) override {
    if (callback_) {
      callback_(scheduler);
    }
  }

 private:
  Callback callback_;
};

}  // namespace EventDriven

#endif  // EVENT_LAMBDA_H
