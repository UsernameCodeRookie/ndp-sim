#ifndef LSU_DRAMSIM3_H
#define LSU_DRAMSIM3_H

#include <array>
#include <iostream>
#include <memory>
#include <queue>
#include <string>
#include <unordered_map>
#include <vector>

#include "../port.h"
#include "../tick_component.h"
#include "dramsim3_wrapper.h"
#include "int_packet.h"
#include "lsu.h"

/**
 * @brief Load-Store Unit with DRAMsim3 Integration
 *
 * Uses DRAMsim3 for realistic DRAM timing simulation.
 */
class LoadStoreUnitDRAM : public Architecture::TickingComponent {
 public:
  LoadStoreUnitDRAM(const std::string& name,
                    EventDriven::EventScheduler& scheduler, uint64_t period,
                    const std::string& dram_config,
                    const std::string& dram_output_dir, size_t queue_depth = 16)
      : Architecture::TickingComponent(name, scheduler, period),
        queue_depth_(queue_depth),
        current_state_(State::IDLE),
        element_index_(0),
        vector_length_(0),
        operations_completed_(0),
        cycles_stalled_(0),
        next_request_id_(1) {
    dram_ = std::make_shared<DRAMsim3Wrapper>(dram_config, dram_output_dir);
    data_cache_.resize(1024 * 1024, 0);

    // Create ports
    auto req_in = std::make_shared<Architecture::Port>(
        "req_in", Architecture::PortDirection::INPUT, this);
    addPort(req_in);

    auto resp_out = std::make_shared<Architecture::Port>(
        "resp_out", Architecture::PortDirection::OUTPUT, this);
    addPort(resp_out);

    auto ready_out = std::make_shared<Architecture::Port>(
        "ready", Architecture::PortDirection::OUTPUT, this);
    addPort(ready_out);

    auto valid_in = std::make_shared<Architecture::Port>(
        "valid", Architecture::PortDirection::INPUT, this);
    addPort(valid_in);

    auto done_out = std::make_shared<Architecture::Port>(
        "done", Architecture::PortDirection::OUTPUT, this);
    addPort(done_out);
  }

  void tick() override {
    dram_->tick();
    processCompletedDRAMTransactions();

    auto req_in = getPort("req_in");
    auto resp_out = getPort("resp_out");
    auto ready_out = getPort("ready");
    auto valid_in = getPort("valid");
    auto done_out = getPort("done");

    bool is_ready = (request_queue_.size() < queue_depth_) &&
                    (current_state_ == State::IDLE);
    auto ready_packet = std::make_shared<IntDataPacket>(is_ready ? 1 : 0);

    ready_out->write(
        std::static_pointer_cast<Architecture::DataPacket>(ready_packet));

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
                      << ": Queued request addr=" << mem_req->getAddress()
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
      case State::WAITING_DRAM:
        handleWaitingDRAM();
        break;
    }

    bool is_done = (current_state_ == State::IDLE) && request_queue_.empty();
    auto done_packet = std::make_shared<IntDataPacket>(is_done ? 1 : 0);
    done_out->write(
        std::static_pointer_cast<Architecture::DataPacket>(done_packet));

