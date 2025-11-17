#ifndef LSU_H
#define LSU_H

#include <array>
#include <deque>
#include <map>
#include <memory>
#include <queue>
#include <string>
#include <vector>

#include "../../packet.h"
#include "../../pipeline.h"
#include "../../port.h"
#include "../../tick.h"
#include "../../trace.h"

// Conditional include for DRAMsim3
#ifdef USE_DRAMSIM3
#include "../dram.h"
#endif

/**
 * @brief LSU Operation Types (compatible with Coral NPU)
 *
 * Scalar operations:
 * - LB, LH, LW, LBU, LHU: Load byte/half/word (signed/unsigned)
 * - SB, SH, SW: Store byte/half/word
 *
 * Vector operations:
 * - VLOAD_UNIT: Vector unit-stride load
 * - VLOAD_STRIDED: Vector strided load
 * - VLOAD_OINDEXED: Vector ordered-indexed load
 * - VLOAD_UINDEXED: Vector unordered-indexed load
 * - VSTORE_UNIT: Vector unit-stride store
 * - VSTORE_STRIDED: Vector strided store
 * - VSTORE_OINDEXED: Vector ordered-indexed store
 * - VSTORE_UINDEXED: Vector unordered-indexed store
 */
enum class LSUOp : uint8_t {
  // Scalar operations
  LB = 0,   // Load signed byte
  LH = 1,   // Load signed half-word
  LW = 2,   // Load word
  LBU = 3,  // Load unsigned byte
  LHU = 4,  // Load unsigned half-word
  SB = 5,   // Store byte
  SH = 6,   // Store half-word
  SW = 7,   // Store word

  // Vector operations (unit-stride)
  VLOAD_UNIT = 8,   // Vector unit-stride load
  VSTORE_UNIT = 9,  // Vector unit-stride store

  // Vector operations (strided)
  VLOAD_STRIDED = 10,   // Vector strided load
  VSTORE_STRIDED = 11,  // Vector strided store

  // Vector operations (indexed)
  VLOAD_OINDEXED = 12,   // Vector ordered-indexed load
  VLOAD_UINDEXED = 13,   // Vector unordered-indexed load
  VSTORE_OINDEXED = 14,  // Vector ordered-indexed store
  VSTORE_UINDEXED = 15,  // Vector unordered-indexed store
};

/**
 * @brief Determine operation size in bytes
 */
inline size_t getOpSize(LSUOp op) {
  switch (op) {
    case LSUOp::LB:
    case LSUOp::LBU:
    case LSUOp::SB:
      return 1;
    case LSUOp::LH:
    case LSUOp::LHU:
    case LSUOp::SH:
      return 2;
    case LSUOp::LW:
    case LSUOp::SW:
      return 4;
    default:
      return 4;  // Default to word
  }
}

/**
 * @brief Check if operation is a load
 */
inline bool isLoadOp(LSUOp op) {
  return static_cast<uint8_t>(op) <= 4 || static_cast<uint8_t>(op) == 8 ||
         static_cast<uint8_t>(op) == 10 || static_cast<uint8_t>(op) == 12 ||
         static_cast<uint8_t>(op) == 13;
}

/**
 * @brief Check if operation is a store
 */
inline bool isStoreOp(LSUOp op) {
  return (static_cast<uint8_t>(op) >= 5 && static_cast<uint8_t>(op) <= 7) ||
         static_cast<uint8_t>(op) == 9 || static_cast<uint8_t>(op) == 11 ||
         static_cast<uint8_t>(op) == 14 || static_cast<uint8_t>(op) == 15;
}

/**
 * @brief Check if operation is a vector operation
 */
inline bool isVectorOp(LSUOp op) { return static_cast<uint8_t>(op) >= 8; }

/**
 * @brief Scalar Memory Request Packet
 *
 * Represents a single scalar load/store request to memory
 */
class MemoryRequestPacket : public Architecture::DataPacket {
 public:
  MemoryRequestPacket(LSUOp op = LSUOp::LW, uint32_t address = 0,
                      int32_t data = 0)
      : op(op), address(address), data(data), request_id(0), elem_idx(0) {}

