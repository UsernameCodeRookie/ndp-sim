#ifndef TPU_DRAMSIM3_H
#define TPU_DRAMSIM3_H

#include <iostream>
#include <memory>
#include <queue>
#include <string>
#include <vector>

#include "../port.h"
#include "../tick_component.h"
#include "int_packet.h"
#include "lsu_dramsim3.h"

/**
 * @brief Systolic Array TPU with DRAMsim3
 *
 * TPU implementation using DRAMsim3 for realistic memory timing.
 */
class SystolicArrayTPUDRAM : public Architecture::TickingComponent {
 public:
  SystolicArrayTPUDRAM(const std::string& name,
                       EventDriven::EventScheduler& scheduler, uint64_t period,
                       const std::string& dram_config,
                       const std::string& dram_output_dir,
                       size_t array_size = 4)
      : Architecture::TickingComponent(name, scheduler, period),
        array_size_(array_size),
        current_state_(State::IDLE),
        gemm_m_(0),
        gemm_n_(0),
        gemm_k_(0),
        current_k_(0),
        operations_completed_(0) {
    // Create MAC array
    mac_array_.resize(array_size_);
    for (size_t i = 0; i < array_size_; ++i) {
      mac_array_[i].resize(array_size_);
      for (size_t j = 0; j < array_size_; ++j) {
        auto mac = std::make_shared<MACUnit>(
            name + "_MAC_" + std::to_string(i) + "_" + std::to_string(j),
            scheduler, period, i, j);
        mac_array_[i][j] = mac;
        mac->start();
      }
    }

    weight_buffer_.resize(array_size_ * array_size_, 0);
    input_buffer_.resize(array_size_ * array_size_, 0);
    output_buffer_.resize(array_size_ * array_size_, 0);

    lsu_ = std::make_shared<LoadStoreUnitDRAM>(
        name + "_LSU", scheduler, period, dram_config, dram_output_dir, 16);
    lsu_->start();

    auto ctrl_in = std::make_shared<Architecture::Port>(
        "ctrl_in", Architecture::PortDirection::INPUT, this);
    addPort(ctrl_in);

    auto status_out = std::make_shared<Architecture::Port>(
        "status_out", Architecture::PortDirection::OUTPUT, this);
    addPort(status_out);
  }

  void tick() override {
    switch (current_state_) {
      case State::IDLE:
        handleIdle();
        break;
      case State::LOAD_WEIGHTS:
        handleLoadWeights();
        break;
      case State::LOAD_INPUT:
        handleLoadInput();
        break;
      case State::COMPUTE:
        handleCompute();
        break;
      case State::STORE_OUTPUT:
        handleStoreOutput();
        break;
      case State::DONE:
        handleDone();
        break;
    }
  }

  void startGEMM(size_t M, size_t N, size_t K, uint32_t base_addr_a,
                 uint32_t base_addr_b, uint32_t base_addr_c) {
    if (current_state_ != State::IDLE) {
      std::cerr << "TPU is busy!" << std::endl;
      return;
    }

    gemm_m_ = M;
    gemm_n_ = N;
    gemm_k_ = K;
    base_addr_a_ = base_addr_a;
    base_addr_b_ = base_addr_b;
    base_addr_c_ = base_addr_c;
    current_k_ = 0;

    if (verbose_) {
      std::cout << "[" << scheduler_.getCurrentTime() << "] " << getName()
                << ": GEMM (" << M << "x" << K << ") * (" << K << "x" << N
                << ")" << std::endl;
    }

    for (size_t i = 0; i < array_size_; ++i) {
      for (size_t j = 0; j < array_size_; ++j) {
        mac_array_[i][j]->resetAccumulator();
      }
    }

    performGEMM();
    current_state_ = State::DONE;
  }

  void performGEMM() {
    std::vector<int> A = readMatrixFromMemory(base_addr_a_, gemm_m_ * gemm_k_);
    std::vector<int> B = readMatrixFromMemory(base_addr_b_, gemm_k_ * gemm_n_);
    std::vector<int> C(gemm_m_ * gemm_n_, 0);

    for (size_t k_tile = 0; k_tile < gemm_k_; ++k_tile) {
      if (verbose_) {
        std::cout << "[" << scheduler_.getCurrentTime() << "] " << getName()
                  << ": k=" << k_tile << "/" << gemm_k_ << std::endl;
      }

      for (size_t i = 0; i < std::min(array_size_, gemm_m_); ++i) {
        for (size_t j = 0; j < std::min(array_size_, gemm_n_); ++j) {
          int a_val = A[i * gemm_k_ + k_tile];
          int b_val = B[k_tile * gemm_n_ + j];

          mac_array_[i][j]->setInputA(a_val);
          mac_array_[i][j]->setInputB(b_val);
          C[i * gemm_n_ + j] += a_val * b_val;

          if (verbose_) {
            std::cout << "  MAC[" << i << "][" << j
                      << "]: " << C[i * gemm_n_ + j] << std::endl;
          }
        }
      }

      operations_completed_ +=
          std::min(array_size_, gemm_m_) * std::min(array_size_, gemm_n_);
    }

    for (size_t i = 0; i < gemm_m_; ++i) {
      for (size_t j = 0; j < gemm_n_; ++j) {
        lsu_->directWrite(base_addr_c_ + i * gemm_n_ + j, C[i * gemm_n_ + j]);
      }
    }
  }

