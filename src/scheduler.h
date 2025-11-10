#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <cstdint>
#include <functional>
#include <iostream>
#include <memory>
#include <queue>
#include <string>
#include <vector>

#include "event_base.h"
#include "event_lambda.h"

namespace EventDriven {

/**
 * @brief Event scheduler
 *
 * Manages event queue and executes events in time order
 */
class EventScheduler {
 public:
  EventScheduler() : current_time_(0), event_count_(0), verbose_(false) {}

  /**
   * @brief Schedule an event
   * @param event Smart pointer to the event
   */
  void schedule(std::shared_ptr<Event> event) {
    if (event && event->getTime() >= current_time_) {
      event_queue_.push(event);
      event_count_++;

      if (verbose_) {
        std::cout << "[Schedule] Event '" << event->getName()
                  << "' (ID: " << event->getID() << ") at time "
                  << event->getTime() << std::endl;
      }
    } else if (event && event->getTime() < current_time_) {
      std::cerr << "[Warning] Cannot schedule event '" << event->getName()
                << "' in the past (time: " << event->getTime()
                << ", current: " << current_time_ << ")" << std::endl;
    }
  }

  /**
   * @brief Schedule a lambda event
   * @param time Event time
   * @param callback Callback function
   * @param priority Priority
   * @param name Event name
   */
  void scheduleAt(uint64_t time, std::function<void(EventScheduler&)> callback,
                  int priority = 0, const std::string& name = "LambdaEvent") {
    auto event = std::make_shared<LambdaEvent>(time, callback, priority, name);
    schedule(event);
  }

  /**
   * @brief Run simulation until no events or maximum time reached
   * @param max_time Maximum simulation time (0 means unlimited)
   */
  void run(uint64_t max_time = 0) {
    if (verbose_) {
      std::cout << "\n=== Starting Event Simulation ===" << std::endl;
      std::cout << "Total events scheduled: " << event_queue_.size()
                << std::endl;
    }

    while (!event_queue_.empty()) {
      auto event = event_queue_.top();
      event_queue_.pop();

      // Check if maximum time is exceeded
      if (max_time > 0 && event->getTime() > max_time) {
        if (verbose_) {
          std::cout << "\n[Stop] Reached maximum simulation time: " << max_time
                    << std::endl;
        }
        break;
      }

      // Update current time
      current_time_ = event->getTime();

      // Skip cancelled events
      if (event->isCancelled()) {
        if (verbose_) {
          std::cout << "[Skip] Cancelled event '" << event->getName()
                    << "' (ID: " << event->getID() << ")" << std::endl;
        }
        continue;
      }

      // Execute event
      if (verbose_) {
        std::cout << "\n[Time: " << current_time_ << "] Executing event '"
                  << event->getName() << "' (ID: " << event->getID() << ")"
                  << std::endl;
      }

      event->execute(*this);
    }

    if (verbose_) {
      std::cout << "\n=== Simulation Complete ===" << std::endl;
      std::cout << "Final time: " << current_time_ << std::endl;
      std::cout << "Total events processed: " << event_count_ << std::endl;
    }
  }

  /**
   * @brief Run a specified number of events
   * @param count Number of events to execute
   */
  void runFor(size_t count) {
    size_t executed = 0;
    while (!event_queue_.empty() && executed < count) {
      auto event = event_queue_.top();
      event_queue_.pop();

      current_time_ = event->getTime();

      if (event->isCancelled()) {
        continue;
      }

      event->execute(*this);
      executed++;
    }
  }

  /**
   * @brief Run until specified time
   * @param until_time Target time
   */
  void runUntil(uint64_t until_time) { run(until_time); }

  // Getter methods
  uint64_t getCurrentTime() const { return current_time_; }
  size_t getPendingEventCount() const { return event_queue_.size(); }
  size_t getTotalEventCount() const { return event_count_; }

  // Reset scheduler
  void reset() {
    while (!event_queue_.empty()) {
      event_queue_.pop();
    }
    current_time_ = 0;
    event_count_ = 0;
  }

  // Set verbose mode
  void setVerbose(bool verbose) { verbose_ = verbose; }
  bool isVerbose() const { return verbose_; }

 private:
  // Event comparator for priority queue
  struct EventCompare {
    bool operator()(const std::shared_ptr<Event>& a,
                    const std::shared_ptr<Event>& b) const {
      return *a < *b;
    }
  };

  std::priority_queue<std::shared_ptr<Event>,
                      std::vector<std::shared_ptr<Event>>, EventCompare>
      event_queue_;
  uint64_t current_time_;  // Current simulation time
  size_t event_count_;     // Total number of scheduled events
  bool verbose_;           // Whether to output detailed information
};

}  // namespace EventDriven

#endif  // SCHEDULER_H