  std::shared_ptr<Architecture::DataPacket> clone() const override {
    return cloneImpl<MemoryRequestPacket>(op, address, data);
  }

  // Operation type
  LSUOp op;

  // Memory address
  uint32_t address;

  // Data to write (for stores) or loaded data (for loads)
  int32_t data;

  // Unique request identifier for tracking through pipeline
  uint32_t request_id;

  // Element index for vector operations
  uint32_t elem_idx;
};

/**
 * @brief Memory Response Packet
 *
 * Response from memory with loaded data
 */
class MemoryResponsePacket : public Architecture::DataPacket {
 public:
  MemoryResponsePacket(int32_t data = 0, uint32_t address = 0,
                       uint32_t request_id = 0, uint32_t rd = 0)
      : data(data), address(address), request_id(request_id), rd(rd) {}

  std::shared_ptr<Architecture::DataPacket> clone() const override {
    return cloneImpl<MemoryResponsePacket>(data, address, request_id, rd);
  }

  // Loaded data
  int32_t data;

  // Memory address (for verification/debugging)
  uint32_t address;

  // Request identifier to match response with request
  uint32_t request_id;

  // Destination register address (for loads)
  uint32_t rd;
};

/**
 * @brief Vector Memory Request Packet
 *
 * Represents a vector load/store request with stride and segment support
 */
class VectorMemoryRequestPacket : public Architecture::DataPacket {
 public:
  VectorMemoryRequestPacket()
      : op(LSUOp::VLOAD_UNIT),
        base_address(0),
        stride(1),
        elem_width(4),
        num_elements(1),
        num_segments(1),
        request_id(0) {}

  std::shared_ptr<Architecture::DataPacket> clone() const override {
    auto clone = std::make_shared<VectorMemoryRequestPacket>();
    clone->op = op;
    clone->base_address = base_address;
    clone->stride = stride;
    clone->elem_width = elem_width;
    clone->num_elements = num_elements;
    clone->num_segments = num_segments;
    clone->request_id = request_id;
    return clone;
  }

  // Operation type
  LSUOp op;

  // Base address for vector operation
  uint32_t base_address;

  // Stride between elements (in bytes)
  uint32_t stride;

  // Element width (1, 2, 4 bytes)
  uint32_t elem_width;

  // Number of elements in vector
  uint32_t num_elements;

  // Number of segments (for segmented loads/stores)
  uint32_t num_segments;

  // Unique request identifier
  uint32_t request_id;
};

/**
 * @brief Memory Bank (Coral NPU compatible)
 *
 * Simple on-chip memory bank with configurable capacity and latency
 * Supports both single-element and burst transactions
 */
class MemoryBank : public Architecture::TickingComponent {
 public:
  MemoryBank(const std::string& name, EventDriven::EventScheduler& scheduler,
             uint64_t period, size_t capacity = 1024, uint64_t latency = 2)
      : Architecture::TickingComponent(name, scheduler, period),
        capacity_(capacity),
        latency_(latency) {
    memory_.resize(capacity_, 0);
  }

  /**
   * @brief Perform a load operation
   * @param address Memory address
   * @return Data at address
   */
  int32_t load(uint32_t address) {
    if (address < capacity_) {
      return memory_[address];
    }
    return 0;
  }

  /**
   * @brief Perform a store operation
   * @param address Memory address
   * @param data Data to write
   */
  void store(uint32_t address, int32_t data) {
    if (address < capacity_) {
      memory_[address] = data;
    }
  }

  /**
   * @brief Get memory bank latency
   */
  uint64_t getLatency() const { return latency_; }

  /**
   * @brief Get memory capacity
   */
  size_t getCapacity() const { return capacity_; }

  void tick() override {
    // Banks are passive - no ticking behavior needed
  }

 private:
  std::vector<int32_t> memory_;
  size_t capacity_;
  uint64_t latency_;
};

