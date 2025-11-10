#ifndef LSU_H
#define LSU_H

#include <array>
#include <iostream>
#include <memory>
#include <queue>
#include <string>
#include <vector>

#include "../port.h"
#include "../tick_component.h"
#include "int_packet.h"

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
 *
 * Contains memory address, data (for stores), and operation type
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
  void setStride(uint32_t stride) { stride_ = stride; }
  void setLength(uint32_t length) { length_ = length; }
  void setMask(bool mask) { mask_ = mask; }

  std::shared_ptr<Architecture::DataPacket> clone() const override {
    auto cloned = std::make_shared<MemoryRequestPacket>(
        op_, address_, data_, stride_, length_, mask_);
    cloned->setTimestamp(timestamp_);
    cloned->setValid(valid_);
    return cloned;
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
 *
 * Contains data read from memory and status
 */
class MemoryResponsePacket : public Architecture::DataPacket {
 public:
  MemoryResponsePacket(int32_t data, bool success = true)
      : data_(data), success_(success) {}

  int32_t getData() const { return data_; }
  bool isSuccess() const { return success_; }

  void setData(int32_t data) { data_ = data; }
  void setSuccess(bool success) { success_ = success; }

  std::shared_ptr<Architecture::DataPacket> clone() const override {
    auto cloned = std::make_shared<MemoryResponsePacket>(data_, success_);
    cloned->setTimestamp(timestamp_);
    cloned->setValid(valid_);
    return cloned;
  }

 private:
  int32_t data_;
  bool success_;
};

/**
 * @brief Memory Bank Component
 *
 * Single memory bank with configurable latency
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

  // Process a memory request
  void processRequest(std::shared_ptr<MemoryRequestPacket> request) {
    if (current_state_ != State::IDLE) {
      std::cerr << "Warning: Memory bank busy!" << std::endl;
      return;
    }

    current_request_ = request;
    cycle_count_ = 0;
    current_state_ = State::PROCESSING;
  }

  // Get the response
  std::shared_ptr<MemoryResponsePacket> getResponse() {
    return current_response_;
  }

  // Read from memory
  int32_t read(uint32_t address) {
    if (address < memory_.size()) {
      return memory_[address];
    }
    return 0;
  }

  // Write to memory
  void write(uint32_t address, int32_t data) {
    if (address < memory_.size()) {
      memory_[address] = data;
    }
  }

  // Clear response and return to idle (called by LSU after reading response)
  void acknowledgeResponse() {
    current_response_ = nullptr;
    current_state_ = State::IDLE;
    current_request_ = nullptr;
  }

 private:
  enum class State { IDLE, PROCESSING, DONE };

  void handleIdle() {
    // Wait for request
    current_response_ = nullptr;
  }

  void handleProcessing() {
    cycle_count_++;

    if (cycle_count_ >= latency_) {
      // Complete the operation
      if (current_request_) {
        uint32_t addr = current_request_->getAddress();

        if (current_request_->getOperation() == LSUOp::LOAD ||
            current_request_->getOperation() == LSUOp::LOAD_VECTOR) {
          // Load operation
          int32_t data = read(addr);
          current_response_ =
              std::make_shared<MemoryResponsePacket>(data, true);
        } else {
          // Store operation
          write(addr, current_request_->getData());
          current_response_ = std::make_shared<MemoryResponsePacket>(0, true);
        }
      }

      current_state_ = State::DONE;
    }
  }

  void handleDone() {
    // Stay in DONE state for one cycle so response can be read
    // Then return to idle
    if (current_response_) {
      // Response available, will be picked up by LSU
      // Stay in DONE state
    } else {
      current_state_ = State::IDLE;
      current_request_ = nullptr;
    }
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
 * Handles memory access operations including:
 * - Scalar loads/stores
 * - Vector loads/stores with stride support
 * - Memory bank conflict resolution
 * - Request queuing with back pressure
 *
 * Based on the vector processor MAU and MCN design
 */
class LoadStoreUnit : public Architecture::TickingComponent {
 public:
  LoadStoreUnit(const std::string& name, EventDriven::EventScheduler& scheduler,
                uint64_t period, size_t num_banks = 8, size_t queue_depth = 4,
                size_t bank_capacity = 64)
      : Architecture::TickingComponent(name, scheduler, period),
        num_banks_(num_banks),
        queue_depth_(queue_depth),
        current_state_(State::IDLE),
        element_index_(0),
        vector_length_(0),
        operations_completed_(0),
        cycles_stalled_(0) {
    // Create memory banks
    for (size_t i = 0; i < num_banks_; ++i) {
      auto bank = std::make_shared<MemoryBank>(
          name + "_Bank" + std::to_string(i), scheduler, period, bank_capacity);
      memory_banks_.push_back(bank);
      bank->start();  // Start the bank's tick cycle
    }

    // Create ports
    // Request input port
    auto req_in = std::make_shared<Architecture::Port>(
        "req_in", Architecture::PortDirection::INPUT, this);
    addPort(req_in);

    // Response output port
    auto resp_out = std::make_shared<Architecture::Port>(
        "resp_out", Architecture::PortDirection::OUTPUT, this);
    addPort(resp_out);

    // Ready signal output
    auto ready_out = std::make_shared<Architecture::Port>(
        "ready", Architecture::PortDirection::OUTPUT, this);
    addPort(ready_out);

    // Valid signal input
    auto valid_in = std::make_shared<Architecture::Port>(
        "valid", Architecture::PortDirection::INPUT, this);
    addPort(valid_in);

    // Done signal output
    auto done_out = std::make_shared<Architecture::Port>(
        "done", Architecture::PortDirection::OUTPUT, this);
    addPort(done_out);
  }

  void tick() override {
    auto req_in = getPort("req_in");
    auto resp_out = getPort("resp_out");
    auto ready_out = getPort("ready");
    auto valid_in = getPort("valid");
    auto done_out = getPort("done");

    // Check if we can accept new requests
    bool is_ready = (request_queue_.size() < queue_depth_) &&
                    (current_state_ == State::IDLE);

    // Send ready signal
    auto ready_packet = std::make_shared<IntDataPacket>(is_ready ? 1 : 0);
    ready_out->write(
        std::static_pointer_cast<Architecture::DataPacket>(ready_packet));

    // Try to enqueue new request if ready and valid
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

    // Process current state
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

    // Send done signal
    bool is_done = (current_state_ == State::IDLE) && request_queue_.empty();
    auto done_packet = std::make_shared<IntDataPacket>(is_done ? 1 : 0);
    done_out->write(
        std::static_pointer_cast<Architecture::DataPacket>(done_packet));

    // Output response if available
    if (current_response_) {
      resp_out->write(std::static_pointer_cast<Architecture::DataPacket>(
          current_response_));
      current_response_ = nullptr;
    }
  }

  // Statistics
  size_t getOperationsCompleted() const { return operations_completed_; }
  size_t getCyclesStalled() const { return cycles_stalled_; }

  void setVerbose(bool verbose) { verbose_ = verbose; }

  // Direct memory access (bypassing ports, for testing/initialization)
  void directWrite(uint32_t address, int32_t data) {
    size_t bank_id = address % num_banks_;
    uint32_t bank_addr = address / num_banks_;
    memory_banks_[bank_id]->write(bank_addr, data);
  }

  int32_t directRead(uint32_t address) {
    size_t bank_id = address % num_banks_;
    uint32_t bank_addr = address / num_banks_;
    return memory_banks_[bank_id]->read(bank_addr);
  }

 private:
  enum class State { IDLE, PROCESSING, WAITING_BANK };

  void handleIdle() {
    if (!request_queue_.empty()) {
      current_request_ = request_queue_.front();
      request_queue_.pop();

      element_index_ = 0;
      vector_length_ = current_request_->getLength();
      current_state_ = State::PROCESSING;

      if (verbose_) {
        std::cout << "[" << scheduler_.getCurrentTime() << "] " << getName()
                  << ": Started processing request" << std::endl;
      }
    }
  }

  void handleProcessing() {
    if (!current_request_ || element_index_ >= vector_length_) {
      // Finished processing current request
      current_state_ = State::IDLE;
      current_request_ = nullptr;
      return;
    }

    // Calculate current address with stride
    uint32_t current_addr = current_request_->getAddress() +
                            element_index_ * current_request_->getStride();

    // Check mask
    if (!current_request_->getMask()) {
      element_index_++;
      return;
    }

    // Determine target bank (using lower bits for bank selection)
    size_t bank_id = current_addr % num_banks_;
    auto& bank = memory_banks_[bank_id];

    // Check if bank is ready
    if (bank->isReady()) {
      // Create bank request
      auto bank_req = std::make_shared<MemoryRequestPacket>(
          current_request_->getOperation(), current_addr,
          current_request_->getData(), 1, 1, true);

      bank->processRequest(bank_req);
      current_bank_ = bank;
      current_state_ = State::WAITING_BANK;

      if (verbose_) {
        std::cout << "[" << scheduler_.getCurrentTime() << "] " << getName()
                  << ": Sent request to bank " << bank_id
                  << ", addr=" << current_addr << std::endl;
      }
    } else {
      // Bank is busy, stall
      cycles_stalled_++;
    }
  }

  void handleWaitingBank() {
    if (current_bank_ && current_bank_->isDone()) {
      // Get response from bank
      auto response = current_bank_->getResponse();

      if (response) {
        current_response_ = response;
        operations_completed_++;

        if (verbose_) {
          std::cout << "[" << scheduler_.getCurrentTime() << "] " << getName()
                    << ": Received response, data=" << response->getData()
                    << std::endl;
        }
      }

      // Acknowledge the response to the bank
      current_bank_->acknowledgeResponse();

      // Move to next element
      element_index_++;
      current_state_ = State::PROCESSING;
      current_bank_ = nullptr;
    }
  }

  size_t num_banks_;
  size_t queue_depth_;
  std::vector<std::shared_ptr<MemoryBank>> memory_banks_;
  std::queue<std::shared_ptr<MemoryRequestPacket>> request_queue_;

  State current_state_;
  std::shared_ptr<MemoryRequestPacket> current_request_;
  std::shared_ptr<MemoryResponsePacket> current_response_;
  std::shared_ptr<MemoryBank> current_bank_;

  size_t element_index_;
  size_t vector_length_;
  size_t operations_completed_;
  size_t cycles_stalled_;
  bool verbose_ = false;
};

/**
 * @brief Helper function to get LSU operation name
 */
inline std::string getLSUOpName(LSUOp op) {
  switch (op) {
    case LSUOp::LOAD:
      return "LOAD";
    case LSUOp::STORE:
      return "STORE";
    case LSUOp::LOAD_VECTOR:
      return "LOAD_VECTOR";
    case LSUOp::STORE_VECTOR:
      return "STORE_VECTOR";
    default:
      return "UNKNOWN";
  }
}

#endif  // LSU_H
