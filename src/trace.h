#ifndef TRACE_H
#define TRACE_H

#include <chrono>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <vector>

namespace EventDriven {

/**
 * @brief Trace event types
 */
enum class TraceEventType {
  TICK,             // Component tick event
  EVENT,            // Generic event
  COMPUTE,          // Computation event
  MEMORY_READ,      // Memory read access
  MEMORY_WRITE,     // Memory write access
  COMMUNICATION,    // Communication/data transfer
  STATE_CHANGE,     // Component state change
  INSTRUCTION,      // Instruction execution
  MAC_OPERATION,    // MAC operation
  REGISTER_ACCESS,  // Register read/write
  QUEUE_OPERATION,  // Queue operations
  PROPAGATE,        // Connection propagation
  CUSTOM            // Custom trace event
};

/**
 * @brief Trace entry structure
 */
struct TraceEntry {
  uint64_t timestamp;          // Simulation time
  TraceEventType type;         // Event type
  std::string component_name;  // Component name
  std::string event_name;      // Event description
  std::string details;         // Detailed information
  int priority;                // Event priority

  TraceEntry(uint64_t ts, TraceEventType t, const std::string& comp,
             const std::string& evt, const std::string& det = "", int prio = 0)
      : timestamp(ts),
        type(t),
        component_name(comp),
        event_name(evt),
        details(det),
        priority(prio) {}
};

/**
 * @brief Singleton Tracer class
 *
 * Thread-safe tracer for logging simulation events
 * Automatically dumps trace file to data directory
 */
class Tracer {
 public:
  // Get singleton instance
  static Tracer& getInstance() {
    static Tracer instance;
    return instance;
  }

  // Delete copy and move constructors
  Tracer(const Tracer&) = delete;
  Tracer& operator=(const Tracer&) = delete;
  Tracer(Tracer&&) = delete;
  Tracer& operator=(Tracer&&) = delete;

  /**
   * @brief Initialize tracer with output file
   * @param filename Output trace filename (will be placed in data/ directory)
   * @param enable Enable/disable tracing
   */
  void initialize(const std::string& filename = "", bool enable = true) {
    std::lock_guard<std::mutex> lock(mutex_);
    enabled_ = enable;

    if (!enabled_) return;

    // Generate filename with timestamp if not provided
    std::string trace_filename = filename;
    if (trace_filename.empty()) {
      auto now = std::chrono::system_clock::now();
      auto time_t = std::chrono::system_clock::to_time_t(now);
      std::stringstream ss;
      ss << "trace_" << std::put_time(std::localtime(&time_t), "%Y%m%d_%H%M%S")
         << ".log";
      trace_filename = ss.str();
    }

    // Ensure data directory exists (will be created if needed)
    output_path_ = "data/" + trace_filename;

    // Clear previous entries
    entries_.clear();
    initialized_ = true;
  }

  /**
   * @brief Add component name pattern to filter list
   * Only components matching these patterns will be traced
   * Empty list = trace all components
   */
  void addComponentFilter(const std::string& pattern) {
    std::lock_guard<std::mutex> lock(mutex_);
    component_filters_.push_back(pattern);
  }

  /**
   * @brief Clear component filters (trace all components)
   */
  void clearComponentFilters() {
    std::lock_guard<std::mutex> lock(mutex_);
    component_filters_.clear();
  }

  /**
   * @brief Check if component should be traced based on filters
   */
  bool shouldTraceComponent(const std::string& component_name) const {
    // If no filters set, trace everything
    if (component_filters_.empty()) return true;

    // Check if component name matches any filter pattern
    for (const auto& filter : component_filters_) {
      if (component_name.find(filter) != std::string::npos) {
        return true;
      }
    }
    return false;
  }

  /**
   * @brief Record a trace event
   */
  void trace(uint64_t timestamp, TraceEventType type,
             const std::string& component_name, const std::string& event_name,
             const std::string& details = "", int priority = 0);

  // Tracing helpers with signatures matching usage in components
  void traceQueueOp(uint64_t timestamp, const std::string& component_name,
                    const std::string& event_name, size_t queue_size,
                    size_t queue_depth) {
    std::stringstream ss;
    ss << event_name << " size=" << queue_size << " depth=" << queue_depth;
    trace(timestamp, TraceEventType::QUEUE_OPERATION, component_name,
          "queue_op", ss.str());
  }

  void traceRegisterAccess(uint64_t timestamp,
                           const std::string& component_name, bool is_write,
                           int address, int value) {
    std::stringstream ss;
    ss << (is_write ? "WRITE" : "READ") << " addr=" << address
       << " value=" << value;
    trace(timestamp, TraceEventType::REGISTER_ACCESS, component_name,
          "register_access", ss.str());
  }

