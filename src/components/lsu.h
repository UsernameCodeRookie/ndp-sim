#ifndef LSU_H
#define LSU_H

#include <array>
#include <iostream>
#include <memory>
#include <queue>
#include <string>
#include <vector>

#include "../port.h"
#include "../tick.h"
#include "int_packet.h"

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
      if (verbose_) {
        std::cout << "[LSU] Using DRAMsim3 for memory simulation\n";
      }
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

  // Direct memory access (bypassing ports)
  void directWrite(uint32_t address, int32_t data) {
#ifdef USE_DRAMSIM3
    if (use_dramsim3_) {
      dram_->write(address, data);
      return;
    }
#endif
    size_t bank_id = address % num_banks_;
    uint32_t bank_addr = address / num_banks_;
    memory_banks_[bank_id]->write(bank_addr, data);
  }

  int32_t directRead(uint32_t address) {
#ifdef USE_DRAMSIM3
    if (use_dramsim3_) {
      return dram_->read(address);
    }
#endif
    size_t bank_id = address % num_banks_;
    uint32_t bank_addr = address / num_banks_;
    return memory_banks_[bank_id]->read(bank_addr);
  }

  // Statistics
  size_t getOperationsCompleted() const { return operations_completed_; }
  size_t getCyclesStalled() const { return cycles_stalled_; }

  void setVerbose(bool verbose) { verbose_ = verbose; }

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
    addPort(std::make_shared<Architecture::Port>(
        "req_in", Architecture::PortDirection::INPUT, this));
    addPort(std::make_shared<Architecture::Port>(
        "resp_out", Architecture::PortDirection::OUTPUT, this));
    addPort(std::make_shared<Architecture::Port>(
        "ready", Architecture::PortDirection::OUTPUT, this));
    addPort(std::make_shared<Architecture::Port>(
        "valid", Architecture::PortDirection::INPUT, this));
    addPort(std::make_shared<Architecture::Port>(
        "done", Architecture::PortDirection::OUTPUT, this));
  }

  void handlePortIO() {
    auto req_in = getPort("req_in");
    auto valid_in = getPort("valid");

    bool is_ready = (request_queue_.size() < queue_depth_) &&
                    (current_state_ == State::IDLE);

    if (is_ready) {
      auto valid_data = valid_in->read();
      auto valid_int = std::dynamic_pointer_cast<IntDataPacket>(valid_data);

      if (valid_int && valid_int->getValue() == 1) {
        auto req_data = req_in->read();
        auto mem_req = std::dynamic_pointer_cast<MemoryRequestPacket>(req_data);

        if (mem_req) {
          request_queue_.push(mem_req);
          if (verbose_) {
            std::cout << "[" << scheduler_.getCurrentTime() << "] " << getName()
                      << ": Enqueued request, addr=" << mem_req->getAddress()
                      << std::endl;
          }
        }
      }
    }
  }

  void sendStatusSignals() {
    auto resp_out = getPort("resp_out");
    auto ready_out = getPort("ready");
    auto done_out = getPort("done");

    bool is_ready = (request_queue_.size() < queue_depth_) &&
                    (current_state_ == State::IDLE);
    ready_out->write(std::make_shared<IntDataPacket>(is_ready ? 1 : 0));

    bool is_done = (current_state_ == State::IDLE) && request_queue_.empty();
    done_out->write(std::make_shared<IntDataPacket>(is_done ? 1 : 0));

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
      return;
    }

    uint32_t address = current_request_->getAddress() +
                       element_index_ * current_request_->getStride();
    size_t bank_id = address % num_banks_;
    uint32_t bank_addr = address / num_banks_;

    auto& bank = memory_banks_[bank_id];

    if (bank->isReady()) {
      bank->processRequest(current_request_);
      current_state_ = State::WAITING_BANK;
    } else {
      cycles_stalled_++;
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
      current_response_ = bank->getResponse();
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
  bool verbose_ = false;

#ifdef USE_DRAMSIM3
  std::shared_ptr<DRAMsim3Wrapper> dram_;
  bool use_dramsim3_ = false;
#endif
};

#endif  // LSU_H