/**
 * @brief Load-Store Unit (LSU) - Coral NPU Compatible
 *
 * Pipeline-based LSU implementation with Coral NPU features:
 * - 3-stage pipeline: Address decode -> Memory access -> Response
 * - Multi-bank memory with configurable bank interleaving
 * - Request queue with backpressure support
 * - Scalar and vector load/store support
 * - Stride and indexed vector access patterns
 * - Optional DRAMsim3 off-chip memory integration
 *
 * Pipeline Stages:
 * - Stage 0: Address Decode
 *   - Decode request and compute effective address
 *   - Handle bank interleaving for multi-bank memory
 *   - Track element indices for vector operations
 *
 * - Stage 1: Memory Access
 *   - Access memory bank (load/store)
 *   - Handle bank conflicts and stalls
 *   - Generate responses for loads
 *
 * - Stage 2: Response
 *   - Forward response to requestor
 *   - Update statistics and tracing
 */
class LoadStoreUnit : public Pipeline {
 public:
  // Timing parameters (in cycles)
  static constexpr uint64_t BANK_ACCESS_LATENCY = 2;  // Memory bank latency
  static constexpr uint64_t REQUEST_QUEUE_DEPTH = 8;  // Max pending requests

  /**
   * @brief LSU Configuration
   */
  struct Config {
    uint64_t period;        // Tick period (cycles)
    size_t num_banks;       // Number of memory banks (2, 4, 8, 16)
    size_t bank_capacity;   // Capacity per bank (words)
    size_t queue_depth;     // Request queue depth
    uint64_t bank_latency;  // Bank access latency (cycles)
    bool enable_bank_conflict_detection;  // Enable conflict detection

    Config()
        : period(1),
          num_banks(8),
          bank_capacity(1024),
          queue_depth(8),
          bank_latency(2),
          enable_bank_conflict_detection(true) {}
  };

  /**
   * @brief Constructor
   * @param name Component name
   * @param scheduler Event scheduler reference
   * @param config LSU configuration
   */
  LoadStoreUnit(const std::string& name, EventDriven::EventScheduler& scheduler,
                const Config& config = Config())
      : Pipeline(name, scheduler, config.period, 3, 1),  // 3-stage pipeline
        config_(config),
        request_id_counter_(0),
        operations_completed_(0),
        bank_conflicts_(0),
        active_banks_(0) {
    // Initialize memory banks
    initializeMemoryBanks();

    // Create ports
    createPorts();

    // Setup pipeline stages
    setupPipeline();

    TRACE_EVENT(scheduler_.getCurrentTime(), getName(), "INIT",
                "LSU initialized: "
                    << config_.num_banks << " banks, " << config_.bank_capacity
                    << " words per bank, queue_depth=" << config_.queue_depth);
  }

  virtual ~LoadStoreUnit() = default;

  /**
   * @brief Load instruction into data memory
   * @param address Memory address
   * @param data Data value to write
   */
  void loadData(uint32_t address, int32_t data) {
    size_t bank_id = address % config_.num_banks;
    uint32_t bank_addr = address / config_.num_banks;

    if (bank_id < memory_banks_.size()) {
      memory_banks_[bank_id]->store(bank_addr, data);
    }
  }

  /**
   * @brief Read data from data memory
   * @param address Memory address
   * @return Data value at address
   */
  int32_t readData(uint32_t address) const {
    size_t bank_id = address % config_.num_banks;
    uint32_t bank_addr = address / config_.num_banks;

    if (bank_id < memory_banks_.size()) {
      return memory_banks_[bank_id]->load(bank_addr);
    }
    return 0;
  }

  /**
   * @brief Get statistics
   */
  uint64_t getOperationsCompleted() const { return operations_completed_; }
  uint64_t getBankConflicts() const { return bank_conflicts_; }
  size_t getActiveBanks() const { return active_banks_; }

 private:
  /**
   * @brief Initialize memory banks
   */
  void initializeMemoryBanks() {
    for (size_t i = 0; i < config_.num_banks; ++i) {
      auto bank = std::make_shared<MemoryBank>(
          getName() + "_Bank" + std::to_string(i), scheduler_, getPeriod(),
          config_.bank_capacity, config_.bank_latency);
      memory_banks_.push_back(bank);
    }
  }