  void traceInstruction(uint64_t timestamp, const std::string& component_name,
                        const std::string& instr_name,
                        const std::string& details) {
    std::stringstream ss;
    ss << instr_name;
    if (!details.empty()) ss << " - " << details;
    trace(timestamp, TraceEventType::INSTRUCTION, component_name, "instruction",
          ss.str());
  }
  const std::string& getOutputPath() const { return output_path_; }

  /**
   * @brief Convenience methods for specific event types
   */
  void traceTick(uint64_t timestamp, const std::string& component_name,
                 const std::string& details = "") {
    trace(timestamp, TraceEventType::TICK, component_name, "tick", details);
  }

  void traceEvent(uint64_t timestamp, const std::string& component_name,
                  const std::string& event_name,
                  const std::string& details = "", int priority = 0) {
    trace(timestamp, TraceEventType::EVENT, component_name, event_name, details,
          priority);
  }

  void traceCompute(uint64_t timestamp, const std::string& component_name,
                    const std::string& operation, const std::string& details) {
    trace(timestamp, TraceEventType::COMPUTE, component_name, operation,
          details);
  }

  void traceMemoryRead(uint64_t timestamp, const std::string& component_name,
                       uint64_t address, int value) {
    std::stringstream ss;
    ss << "addr=0x" << std::hex << address << " value=" << std::dec << value;
    trace(timestamp, TraceEventType::MEMORY_READ, component_name, "memory_read",
          ss.str());
  }

  void traceMemoryWrite(uint64_t timestamp, const std::string& component_name,
                        uint64_t address, int value) {
    std::stringstream ss;
    ss << "addr=0x" << std::hex << address << " value=" << std::dec << value;
    trace(timestamp, TraceEventType::MEMORY_WRITE, component_name,
          "memory_write", ss.str());
  }

  void traceMAC(uint64_t timestamp, const std::string& component_name, int acc,
                int a, int b) {
    std::stringstream ss;
    ss << "acc=" << acc << " (prev + " << a << " * " << b << ")";
    trace(timestamp, TraceEventType::MAC_OPERATION, component_name, "MAC",
          ss.str());
  }

  void tracePropagate(uint64_t timestamp, const std::string& component_name,
                      const std::string& details = "") {
    trace(timestamp, TraceEventType::PROPAGATE, component_name, "propagate",
          details);
  }

  /**
   * @brief Dump all trace entries to file
   */
  void dump() {
    if (!enabled_ || !initialized_) return;

    std::lock_guard<std::mutex> lock(mutex_);

    // Create data directory if it doesn't exist
#ifdef _WIN32
    system("if not exist data mkdir data");
#else
    system("mkdir -p data");
#endif

    std::ofstream file(output_path_);
    if (!file.is_open()) {
      std::cerr << "Error: Cannot open trace file: " << output_path_ << "\n";
      return;
    }

    // Write header
    file << "# Trace Log - Total entries: " << entries_.size() << "\n";
    file << "# [Timestamp] [Type] [Component] [Event] [Details] [Priority]\n";
    file << std::string(80, '=') << "\n\n";

    // Write all entries
    for (const auto& entry : entries_) {
      writeEntry(file, entry);
    }

    file.close();

    if (verbose_) {
      std::cout << "[Tracer] Dumped " << entries_.size()
                << " entries to: " << output_path_ << "\n";
    }
  }

  /**
   * @brief Set verbose mode
   */
  void setVerbose(bool verbose) { verbose_ = verbose; }

  /**
   * @brief Enable/disable immediate flush (for debugging)
   */
  void setImmediateFlush(bool enable) { immediate_flush_ = enable; }

  /**
   * @brief Enable/disable tracing
   */
  void setEnabled(bool enable) { enabled_ = enable; }

  /**
   * @brief Get statistics
   */
  size_t getEntryCount() const { return entries_.size(); }
  bool isEnabled() const { return enabled_; }

  /**
   * @brief Clear all trace entries
   */
  void clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    entries_.clear();
  }

 private:
  Tracer()
      : enabled_(false),
        initialized_(false),
        verbose_(false),
        immediate_flush_(false),
        output_path_("data/trace.log") {}

  ~Tracer() {
    // Auto dump on destruction
    if (enabled_ && initialized_ && !entries_.empty()) {
      dump();
    }
  }

