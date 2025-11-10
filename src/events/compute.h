#ifndef COMPUTE_H
#define COMPUTE_H

#include <iostream>
#include <memory>
#include <string>

#include "../event.h"
#include "../scheduler.h"

/**
 * @brief Custom event example: Compute event
 */
class ComputeEvent : public EventDriven::Event {
 public:
  ComputeEvent(uint64_t time, int value,
               const std::string& name = "ComputeEvent")
      : EventDriven::Event(time, 0, EventDriven::EventType::COMPUTE, name),
        value_(value) {}

  void execute(EventDriven::EventScheduler& scheduler) override {
    std::cout << "  Computing value: " << value_ << " * 2 = " << (value_ * 2)
              << std::endl;

    // Can schedule new events during event execution
    if (value_ < 100) {
      auto next_event = std::make_shared<ComputeEvent>(
          scheduler.getCurrentTime() + 10, value_ * 2, "ComputeEvent-Next");
      scheduler.schedule(next_event);
    }
  }

 private:
  int value_;
};

#endif  // COMPUTE_H