  void loadMatrixToMemory(const std::vector<int>& matrix, uint32_t base_addr) {
    for (size_t i = 0; i < matrix.size(); ++i) {
      lsu_->directWrite(base_addr + i, matrix[i]);
    }
  }

  std::vector<int> readMatrixFromMemory(uint32_t base_addr, size_t size) {
    std::vector<int> result(size);
    for (size_t i = 0; i < size; ++i) {
      result[i] = lsu_->directRead(base_addr + i);
    }
    return result;
  }

  bool isDone() const { return current_state_ == State::DONE; }
  size_t getOperationsCompleted() const { return operations_completed_; }

  void setVerbose(bool verbose) {
    verbose_ = verbose;
    lsu_->setVerbose(verbose);
    for (size_t i = 0; i < array_size_; ++i) {
      for (size_t j = 0; j < array_size_; ++j) {
        mac_array_[i][j]->setVerbose(verbose);
      }
    }
  }

  void printStats() const {
    std::cout << "\n=== TPU Statistics ===" << std::endl;
    std::cout << "Operations: " << operations_completed_ << std::endl;
    lsu_->printStats();
  }

 private:
  enum class State {
    IDLE,
    LOAD_WEIGHTS,
    LOAD_INPUT,
    COMPUTE,
    STORE_OUTPUT,
    DONE
  };

  void handleIdle() {
    // Wait for GEMM command
  }

  void handleLoadWeights() {
    if (verbose_) {
      std::cout << "[" << scheduler_.getCurrentTime() << "] " << getName()
                << ": Loading weights (B matrix) from DRAM" << std::endl;
    }
    current_state_ = State::LOAD_INPUT;
  }

  void handleLoadInput() {
    if (verbose_) {
      std::cout << "[" << scheduler_.getCurrentTime() << "] " << getName()
                << ": Loading input (A matrix) from DRAM" << std::endl;
    }
    current_state_ = State::COMPUTE;
  }

  void handleCompute() {
    if (verbose_) {
      std::cout << "[" << scheduler_.getCurrentTime() << "] " << getName()
                << ": Computing (k_step=" << current_k_ << "/" << gemm_k_ << ")"
                << std::endl;
    }

    current_k_++;
    operations_completed_ +=
        std::min(array_size_, gemm_m_) * std::min(array_size_, gemm_n_);

    if (current_k_ >= gemm_k_) {
      current_state_ = State::STORE_OUTPUT;
    }
  }

  void handleStoreOutput() {
    if (verbose_) {
      std::cout << "[" << scheduler_.getCurrentTime() << "] " << getName()
                << ": Storing output (C matrix) to DRAM" << std::endl;
    }
    current_state_ = State::DONE;
  }

  void handleDone() {
    if (verbose_) {
      std::cout << "[" << scheduler_.getCurrentTime() << "] " << getName()
                << ": GEMM computation done!" << std::endl;
    }

    // Send status
    auto status_out = getPort("status_out");
    auto done_packet = std::make_shared<IntDataPacket>(1);
    status_out->write(
        std::static_pointer_cast<Architecture::DataPacket>(done_packet));

    current_state_ = State::IDLE;
  }

  size_t array_size_;
  std::vector<std::vector<std::shared_ptr<MACUnit>>> mac_array_;
  std::shared_ptr<LoadStoreUnitDRAM> lsu_;

  std::vector<int> weight_buffer_;
  std::vector<int> input_buffer_;
  std::vector<int> output_buffer_;

  State current_state_;
  size_t gemm_m_, gemm_n_, gemm_k_;
  uint32_t base_addr_a_, base_addr_b_, base_addr_c_;
  size_t current_k_;
  size_t operations_completed_;
  bool verbose_ = false;
};

#endif  // TPU_DRAMSIM3_H
