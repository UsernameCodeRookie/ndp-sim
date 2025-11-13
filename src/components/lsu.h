#ifndef LSU_H
#define LSU_H

#include <array>
#include <memory>
#include <queue>
#include <string>
#include <vector>

#include "../packet.h"
#include "../port.h"
#include "../tick.h"
#include "../trace.h"

// Conditional include for DRAMsim3
#ifdef USE_DRAMSIM3
#include "dram.h"
#endif

/**
 * @brief LSU Operation Types
 */
enum class LSUOp {
  LOAD,         // Load from memory
  STORE,        // Store to memory
  LOAD_VECTOR,  // Vector load (with stride)
  STORE_VECTOR  // Vector store (with stride)
};

/**
 * @brief Memory Request Packet
 */
class MemoryRequestPacket : public Architecture::DataPacket {
 public:
  MemoryRequestPacket(LSUOp op, uint32_t address, int32_t data = 0,
                      uint32_t stride = 1, uint32_t length = 1,
                      bool mask = true)
      : op_(op),
        address_(address),
        data_(data),
        stride_(stride),
        length_(length),
        mask_(mask) {}

  LSUOp getOperation() const { return op_; }
  uint32_t getAddress() const { return address_; }
  int32_t getData() const { return data_; }
  uint32_t getStride() const { return stride_; }
  uint32_t getLength() const { return length_; }
  bool getMask() const { return mask_; }

  void setOperation(LSUOp op) { op_ = op; }
  void setAddress(uint32_t addr) { address_ = addr; }
  void setData(int32_t data) { data_ = data; }

  std::shared_ptr<Architecture::DataPacket> clone() const override {
    return std::make_shared<MemoryRequestPacket>(op_, address_, data_, stride_,
                                                 length_, mask_);
  }

 private:
  LSUOp op_;
  uint32_t address_;
  int32_t data_;
  uint32_t stride_;
  uint32_t length_;
  bool mask_;
};

/**
 * @brief Memory Response Packet
 */
class MemoryResponsePacket : public Architecture::DataPacket {
 public:
  MemoryResponsePacket(int32_t data, uint32_t address)
      : data_(data), address_(address) {}

  int32_t getData() const { return data_; }
  uint32_t getAddress() const { return address_; }

  std::shared_ptr<Architecture::DataPacket> clone() const override {
    return std::make_shared<MemoryResponsePacket>(data_, address_);
  }

 private:
  int32_t data_;
  uint32_t address_;
};

/**
 * @brief Memory Bank
 *
 * Simple on-chip memory bank
 */
class MemoryBank : public Architecture::TickingComponent {
 public:
  MemoryBank(const std::string& name, EventDriven::EventScheduler& scheduler,
             uint64_t period, size_t capacity = 64, size_t latency = 3)
      : Architecture::TickingComponent(name, scheduler, period),
        capacity_(capacity),
        latency_(latency),
        current_state_(State::IDLE),
        cycle_count_(0) {
    memory_.resize(capacity_, 0);
  }

  void tick() override {
    switch (current_state_) {
      case State::IDLE:
        handleIdle();
        break;
      case State::PROCESSING:
        handleProcessing();
        break;
      case State::DONE:
        handleDone();
        break;
    }
  }

  bool isReady() const { return current_state_ == State::IDLE; }
  bool isDone() const { return current_state_ == State::DONE; }

  void processRequest(std::shared_ptr<MemoryRequestPacket> request) {
    if (current_state_ != State::IDLE) {
      std::cerr << "Warning: Memory bank busy!" << std::endl;
      return;
    }

    current_request_ = request;
    cycle_count_ = 0;
    current_state_ = State::PROCESSING;
  }

  std::shared_ptr<MemoryResponsePacket> getResponse() {
    return current_response_;
  }

  int32_t read(uint32_t address) {
    if (address < memory_.size()) {
      return memory_[address];
    }
    return 0;
  }

  void write(uint32_t address, int32_t data) {
    if (address < memory_.size()) {
      memory_[address] = data;
    }
  }

  void acknowledgeResponse() {
    current_response_ = nullptr;
    current_state_ = State::IDLE;
    current_request_ = nullptr;
  }

 private:
  enum class State { IDLE, PROCESSING, DONE };

  void handleIdle() { current_response_ = nullptr; }

  void handleProcessing() {
    cycle_count_++;
    if (cycle_count_ >= latency_) {
      if (current_request_->getOperation() == LSUOp::LOAD) {
        int32_t data = read(current_request_->getAddress());
        current_response_ = std::make_shared<MemoryResponsePacket>(
            data, current_request_->getAddress());
      } else {
        write(current_request_->getAddress(), current_request_->getData());
        current_response_ = std::make_shared<MemoryResponsePacket>(
            0, current_request_->getAddress());
      }
      current_state_ = State::DONE;
    }
  }

  void handleDone() {
    // Wait for acknowledgment
  }

  std::vector<int32_t> memory_;
  size_t capacity_;
  size_t latency_;
  State current_state_;
  size_t cycle_count_;
  std::shared_ptr<MemoryRequestPacket> current_request_;
  std::shared_ptr<MemoryResponsePacket> current_response_;
};

