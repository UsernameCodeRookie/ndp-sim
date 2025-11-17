#ifndef EVENT_H
#define EVENT_H

#include <cstdint>
#include <functional>
#include <memory>
#include <string>

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
   * Earlier events have higher priority, when time is the same, HIGHER priority
   * number comes first (higher priority)
   */
  bool operator<(const Event& other) const {
    if (time_ != other.time_) {
      return time_ > other.time_;  // Min heap, earlier time comes first
    }
    return priority_ > other.priority_;  // HIGHER priority NUMBER comes first
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

  void execute(EventScheduler& scheduler) override;

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
