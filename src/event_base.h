#ifndef EVENT_BASE_H
#define EVENT_BASE_H

#include <cstdint>
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

}  // namespace EventDriven

#endif  // EVENT_BASE_H