/**
 * @brief Load-Store Unit (LSU)
 *
 * Unified LSU supporting both on-chip memory banks and DRAMsim3.
 * Use USE_DRAMSIM3 macro to enable DRAMsim3 integration.
 *
 * Features:
 * - Scalar/vector loads and stores
 * - Memory bank conflict resolution
 * - Request queuing with back pressure
 * - Optional DRAMsim3 off-chip memory simulation
 */
class LoadStoreUnit : public Architecture::TickingComponent {
 public:
  // Timing parameters
  static constexpr uint64_t MEMORY_READ_LATENCY = 1;  // Cycles for memory read
  static constexpr uint64_t MEMORY_WRITE_LATENCY =
      1;  // Cycles for memory write

  LoadStoreUnit(const std::string& name, EventDriven::EventScheduler& scheduler,
                uint64_t period, size_t num_banks = 8, size_t queue_depth = 4,
                size_t bank_capacity = 64
#ifdef USE_DRAMSIM3
                ,
                const std::string& config_file = "",
                const std::string& output_dir = ""
#endif
                )
      : Architecture::TickingComponent(name, scheduler, period),
        num_banks_(num_banks),
        queue_depth_(queue_depth),
        current_state_(State::IDLE),
        element_index_(0),
        vector_length_(0),
        operations_completed_(0),
        cycles_stalled_(0) {

#ifdef USE_DRAMSIM3
    // Use DRAMsim3 for off-chip memory
    if (!config_file.empty()) {
      dram_ = std::make_shared<DRAMsim3Wrapper>(name + "_DRAM", config_file,
                                                output_dir);
      use_dramsim3_ = true;
    } else {
      use_dramsim3_ = false;
      createMemoryBanks(bank_capacity);
    }
#else
    // Use simple memory banks
    createMemoryBanks(bank_capacity);
#endif

    createPorts();
  }

  void tick() override {
    // Tick all memory banks first
    for (auto& bank : memory_banks_) {
      bank->tick();
    }

    handlePortIO();

    switch (current_state_) {
      case State::IDLE:
        handleIdle();
        break;
      case State::PROCESSING:
        handleProcessing();
        break;
      case State::WAITING_BANK:
        handleWaitingBank();
        break;
    }

    sendStatusSignals();
  }

  // Statistics
  size_t getOperationsCompleted() const { return operations_completed_; }
  size_t getCyclesStalled() const { return cycles_stalled_; }

  // Timing information
  uint64_t getReadLatency() const { return MEMORY_READ_LATENCY; }
  uint64_t getWriteLatency() const { return MEMORY_WRITE_LATENCY; }

 private:
  enum class State { IDLE, PROCESSING, WAITING_BANK };

  void createMemoryBanks(size_t bank_capacity) {
    for (size_t i = 0; i < num_banks_; ++i) {
      auto bank =
          std::make_shared<MemoryBank>(getName() + "_Bank" + std::to_string(i),
                                       scheduler_, getPeriod(), bank_capacity);
      memory_banks_.push_back(bank);
      bank->start();
    }
  }

  void createPorts() {
    addPort("req_in", Architecture::PortDirection::INPUT);
    addPort("resp_out", Architecture::PortDirection::OUTPUT);
    addPort("ready", Architecture::PortDirection::OUTPUT);
    addPort("valid", Architecture::PortDirection::INPUT);
    addPort("done", Architecture::PortDirection::OUTPUT);
  }

  void handlePortIO() {
    auto req_in = getPort("req_in");

    // Simply read request if available (backpressure handled by connection)
    if (req_in->hasData()) {
      auto req_data = req_in->read();
      auto mem_req = std::dynamic_pointer_cast<MemoryRequestPacket>(req_data);

      if (mem_req && request_queue_.size() < queue_depth_) {
        request_queue_.push(mem_req);

        TRACE_EVENT(
            scheduler_.getCurrentTime(), getName(), "MEM_REQ",
            (mem_req->getOperation() == LSUOp::LOAD ? "LOAD" : "STORE")
                << " addr=0x" << std::hex << mem_req->getAddress() << std::dec
                << " bank=" << (mem_req->getAddress() % num_banks_)
                << " queue=" << request_queue_.size() << "/" << queue_depth_
                << " cycle=" << scheduler_.getCurrentTime()
                << " period=" << getPeriod()
                << (mem_req->getOperation() == LSUOp::STORE
                        ? " data=" + std::to_string(mem_req->getData())
                        : ""));
      }
    }
  }

  void sendStatusSignals() {
    auto resp_out = getPort("resp_out");
    auto ready_out = getPort("ready");
    auto done_out = getPort("done");

    bool is_ready = (request_queue_.size() < queue_depth_) &&
                    (current_state_ == State::IDLE);
    ready_out->write(
        std::make_shared<Architecture::IntDataPacket>(is_ready ? 1 : 0));

    bool is_done = (current_state_ == State::IDLE) && request_queue_.empty();
    done_out->write(
        std::make_shared<Architecture::IntDataPacket>(is_done ? 1 : 0));

    if (current_response_) {
      resp_out->write(std::static_pointer_cast<Architecture::DataPacket>(
          current_response_));
      current_response_ = nullptr;
    }
  }