  // Convert event type to string
  static const char* eventTypeToString(TraceEventType type) {
    switch (type) {
      case TraceEventType::TICK:
        return "TICK";
      case TraceEventType::EVENT:
        return "EVENT";
      case TraceEventType::COMPUTE:
        return "COMPUTE";
      case TraceEventType::MEMORY_READ:
        return "MEM_READ";
      case TraceEventType::MEMORY_WRITE:
        return "MEM_WRITE";
      case TraceEventType::COMMUNICATION:
        return "COMM";
      case TraceEventType::STATE_CHANGE:
        return "STATE";
      case TraceEventType::INSTRUCTION:
        return "INSTR";
      case TraceEventType::MAC_OPERATION:
        return "MAC";
      case TraceEventType::REGISTER_ACCESS:
        return "REG";
      case TraceEventType::QUEUE_OPERATION:
        return "QUEUE";
      case TraceEventType::PROPAGATE:
        return "PROP";
      case TraceEventType::CUSTOM:
        return "CUSTOM";
      default:
        return "UNKNOWN";
    }
  }

  // Write a single entry to file
  void writeEntry(std::ofstream& file, const TraceEntry& entry) {
    file << "[" << std::setw(10) << std::setfill(' ') << entry.timestamp
         << "] ";
    file << "[" << std::setw(10) << std::left << eventTypeToString(entry.type)
         << "] ";
    file << "[" << std::setw(20) << entry.component_name << "] ";
    file << "[" << std::setw(15) << entry.event_name << "] ";
    if (!entry.details.empty()) {
      file << entry.details;
    }
    if (entry.priority != 0) {
      file << " (priority=" << entry.priority << ")";
    }
    file << "\n";
  }

  // Flush a single entry immediately (for immediate_flush mode)
  void flushEntry(const TraceEntry& entry) {
    static std::ofstream file;

    if (!file.is_open()) {
#ifdef _WIN32
      system("if not exist data mkdir data");
#else
      system("mkdir -p data");
#endif
      file.open(output_path_);
      if (file.is_open()) {
        file << "# Trace Log - Immediate Flush Mode\n\n";
      }
    }

    if (file.is_open()) {
      writeEntry(file, entry);
      file.flush();
    }
  }

  bool enabled_;
  bool initialized_;
  bool verbose_;
  bool immediate_flush_;
  std::string output_path_;
  std::vector<TraceEntry> entries_;
  std::vector<std::string>
      component_filters_;  // Component name patterns to trace
  mutable std::mutex mutex_;
};

// Inline implementation of Tracer::trace
inline void Tracer::trace(uint64_t timestamp, TraceEventType type,
                          const std::string& component_name,
                          const std::string& event_name,
                          const std::string& details, int priority) {
  if (!enabled_ || !initialized_) return;

  // Filter by component if filters are set
  if (!shouldTraceComponent(component_name)) return;

  std::lock_guard<std::mutex> lock(mutex_);
  entries_.emplace_back(timestamp, type, component_name, event_name, details,
                        priority);

  // Optional: immediate flush for debugging (can be disabled for performance)
  if (immediate_flush_) {
    flushEntry(entries_.back());
  }
}

}  // namespace EventDriven

// ===========================================================================
// Trace Macros - Convenience macros for cleaner trace calls in components
// ===========================================================================

// Macro for tracing events
#define TRACE_EVENT(time, component, event_type, details)                      \
  do {                                                                         \
    std::stringstream _ss;                                                     \
    _ss << details;                                                            \
    EventDriven::Tracer::getInstance().traceEvent(time, component, event_type, \
                                                  _ss.str());                  \
  } while (0)

// Macro for tracing compute operations
#define TRACE_COMPUTE(time, component, op_type, details)                      \
  do {                                                                        \
    std::stringstream _ss;                                                    \
    _ss << details;                                                           \
    EventDriven::Tracer::getInstance().traceCompute(time, component, op_type, \
                                                    _ss.str());               \
  } while (0)

// Macro for tracing instructions
#define TRACE_INSTRUCTION(time, component, instr, details)                 \
  do {                                                                     \
    std::stringstream _ss;                                                 \
    _ss << details;                                                        \
    EventDriven::Tracer::getInstance().traceInstruction(time, component,   \
                                                        instr, _ss.str()); \
  } while (0)

// Macro for tracing MAC operations
#define TRACE_MAC(time, component, acc, a, b, details_unused) \
  EventDriven::Tracer::getInstance().traceMAC(time, component, acc, a, b)

// Macro for tracing memory read operations
#define TRACE_MEM_READ(time, component, addr, value)                        \
  EventDriven::Tracer::getInstance().traceMemoryRead(time, component, addr, \
                                                     value)

// Macro for tracing memory write operations
#define TRACE_MEM_WRITE(time, component, addr, value)                        \
  EventDriven::Tracer::getInstance().traceMemoryWrite(time, component, addr, \
                                                      value)

// Macro for tracing queue operations
#define TRACE_QUEUE_OP(time, component, op, size, depth)                     \
  EventDriven::Tracer::getInstance().traceQueueOp(time, component, op, size, \
                                                  depth)

#endif  // TRACE_H
