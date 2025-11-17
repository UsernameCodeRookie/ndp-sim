#ifndef BUFFER_H
#define BUFFER_H

#include <cassert>
#include <deque>
#include <memory>
#include <sstream>
#include <string>

#include "packet.h"
#include "scheduler.h"

/**
 * @brief Buffer Mode Enumeration
 */
enum class BufferMode {
  FIFO,          // First-In-First-Out queue mode (push_back/pop_front)
  RANDOM_ACCESS  // Random access mode (instruction memory style)
};

/**
 * @brief Buffer Parameters
 *
 * Parameterizes buffer configuration for flexible queue/memory management
 */
struct BufferParameters {
  uint32_t depth;               // Buffer depth (number of entries)
  uint32_t data_width;          // Width of data in bits
  uint32_t num_read_ports;      // Number of read ports
  uint32_t num_write_ports;     // Number of write ports
  BufferMode mode;              // Buffer operation mode (FIFO or RANDOM_ACCESS)
  bool enable_bypass;           // Enable write-through bypass when empty
  bool enable_overflow_check;   // Enable overflow error checking
  bool enable_underflow_check;  // Enable underflow error checking
  uint32_t read_latency;        // Latency for read operations (cycles)
  uint32_t write_latency;       // Latency for write operations (cycles)

  BufferParameters(uint32_t depth_val = 32, uint32_t width = 32,
                   uint32_t num_rd_ports = 1, uint32_t num_wr_ports = 1,
                   BufferMode mode_val = BufferMode::FIFO, bool bypass = false,
                   bool overflow_check = true, bool underflow_check = true,
                   uint32_t rd_latency = 0, uint32_t wr_latency = 0)
      : depth(depth_val),
        data_width(width),
        num_read_ports(num_rd_ports),
        num_write_ports(num_wr_ports),
        mode(mode_val),
        enable_bypass(bypass),
        enable_overflow_check(overflow_check),
        enable_underflow_check(underflow_check),
        read_latency(rd_latency),
        write_latency(wr_latency) {
    // Validate configuration
    assert(depth > 0 && depth <= 16384);
    assert(data_width > 0 && data_width <= 1024);
    assert(num_read_ports > 0 && num_read_ports <= 16);
    assert(num_write_ports > 0 && num_write_ports <= 16);
  }
};

/**
 * @brief FIFO Buffer Entry
 *
 * Represents a single entry in the FIFO buffer with optional metadata
 */
class FIFOBufferEntry {
 public:
  FIFOBufferEntry(std::shared_ptr<Architecture::DataPacket> data = nullptr,
                  uint64_t arrival_time = 0)
      : data(data), arrival_time(arrival_time), valid(true) {}

  std::shared_ptr<Architecture::DataPacket> data;
  uint64_t arrival_time;  // When the entry was added to the buffer
  bool valid;
};

/**
 * @brief Buffer Component
 *
 * A parameterizable buffer that supports both FIFO and random access modes.
 * Supports:
 * - FIFO (queue) mode: push_back/pop_front operations
 * - Random Access mode: indexed load/fetch operations
 * - Configurable depth and data width
 * - Multiple read and write ports
 * - Write-through bypass mode
 * - Overflow/underflow detection
 * - Configurable read/write latencies
 */
class Buffer {
 public:
  Buffer(const std::string& name, EventDriven::EventScheduler& scheduler,
         const BufferParameters& params = BufferParameters())
      : name_(name), scheduler_(scheduler), params_(params) {
    initialize();
  }

  virtual ~Buffer() = default;

  // Getters
  const BufferParameters& getParameters() const { return params_; }
  BufferMode getMode() const { return params_.mode; }
  uint32_t getCurrentOccupancy() const { return fifo_queue_.size(); }
  uint32_t getAvailableSpace() const {
    return params_.depth - fifo_queue_.size();
  }
  bool isEmpty() const { return fifo_queue_.empty(); }
  bool isFull() const { return fifo_queue_.size() >= params_.depth; }

  /**
   * @brief Write data to the buffer (FIFO mode)
   *
   * For FIFO mode: Adds data to the end of the queue
   * For RANDOM_ACCESS mode: Not supported, use store() instead
   *
   * @param data Data packet to write
   * @param port_id ID of the write port (0 to num_write_ports-1)
   * @return true if write successful, false if buffer is full
   */
  bool write(std::shared_ptr<Architecture::DataPacket> data,
             uint32_t port_id = 0) {
    if (params_.mode != BufferMode::FIFO) {
      std::cerr << "[" << name_ << "] write() only supported in FIFO mode"
                << std::endl;
      return false;
    }

    if (port_id >= params_.num_write_ports) {
      std::cerr << "[" << name_ << "] Invalid write port ID: " << port_id
                << std::endl;
      return false;
    }

    if (isFull()) {
      if (params_.enable_overflow_check) {
        std::cerr << "[" << name_ << "] FIFO overflow on port " << port_id
                  << std::endl;
      }
      return false;
    }

    fifo_queue_.push_back(FIFOBufferEntry(data, scheduler_.getCurrentTime()));
    write_count_++;
    return true;
  }