  void handleIdle() {
    if (!request_queue_.empty()) {
      current_request_ = request_queue_.front();
      request_queue_.pop();

      element_index_ = 0;
      vector_length_ = current_request_->getLength();

      current_state_ = State::PROCESSING;
    }
  }

  void handleProcessing() {
#ifdef USE_DRAMSIM3
    if (use_dramsim3_) {
      handleProcessingDRAM();
      return;
    }
#endif
    handleProcessingBanks();
  }

  void handleProcessingBanks() {
    if (element_index_ >= vector_length_) {
      current_state_ = State::IDLE;
      operations_completed_++;

      TRACE_EVENT(
          scheduler_.getCurrentTime(), getName(), "MEM_DONE",
          (current_request_->getOperation() == LSUOp::LOAD ? "LOAD_DONE"
                                                           : "STORE_DONE")
              << " addr=0x" << std::hex << current_request_->getAddress()
              << std::dec << " length=" << vector_length_ << " ops="
              << operations_completed_ << " stalls=" << cycles_stalled_
              << " cycle=" << scheduler_.getCurrentTime()
              << " period=" << getPeriod());

      return;
    }

    uint32_t address = current_request_->getAddress() +
                       element_index_ * current_request_->getStride();
    size_t bank_id = address % num_banks_;
    uint32_t bank_addr = address / num_banks_;

    auto& bank = memory_banks_[bank_id];

    if (bank->isReady()) {
      // Create a new request with bank-local address
      auto bank_request = std::make_shared<MemoryRequestPacket>(
          current_request_->getOperation(), bank_addr,
          current_request_->getData(), 1, 1, current_request_->getMask());
      bank->processRequest(bank_request);
      current_state_ = State::WAITING_BANK;

      if (current_request_->getOperation() == LSUOp::LOAD) {
        TRACE_MEM_READ(scheduler_.getCurrentTime(), getName(), address,
                       bank_id);
      } else {
        TRACE_MEM_WRITE(scheduler_.getCurrentTime(), getName(), address,
                        current_request_->getData());
      }
      TRACE_EVENT(
          scheduler_.getCurrentTime(), getName(), "BANK_ACCESS",
          (current_request_->getOperation() == LSUOp::LOAD ? "BANK_READ"
                                                           : "BANK_WRITE")
              << " bank=" << bank_id << " addr=0x" << std::hex << address
              << std::dec << " elem=" << element_index_ << "/" << vector_length_
              << " cycle=" << scheduler_.getCurrentTime()
              << " period=" << getPeriod());

    } else {
      cycles_stalled_++;

      TRACE_EVENT(
          scheduler_.getCurrentTime(), getName(), "STALL",
          "BANK_CONFLICT bank=" << bank_id << " addr=0x" << std::hex << address
                                << std::dec << " elem=" << element_index_
                                << " total_stalls=" << cycles_stalled_
                                << " cycle=" << scheduler_.getCurrentTime());
    }
  }

#ifdef USE_DRAMSIM3
  void handleProcessingDRAM() {
    if (element_index_ >= vector_length_) {
      current_state_ = State::IDLE;
      operations_completed_++;
      return;
    }

    uint32_t address = current_request_->getAddress() +
                       element_index_ * current_request_->getStride();

    if (current_request_->getOperation() == LSUOp::LOAD ||
        current_request_->getOperation() == LSUOp::LOAD_VECTOR) {
      int32_t data = dram_->read(address);
      current_response_ = std::make_shared<MemoryResponsePacket>(data, address);
    } else {
      dram_->write(address, current_request_->getData());
      current_response_ = std::make_shared<MemoryResponsePacket>(0, address);
    }

    element_index_++;
  }
#endif

  void handleWaitingBank() {
    uint32_t address = current_request_->getAddress() +
                       element_index_ * current_request_->getStride();
    size_t bank_id = address % num_banks_;
    auto& bank = memory_banks_[bank_id];

    if (bank->isDone()) {
      auto bank_response = bank->getResponse();
      if (bank_response) {
        // Create response with original global address, not bank address
        current_response_ = std::make_shared<MemoryResponsePacket>(
            bank_response->getData(), address);
      }
      bank->acknowledgeResponse();
      element_index_++;
      current_state_ = State::PROCESSING;
    }
  }

  size_t num_banks_;
  size_t queue_depth_;
  std::vector<std::shared_ptr<MemoryBank>> memory_banks_;
  std::queue<std::shared_ptr<MemoryRequestPacket>> request_queue_;
  std::shared_ptr<MemoryRequestPacket> current_request_;
  std::shared_ptr<MemoryResponsePacket> current_response_;

  State current_state_;
  size_t element_index_;
  size_t vector_length_;
  size_t operations_completed_;
  size_t cycles_stalled_;

#ifdef USE_DRAMSIM3
  std::shared_ptr<DRAMsim3Wrapper> dram_;
  bool use_dramsim3_ = false;
#endif
};

#endif  // LSU_H