    if (current_response_) {
      resp_out->write(std::static_pointer_cast<Architecture::DataPacket>(
          current_response_));
      current_response_ = nullptr;
    }
  }

  size_t getOperationsCompleted() const { return operations_completed_; }
  size_t getCyclesStalled() const { return cycles_stalled_; }

  void setVerbose(bool verbose) {
    verbose_ = verbose;
    dram_->setVerbose(verbose);
  }

  void directWrite(uint32_t address, int32_t data) {
    if (address < data_cache_.size()) {
      data_cache_[address] = data;
    }
  }

  int32_t directRead(uint32_t address) {
    if (address < data_cache_.size()) {
      return data_cache_[address];
    }
    return 0;
  }

  void printStats() const {
    std::cout << "\n=== LSU Statistics ===" << std::endl;
    std::cout << "Operations: " << operations_completed_ << std::endl;
    std::cout << "Stalled Cycles: " << cycles_stalled_ << std::endl;
    std::cout << "Pending: " << pending_requests_.size() << std::endl;
    dram_->printStats();
  }

 private:
  enum class State { IDLE, PROCESSING, WAITING_DRAM };

  struct PendingRequest {
    uint64_t request_id;
    std::shared_ptr<MemoryRequestPacket> mem_request;
    uint32_t current_addr;
    bool is_write;
  };

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

    // Issue transaction to DRAMsim3
    bool is_write = (current_request_->getOperation() == LSUOp::STORE ||
                     current_request_->getOperation() == LSUOp::STORE_VECTOR);

    uint64_t request_id = next_request_id_++;

    bool accepted = false;
    if (is_write) {
      // For writes, store data to cache first
      if (current_addr < data_cache_.size()) {
        data_cache_[current_addr] = current_request_->getData();
      }
      accepted = dram_->issueWrite(current_addr, request_id);
    } else {
      accepted = dram_->issueRead(current_addr, request_id);
    }

    if (accepted) {
      // Track pending request
      PendingRequest pending;
      pending.request_id = request_id;
      pending.mem_request = current_request_;
      pending.current_addr = current_addr;
      pending.is_write = is_write;
      pending_requests_[request_id] = pending;

      if (verbose_) {
        std::cout << "[" << scheduler_.getCurrentTime() << "] " << getName()
                  << ": Issued " << (is_write ? "WRITE" : "READ")
                  << " to DRAM, addr=" << current_addr << " id=" << request_id
                  << std::endl;
      }

      element_index_++;
      current_state_ = State::WAITING_DRAM;
    } else {
      // DRAM queue full, stall
      cycles_stalled_++;
      if (verbose_) {
        std::cout << "[" << scheduler_.getCurrentTime() << "] " << getName()
                  << ": DRAM queue full, stalling" << std::endl;
      }
    }
  }

  void handleWaitingDRAM() {
    // Check if we have any completed transactions
    // This is handled in processCompletedDRAMTransactions()

    // If current vector operation has more elements, continue processing
    if (element_index_ < vector_length_) {
      current_state_ = State::PROCESSING;
    } else {
      // All elements issued, but wait for completions
      if (pending_requests_.empty()) {
        current_state_ = State::IDLE;
        current_request_ = nullptr;
      }
    }
  }

  void processCompletedDRAMTransactions() {
    // Process completed reads
    while (dram_->hasCompletedReads()) {
      uint64_t request_id = dram_->popCompletedRead();
      auto it = pending_requests_.find(request_id);

      if (it != pending_requests_.end()) {
        auto& pending = it->second;

        // Read data from cache
        int32_t data = 0;
        if (pending.current_addr < data_cache_.size()) {
          data = data_cache_[pending.current_addr];
        }

        // Create response
        current_response_ = std::make_shared<MemoryResponsePacket>(data, true);
        operations_completed_++;

        if (verbose_) {
          std::cout << "[" << scheduler_.getCurrentTime() << "] " << getName()
                    << ": READ completed, addr=" << pending.current_addr
                    << " data=" << data << std::endl;
        }

        pending_requests_.erase(it);
      }
    }

    // Process completed writes
    while (dram_->hasCompletedWrites()) {
      uint64_t request_id = dram_->popCompletedWrite();
      auto it = pending_requests_.find(request_id);

      if (it != pending_requests_.end()) {
        auto& pending = it->second;

        // Create response (write doesn't return data)
        current_response_ = std::make_shared<MemoryResponsePacket>(0, true);
        operations_completed_++;

        if (verbose_) {
          std::cout << "[" << scheduler_.getCurrentTime() << "] " << getName()
                    << ": WRITE completed, addr=" << pending.current_addr
                    << std::endl;
        }

        pending_requests_.erase(it);
      }
    }
  }

  std::shared_ptr<DRAMsim3Wrapper> dram_;
  std::vector<int32_t> data_cache_;  // Simple cache for data storage

  size_t queue_depth_;
  std::queue<std::shared_ptr<MemoryRequestPacket>> request_queue_;

  State current_state_;
  std::shared_ptr<MemoryRequestPacket> current_request_;
  std::shared_ptr<MemoryResponsePacket> current_response_;

  size_t element_index_;
  size_t vector_length_;
  size_t operations_completed_;
  size_t cycles_stalled_;

  uint64_t next_request_id_;
  std::unordered_map<uint64_t, PendingRequest> pending_requests_;

  bool verbose_ = false;
};

#endif  // LSU_DRAMSIM3_H