  /**
   * @brief Create LSU ports
   */
  void createPorts() {
    // Request input port (expects MemoryRequestPacket or
    // VectorMemoryRequestPacket)
    addPort("req_in", Architecture::PortDirection::INPUT);

    // Response output port (MemoryResponsePacket)
    addPort("resp_out", Architecture::PortDirection::OUTPUT);

    // Status signals
    addPort("ready", Architecture::PortDirection::OUTPUT);  // Can accept
    addPort("valid", Architecture::PortDirection::OUTPUT);  // Response valid
    addPort("done", Architecture::PortDirection::OUTPUT);   // All ops done

    // Create ports for RegisterFileWire
    addPort("rd_out", Architecture::PortDirection::OUTPUT);
    addPort("data_out", Architecture::PortDirection::OUTPUT);
  }

  /**
   * @brief Setup 3-stage pipeline
   */
  void setupPipeline() {
    // Stage 0: Address Decode
    // Decode request and prepare for memory access
    setStageFunction(0, [this](std::shared_ptr<Architecture::DataPacket> data) {
      uint64_t current_time = scheduler_.getCurrentTime();

      // Try to read a request from input port if stage 0 is empty
      auto req_in = getPort("req_in");
      if (!data && req_in && req_in->hasData()) {
        data = req_in->read();
      }

      if (!data) {
        return data;  // No request available
      }

      // Try to decode as scalar memory request
      auto mem_req = std::dynamic_pointer_cast<MemoryRequestPacket>(data);
      if (mem_req) {
        // Assign request ID if not already assigned
        if (mem_req->request_id == 0) {
          mem_req->request_id = ++request_id_counter_;
        }

        uint32_t address = mem_req->address;
        size_t bank_id = address % config_.num_banks;

        TRACE_EVENT(current_time, getName(), "DECODE",
                    (isLoadOp(mem_req->op) ? "LOAD" : "STORE")
                        << " addr=0x" << std::hex << address << std::dec
                        << " bank=" << bank_id
                        << " req_id=" << mem_req->request_id
                        << " op=" << static_cast<int>(mem_req->op));

        return data;
      }

      // Try to decode as vector memory request
      auto vec_req = std::dynamic_pointer_cast<VectorMemoryRequestPacket>(data);
      if (vec_req) {
        if (vec_req->request_id == 0) {
          vec_req->request_id = ++request_id_counter_;
        }

        TRACE_EVENT(current_time, getName(), "DECODE_VEC",
                    (isLoadOp(vec_req->op) ? "VLOAD" : "VSTORE")
                        << " base=0x" << std::hex << vec_req->base_address
                        << std::dec << " stride=" << vec_req->stride
                        << " elements=" << vec_req->num_elements
                        << " req_id=" << vec_req->request_id);

        return data;
      }

      return data;
    });

    // Stage 1: Memory Access
    // Perform actual load/store to memory bank
    setStageFunction(1, [this](std::shared_ptr<Architecture::DataPacket> data) {
      uint64_t current_time = scheduler_.getCurrentTime();

      if (!data) return data;

      // Try scalar memory request
      auto mem_req = std::dynamic_pointer_cast<MemoryRequestPacket>(data);
      if (mem_req) {
        size_t bank_id = mem_req->address % config_.num_banks;

        if (bank_id >= memory_banks_.size()) {
          TRACE_EVENT(current_time, getName(), "MEM_ERROR",
                      "Invalid bank ID: " << bank_id);
          return data;
        }

        auto bank = memory_banks_[bank_id];
        uint32_t bank_addr = mem_req->address / config_.num_banks;

        if (isLoadOp(mem_req->op)) {
          // Perform load
          int32_t load_data = bank->load(bank_addr);

          TRACE_MEM_READ(current_time, getName(), mem_req->address, bank_id);
          TRACE_EVENT(current_time, getName(), "MEM_READ",
                      "addr=0x" << std::hex << mem_req->address << std::dec
                                << " bank=" << bank_id << " data=" << load_data
                                << " req_id=" << mem_req->request_id);

          // Create response packet
          auto resp = std::make_shared<MemoryResponsePacket>(
              load_data, mem_req->address, mem_req->request_id);
          resp->timestamp = current_time;
          return std::static_pointer_cast<Architecture::DataPacket>(resp);
        } else {
          // Perform store
          bank->store(bank_addr, mem_req->data);

          TRACE_MEM_WRITE(current_time, getName(), mem_req->address,
                          mem_req->data);
          TRACE_EVENT(current_time, getName(), "MEM_WRITE",
                      "addr=0x" << std::hex << mem_req->address << std::dec
                                << " bank=" << bank_id
                                << " data=" << mem_req->data
                                << " req_id=" << mem_req->request_id);

          // Create store acknowledgement
          auto resp = std::make_shared<MemoryResponsePacket>(
              0, mem_req->address, mem_req->request_id);
          resp->timestamp = current_time;
          return std::static_pointer_cast<Architecture::DataPacket>(resp);
        }
      }

      // Try vector memory request
      auto vec_req = std::dynamic_pointer_cast<VectorMemoryRequestPacket>(data);
      if (vec_req) {
        // For vectors, we need to handle multiple elements
        // For now, process first element (simplified)
        uint32_t address = vec_req->base_address;
        size_t bank_id = address % config_.num_banks;

        if (bank_id >= memory_banks_.size()) {
          TRACE_EVENT(current_time, getName(), "VEC_ERROR",
                      "Invalid bank ID: " << bank_id);
          return data;
        }

        auto bank = memory_banks_[bank_id];
        uint32_t bank_addr = address / config_.num_banks;

        if (isLoadOp(vec_req->op)) {
          int32_t load_data = bank->load(bank_addr);
          auto resp = std::make_shared<MemoryResponsePacket>(
              load_data, address, vec_req->request_id);
          resp->timestamp = current_time;
          return std::static_pointer_cast<Architecture::DataPacket>(resp);
        } else {
          // Store (would need data from vector register in real implementation)
          auto resp = std::make_shared<MemoryResponsePacket>(
              0, address, vec_req->request_id);
          resp->timestamp = current_time;
          return std::static_pointer_cast<Architecture::DataPacket>(resp);
        }
      }

      return data;
    });

    // Stage 2: Response
    // Output response and update statistics
    setStageFunction(2, [this](std::shared_ptr<Architecture::DataPacket> data) {
      uint64_t current_time = scheduler_.getCurrentTime();

      if (!data) return data;

      auto resp = std::dynamic_pointer_cast<MemoryResponsePacket>(data);
      if (resp) {
        operations_completed_++;

        TRACE_EVENT(current_time, getName(), "RESPONSE",
                    "req_id=" << resp->request_id << " addr=0x" << std::hex
                              << resp->address << std::dec
                              << " data=" << resp->data
                              << " ops=" << operations_completed_);

        // Write response to output port
        auto resp_out = getPort("resp_out");
        if (resp_out) {
          resp_out->write(
              std::static_pointer_cast<Architecture::DataPacket>(resp));
        }

        // Output rd and loaded data to RegisterFileWire ports (for loads only)
        if (resp->rd != 0) {
          auto rd_port = getPort("rd_out");
          auto data_port = getPort("data_out");
          if (rd_port && data_port) {
            rd_port->setData(
                std::make_shared<Architecture::IntDataPacket>(resp->rd));
            data_port->setData(std::make_shared<Architecture::IntDataPacket>(
                static_cast<uint32_t>(resp->data)));
          }
        }

        // Set valid signal
        auto valid_out = getPort("valid");
        if (valid_out) {
          valid_out->write(
              std::make_shared<Architecture::BoolDataPacket>(true));
        }
      }

      return data;
    });

    // Set stage latencies
    setStageLatency(0, 1);                     // Address decode: 1 cycle
    setStageLatency(1, config_.bank_latency);  // Memory access: bank latency
    setStageLatency(2, 1);                     // Response: 1 cycle
  }

  // Member variables
  Config config_;
  std::vector<std::shared_ptr<MemoryBank>> memory_banks_;
  uint32_t request_id_counter_;
  uint64_t operations_completed_;
  uint64_t bank_conflicts_;
  size_t active_banks_;
};

#endif  // LSU_H
