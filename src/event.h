#ifndef EVENT_H
#define EVENT_H

#include <functional>
#include <iostream>
#include <memory>
#include <queue>
#include <string>
#include <vector>

namespace EventDriven {

// Forward declaration
class EventScheduler;

/**
 * @brief Event type enumeration
 */
enum class EventType { GENERIC, COMPUTE, MEMORY_ACCESS, COMMUNICATION, CUSTOM };

/**
 * @brief Event base class
 *
 * All events should inherit from this class and implement the execute() method
 */
class Event {
 public:
  using EventID = uint64_t;

  /**
   * @brief Constructor
   * @param time Event trigger time
   * @param priority Event priority (used when time is the same)
   * @param type Event type
   * @param name Event name
   */
  Event(uint64_t time, int priority = 0, EventType type = EventType::GENERIC,
        const std::string& name = "Event")
      : time_(time),
        priority_(priority),
        type_(type),
        name_(name),
        id_(next_id_++),
        cancelled_(false) {}

  virtual ~Event() = default;

  /**
   * @brief Execute the event logic
   * @param scheduler Reference to the event scheduler, allowing events to
   * schedule new events
   */
  virtual void execute(EventScheduler& scheduler) = 0;

  // Getter methods
  uint64_t getTime() const { return time_; }
  int getPriority() const { return priority_; }
  EventType getType() const { return type_; }
  const std::string& getName() const { return name_; }
  EventID getID() const { return id_; }
  bool isCancelled() const { return cancelled_; }

  // Cancel event
  void cancel() { cancelled_ = true; }

  /**
   * @brief Comparison operator for priority queue sorting
   * Earlier events have higher priority, when time is the same, higher priority
   * value comes first
   */
  bool operator<(const Event& other) const {
    if (time_ != other.time_) {
      return time_ > other.time_;  // Min heap, earlier time comes first
    }
    return priority_ < other.priority_;  // Higher priority value comes first
  }

 protected:
  uint64_t time_;     // Event trigger time
  int priority_;      // Priority
  EventType type_;    // Event type
  std::string name_;  // Event name
  EventID id_;        // Unique ID
  bool cancelled_;    // Whether cancelled

  static EventID next_id_;  // Next available ID
};

// Initialize static member
Event::EventID Event::next_id_ = 0;

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

#endif  // EVENT_H
