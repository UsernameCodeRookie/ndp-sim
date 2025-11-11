#ifndef DRAM_H
#define DRAM_H

#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <queue>
#include <string>

#include "../../third_party/DRAMsim3/src/memory_system.h"

/**
 * @brief DRAMsim3 Memory Wrapper
 *
 * Wraps DRAMsim3 MemorySystem for event-driven simulation.
 */
class DRAMsim3Wrapper {
 public:
  DRAMsim3Wrapper(const std::string& config_file, const std::string& output_dir)
      : config_file_(config_file),
        output_dir_(output_dir),
        cycle_count_(0),
        total_reads_(0),
        total_writes_(0),
        verbose_(false) {
    auto read_cb = [this](uint64_t addr) { this->readCallback(addr); };
    auto write_cb = [this](uint64_t addr) { this->writeCallback(addr); };

    memory_system_ =
        std::unique_ptr<dramsim3::MemorySystem>(dramsim3::GetMemorySystem(
            config_file_, output_dir_, read_cb, write_cb));

    if (verbose_) {
      std::cout << "DRAMsim3 initialized: TCK=" << memory_system_->GetTCK()
                << "ns, BusWidth=" << memory_system_->GetBusBits() << "bits"
                << std::endl;
    }
  }

  void tick() {
    memory_system_->ClockTick();
    cycle_count_++;
    processCompletedTransactions();
  }

  bool issueRead(uint64_t addr, uint64_t request_id) {
    if (!memory_system_->WillAcceptTransaction(addr, false)) {
      return false;
    }

    if (memory_system_->AddTransaction(addr, false)) {
      pending_reads_[addr] = request_id;
      total_reads_++;
      if (verbose_) {
        std::cout << "[" << cycle_count_ << "] READ: addr=0x" << std::hex
                  << addr << std::dec << " id=" << request_id << std::endl;
      }
      return true;
    }
    return false;
  }

  bool issueWrite(uint64_t addr, uint64_t request_id) {
    if (!memory_system_->WillAcceptTransaction(addr, true)) {
      return false;
    }

    if (memory_system_->AddTransaction(addr, true)) {
      pending_writes_[addr] = request_id;
      total_writes_++;
      if (verbose_) {
        std::cout << "[" << cycle_count_ << "] WRITE: addr=0x" << std::hex
                  << addr << std::dec << " id=" << request_id << std::endl;
      }
      return true;
    }
    return false;
  }

  bool hasCompletedReads() const { return !completed_reads_.empty(); }
  bool hasCompletedWrites() const { return !completed_writes_.empty(); }

  uint64_t popCompletedRead() {
    if (completed_reads_.empty()) return 0;
    uint64_t id = completed_reads_.front();
    completed_reads_.pop();
    return id;
  }

  uint64_t popCompletedWrite() {
    if (completed_writes_.empty()) return 0;
    uint64_t id = completed_writes_.front();
    completed_writes_.pop();
    return id;
  }

  size_t getPendingReads() const { return pending_reads_.size(); }
  size_t getPendingWrites() const { return pending_writes_.size(); }

  void printStats() const {
    std::cout << "\n=== DRAMsim3 Statistics ===" << std::endl;
    std::cout << "Total Cycles: " << cycle_count_ << std::endl;
    std::cout << "Total Reads: " << total_reads_ << std::endl;
    std::cout << "Total Writes: " << total_writes_ << std::endl;
    std::cout << "Pending Reads: " << pending_reads_.size() << std::endl;
    std::cout << "Pending Writes: " << pending_writes_.size() << std::endl;
    memory_system_->PrintStats();
  }

  void resetStats() {
    memory_system_->ResetStats();
    total_reads_ = 0;
    total_writes_ = 0;
  }

  void setVerbose(bool verbose) { verbose_ = verbose; }
  uint64_t getCycleCount() const { return cycle_count_; }

 private:
  void readCallback(uint64_t addr) {
    auto it = pending_reads_.find(addr);
    if (it != pending_reads_.end()) {
      uint64_t request_id = it->second;
      completed_reads_.push(request_id);
      pending_reads_.erase(it);
      if (verbose_) {
        std::cout << "[" << cycle_count_ << "] READ done: addr=0x" << std::hex
                  << addr << std::dec << " id=" << request_id << std::endl;
      }
    }
  }

  void writeCallback(uint64_t addr) {
    auto it = pending_writes_.find(addr);
    if (it != pending_writes_.end()) {
      uint64_t request_id = it->second;
      completed_writes_.push(request_id);
      pending_writes_.erase(it);
      if (verbose_) {
        std::cout << "[" << cycle_count_ << "] WRITE done: addr=0x" << std::hex
                  << addr << std::dec << " id=" << request_id << std::endl;
      }
    }
  }

  void processCompletedTransactions() {
    // Callbacks handle completion
  }

  std::string config_file_;
  std::string output_dir_;
  std::unique_ptr<dramsim3::MemorySystem> memory_system_;

  uint64_t cycle_count_;
  uint64_t total_reads_;
  uint64_t total_writes_;

  // Pending transactions: addr -> request_id
  std::map<uint64_t, uint64_t> pending_reads_;
  std::map<uint64_t, uint64_t> pending_writes_;

  // Completed transactions
  std::queue<uint64_t> completed_reads_;
  std::queue<uint64_t> completed_writes_;

  bool verbose_;
};

#endif  // DRAM_H
