#ifndef DEBUGGER_H
#define DEBUGGER_H

#include <common.h>

// General-purpose event types
enum class EventType {
  Info,          // general information
  StateChange,   // internal state updated
  DataTransfer,  // data moved between modules
  Custom         // user-defined
};

// Debug event structure
struct DebugEvent {
  std::string module;            // Module name
  EventType type;                // Event type
  std::vector<uint32_t> values;  // Optional related values
  std::string msg;               // Optional message
};

// Abstract debugger interface
struct Debugger {
  virtual ~Debugger() = default;
  virtual void record(const DebugEvent& e) = 0;
};

#define DEBUG_EVENT(dbg, comp, etype, vals, msg) \
  do {                                           \
    DebugEvent __ev{comp, etype, vals, msg};     \
    DEBUG_RECORD(dbg, __ev);                     \
  } while (0)

#ifdef ENABLE_DEBUG
// Macro wrapper for recording events
#define DEBUG_RECORD(dbg, event)   \
  do {                             \
    if (dbg) dbg->record((event)); \
  } while (0)
#else
#define DEBUG_RECORD(dbg, event) \
  do {                           \
    (void)(event);               \
  } while (0)
#endif

struct PrintDebugger : public Debugger {
  void record(const DebugEvent& e) override {
    std::cout << "[" << e.module << "] ";
    switch (e.type) {
      case EventType::Info:
        std::cout << "Info";
        break;
      case EventType::StateChange:
        std::cout << "StateChange";
        break;
      case EventType::DataTransfer:
        std::cout << "DataTransfer";
        break;
      case EventType::Custom:
        std::cout << "Custom";
        break;
    }
    if (!e.msg.empty()) std::cout << " (" << e.msg << ")";
    if (!e.values.empty()) {
      std::cout << " values=[";
      for (size_t i = 0; i < e.values.size(); ++i) {
        std::cout << e.values[i];
        if (i + 1 < e.values.size()) std::cout << ",";
      }
      std::cout << "]";
    }
    std::cout << "\n";
  }
};

#endif  // DEBUGGER_H