#ifndef TRACE_H
#define TRACE_H

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <unordered_map>
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
 * @brief VCD signal definition
 */
struct VCDSignal {
  std::string name;        // Signal name
  std::string identifier;  // Single character identifier
  std::string type;        // wire/reg
  size_t width;            // Bit width
  std::string value;       // Current value

  VCDSignal() : name(""), identifier(""), type("wire"), width(32), value("0") {}

  VCDSignal(const std::string& n, const std::string& id, const std::string& t,
            size_t w, const std::string& v = "x")
      : name(n), identifier(id), type(t), width(w), value(v) {}
};

/**
 * @brief Singleton Tracer class with VCD output
 *
 * Thread-safe tracer for logging simulation events in VCD format
 * Automatically dumps trace file to data directory in GTKWave-compatible format
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
      auto time_val = std::chrono::system_clock::to_time_t(now);
      std::stringstream ss;
      ss << "trace_"
         << std::put_time(std::localtime(&time_val), "%Y%m%d_%H%M%S") << ".vcd";
      trace_filename = ss.str();
    } else {
      // Convert log extension to vcd if needed
      if (trace_filename.find(".log") != std::string::npos) {
        trace_filename.replace(trace_filename.find(".log"), 4, ".vcd");
      } else if (trace_filename.find(".vcd") == std::string::npos) {
        trace_filename += ".vcd";
      }
    }

    // Ensure data directory exists (will be created if needed)
    output_path_ = "data/" + trace_filename;

    // Clear previous entries
    entries_.clear();
    signals_.clear();
    signal_id_counter_ = 0;
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
   * @brief Dump all trace entries to VCD file
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

    // Write VCD header
    writeVCDHeader(file);

    // Initialize all signals to 0 at time 0
    for (auto& signal_pair : signals_) {
      auto& signal = signal_pair.second;
      file << "0" << signal.identifier << "\n";
    }

    // Write trace data in VCD format
    uint64_t last_timestamp = 0;
    uint64_t entries_written = 0;
    for (const auto& entry : entries_) {
      // Format value as hex - this extracts numeric values from details
      std::string hex_value = formatValueAsHex(entry.details);

      // Only write entries with valid numeric values
      if (!hex_value.empty()) {
        if (entry.timestamp != last_timestamp) {
          file << "#" << entry.timestamp << "\n";
          last_timestamp = entry.timestamp;
        }

        // Write signal value change with proper VCD format
        std::string signal_id =
            getOrCreateSignalId(entry.component_name, entry.event_name, 32);
        file << hex_value << signal_id << "\n";
        entries_written++;
      }
    }

    file.close();

    if (verbose_) {
      std::cout << "[Tracer] Dumped " << entries_written << " / "
                << entries_.size() << " entries to VCD file: " << output_path_
                << "\n";
      std::cout << "[Tracer] Total signals: " << signals_.size() << "\n";
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
        output_path_("data/trace.vcd"),
        signal_id_counter_(0) {}

  ~Tracer() {
    // Auto dump on destruction
    if (enabled_ && initialized_ && !entries_.empty()) {
      dump();
    }
  }

  // VCD identifier generation (uses single-char IDs like !, ", #, etc.)
  // Avoid special VCD characters that might break the format
  static const char* getVCDIdentifier(size_t index) {
    // Use alphanumeric characters only (GTKWave compatible)
    static const char* identifiers[] = {
        "a",  "b",  "c",  "d",  "e",  "f",  "g", "h", "i",  "j",  "k",
        "l",  "m",  "n",  "o",  "p",  "q",  "r", "s", "t",  "u",  "v",
        "w",  "x",  "y",  "z",  "A",  "B",  "C", "D", "E",  "F",  "G",
        "H",  "I",  "J",  "K",  "L",  "M",  "N", "O", "P",  "Q",  "R",
        "S",  "T",  "U",  "V",  "W",  "X",  "Y", "Z", "aa", "ab", "ac",
        "ad", "ae", "af", "ag", "ah", "ai", "aj"};
    return identifiers[index % 62];
  }

  // Get or create signal ID for a component/event pair
  std::string getOrCreateSignalId(const std::string& component,
                                  const std::string& event, size_t width) {
    std::string signal_name = component + "_" + event;
    if (signals_.find(signal_name) == signals_.end()) {
      std::string id = getVCDIdentifier(signal_id_counter_++);
      signals_[signal_name] = VCDSignal(signal_name, id, "wire", width, "0");
    }
    return signals_[signal_name].identifier;
  }

  // Convert details string to hex value in proper VCD format
  std::string formatValueAsHex(const std::string& details) {
    // Try to extract numeric value from details string
    // Format: "key=value" or just a number, or "key1=val1 key2=val2..."
    try {
      // First, try to find any hex value (0x prefix)
      size_t hex_pos = details.find("0x");
      if (hex_pos != std::string::npos) {
        // Extract hex value after 0x
        std::string hex_val = details.substr(hex_pos + 2);
        // Take only valid hex digits
        size_t end = hex_val.find_first_not_of("0123456789abcdefABCDEF ");
        if (end != std::string::npos) {
          hex_val = hex_val.substr(0, end);
        }
        // Remove spaces
        hex_val.erase(std::remove(hex_val.begin(), hex_val.end(), ' '),
                      hex_val.end());
        if (!hex_val.empty()) {
          // Pad with zeros to 8 digits max
          while (hex_val.length() < 8) hex_val = "0" + hex_val;
          if (hex_val.length() > 8) hex_val = hex_val.substr(0, 8);
          return hex_val;
        }
      }

      // Try to find any = sign and extract the first numeric value
      size_t eq_pos = details.find('=');
      if (eq_pos != std::string::npos) {
        std::string value_str = details.substr(eq_pos + 1);
        // Trim leading whitespace
        size_t start = value_str.find_first_not_of(" \t");
        if (start != std::string::npos) {
          value_str = value_str.substr(start);
          // Take only digits before space or special char
          size_t end = value_str.find_first_not_of("0123456789");
          if (end != std::string::npos) {
            value_str = value_str.substr(0, end);
          }
          if (!value_str.empty()) {
            long long value = std::stoll(value_str);
            if (value < 0) value = 0;
            std::stringstream ss;
            ss << std::setfill('0') << std::setw(8) << std::hex << value;
            return ss.str();
          }
        }
      }

      // Try to parse whole string as number
      size_t space_pos = details.find(' ');
      std::string first_token = (space_pos != std::string::npos)
                                    ? details.substr(0, space_pos)
                                    : details;
      long long value = std::stoll(first_token);
      if (value < 0) value = 0;
      std::stringstream ss;
      ss << std::setfill('0') << std::setw(8) << std::hex << value;
      return ss.str();
    } catch (...) {
      // If we can't parse, return empty (don't write invalid values)
      return "";
    }
  }

  // Write VCD file header
  void writeVCDHeader(std::ofstream& file) {
    // Write VCD standard header
    file << "$date\n";
    file << "  " << std::string(40, ' ') << "\n";

    auto now = std::chrono::system_clock::now();
    auto time_val = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time_val), "  %Y-%m-%d %H:%M:%S");
    file << ss.str() << "\n";
    file << "$end\n";

    file << "$version\n";
    file << "  NDP Simulator Event-Driven Tracer\n";
    file << "$end\n";

    file << "$timescale 1ps $end\n";

    // Write signal definitions
    file << "$scope module top $end\n";
    for (const auto& signal_pair : signals_) {
      const auto& signal = signal_pair.second;
      file << "$var wire " << signal.width << " " << signal.identifier << " "
           << signal.name << " $end\n";
    }
    file << "$upscope $end\n";

    file << "$enddefs\n";
    file << "#0\n";
  }

  bool enabled_;
  bool initialized_;
  bool verbose_;
  bool immediate_flush_;
  std::string output_path_;
  std::vector<TraceEntry> entries_;
  std::unordered_map<std::string, VCDSignal> signals_;
  size_t signal_id_counter_;
  std::vector<std::string> component_filters_;
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