  /**
   * @brief Read data from the buffer (FIFO mode)
   *
   * For FIFO mode: Removes and returns data from the front of the queue
   * For RANDOM_ACCESS mode: Not supported, use load() instead
   *
   * @param port_id ID of the read port (0 to num_read_ports-1)
   * @return Data packet if successful, nullptr if buffer is empty
   */
  std::shared_ptr<Architecture::DataPacket> read(uint32_t port_id = 0) {
    if (params_.mode != BufferMode::FIFO) {
      std::cerr << "[" << name_ << "] read() only supported in FIFO mode"
                << std::endl;
      return nullptr;
    }

    if (port_id >= params_.num_read_ports) {
      std::cerr << "[" << name_ << "] Invalid read port ID: " << port_id
                << std::endl;
      return nullptr;
    }

    if (isEmpty()) {
      if (params_.enable_underflow_check) {
        std::cerr << "[" << name_ << "] FIFO underflow on port " << port_id
                  << std::endl;
      }
      return nullptr;
    }

    FIFOBufferEntry entry = fifo_queue_.front();
    fifo_queue_.pop_front();
    read_count_++;
    return entry.data;
  }

  /**
   * @brief Peek at the front entry without removing it (FIFO mode)
   *
   * For FIFO mode: Returns data from the front without removing
   * For RANDOM_ACCESS mode: Not supported
   *
   * @param port_id ID of the read port
   * @return Data packet if available, nullptr if buffer is empty
   */
  std::shared_ptr<Architecture::DataPacket> peek(uint32_t port_id = 0) const {
    if (params_.mode != BufferMode::FIFO) {
      std::cerr << "[" << name_ << "] peek() only supported in FIFO mode"
                << std::endl;
      return nullptr;
    }

    if (port_id >= params_.num_read_ports) {
      std::cerr << "[" << name_ << "] Invalid read port ID: " << port_id
                << std::endl;
      return nullptr;
    }

    if (isEmpty()) {
      return nullptr;
    }

    return fifo_queue_.front().data;
  }

  /**
   * @brief Store data at a specific address (RANDOM_ACCESS mode)
   *
   * For RANDOM_ACCESS mode: Stores data at the specified index
   * For FIFO mode: Not supported, use write() instead
   *
   * @param address Index/address to store at
   * @param data Data to store (typically instruction as uint32_t)
   * @return true if successful
   */
  bool store(uint32_t address, uint32_t data) {
    if (params_.mode != BufferMode::RANDOM_ACCESS) {
      std::cerr << "[" << name_
                << "] store() only supported in RANDOM_ACCESS mode"
                << std::endl;
      return false;
    }

    uint32_t index = address / 4;  // Assuming 4-byte aligned addresses
    if (index >= params_.depth) {
      // Expand capacity if needed
      if (params_.enable_overflow_check) {
        std::cerr << "[" << name_ << "] Address out of bounds: " << address
                  << std::endl;
      }
      return false;
    }

    // Expand vector if needed
    if (index >= random_access_storage_.size()) {
      random_access_storage_.resize(index + 1, 0);
    }

    random_access_storage_[index] = data;
    write_count_++;
    return true;
  }

  /**
   * @brief Load data from a specific address (RANDOM_ACCESS mode)
   *
   * For RANDOM_ACCESS mode: Returns data at the specified index
   * For FIFO mode: Not supported, use read() instead
   *
   * @param address Index/address to read from
   * @return Data at address, or 0 if out of bounds
   */
  uint32_t load(uint32_t address) const {
    if (params_.mode != BufferMode::RANDOM_ACCESS) {
      std::cerr << "[" << name_
                << "] load() only supported in RANDOM_ACCESS mode" << std::endl;
      return 0;
    }

    uint32_t index = address / 4;
    if (index < random_access_storage_.size()) {
      return random_access_storage_[index];
    }

    if (params_.enable_underflow_check) {
      std::cerr << "[" << name_ << "] Address out of bounds: " << address
                << std::endl;
    }
    return 0;  // Return default (0 for instructions = NOP)
  }

  /**
   * @brief Clear all entries from the FIFO buffer
   */
  void clear() { fifo_queue_.clear(); }

  /**
   * @brief Initialize the buffer
   *
   * Initializes the buffer configuration
   */
  void initialize() {
    // Buffer initialization logic
    // (removed port management as Buffer no longer inherits from Component)
  }

  /**
   * @brief Reset the buffer state
   */
  void reset() {
    fifo_queue_.clear();
    random_access_storage_.clear();
    write_count_ = 0;
    read_count_ = 0;
  }

  /**
   * @brief Get buffer statistics
   *
   * @return String containing buffer statistics
   */
  std::string getStatistics() const {
    std::stringstream ss;
    ss << "[" << name_ << "] Buffer Statistics:" << std::endl;
    ss << "  Mode: "
       << (params_.mode == BufferMode::FIFO ? "FIFO" : "RANDOM_ACCESS")
       << std::endl;
    ss << "  Depth: " << params_.depth << std::endl;

    if (params_.mode == BufferMode::FIFO) {
      ss << "  Current Occupancy: " << getCurrentOccupancy() << std::endl;
      ss << "  Available Space: " << getAvailableSpace() << std::endl;
    } else {
      ss << "  Current Size: " << random_access_storage_.size() << std::endl;
    }

    ss << "  Total Writes: " << write_count_ << std::endl;
    ss << "  Total Reads: " << read_count_ << std::endl;
    ss << "  Bypass Enabled: " << (params_.enable_bypass ? "Yes" : "No")
       << std::endl;
    return ss.str();
  }

 private:
  std::string name_;
  EventDriven::EventScheduler& scheduler_;
  BufferParameters params_;
  std::deque<FIFOBufferEntry> fifo_queue_;
  std::vector<uint32_t> random_access_storage_;  // For random access mode
  uint64_t write_count_ = 0;
  uint64_t read_count_ = 0;
};

#endif  // BUFFER_H
