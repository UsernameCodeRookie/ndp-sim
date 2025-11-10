#ifndef TPU_H
#define TPU_H

#include <iostream>
#include <memory>
#include <queue>
#include <string>
#include <vector>

#include "../port.h"
#include "../tick_component.h"
#include "int_packet.h"
#include "lsu.h"

/**
 * @brief Matrix Element (for TPU operations)
 */
struct MatrixElement {
  int row;
  int col;
  int value;

  MatrixElement(int r = 0, int c = 0, int v = 0) : row(r), col(c), value(v) {}
};

/**
 * @brief MAC (Multiply-Accumulate) Unit
 *
 * Basic computation unit for TPU
 * Performs: result = accumulator + (input_a * input_b)
 */
class MACUnit : public Architecture::TickingComponent {
 public:
  MACUnit(const std::string& name, EventDriven::EventScheduler& scheduler,
          uint64_t period, int row, int col)
      : Architecture::TickingComponent(name, scheduler, period),
        row_(row),
        col_(col),
        accumulator_(0),
        input_a_(0),
        input_b_(0),
        output_valid_(false) {}

  void tick() override {
    // Perform MAC operation
    int product = input_a_ * input_b_;
    accumulator_ += product;
    output_valid_ = true;

    if (verbose_) {
      std::cout << "[" << scheduler_.getCurrentTime() << "] " << getName()
                << " [" << row_ << "," << col_ << "]: " << accumulator_
                << " += " << input_a_ << " * " << input_b_ << " = "
                << accumulator_ << std::endl;
    }
  }

  void setInputA(int value) { input_a_ = value; }
  void setInputB(int value) { input_b_ = value; }
  void resetAccumulator() {
    accumulator_ = 0;
    output_valid_ = false;
  }

  int getAccumulator() const { return accumulator_; }
  bool isOutputValid() const { return output_valid_; }

  int getRow() const { return row_; }
  int getCol() const { return col_; }

  void setVerbose(bool verbose) { verbose_ = verbose; }

 private:
  int row_;
  int col_;
  int accumulator_;
  int input_a_;
  int input_b_;
  bool output_valid_;
  bool verbose_ = false;
};

/**
 * @brief Systolic Array TPU
 *
 * A simple Tensor Processing Unit using systolic array architecture
 * Features:
 * - 2D grid of MAC units
 * - Systolic data flow (data flows through the array)
 * - Integrated LSU for memory access
 * - GEMM (General Matrix Multiply) support
 *
 * Matrix multiplication: C = A * B
 * - A: M x K matrix
 * - B: K x N matrix
 * - C: M x N matrix (result)
 */
class SystolicArrayTPU : public Architecture::TickingComponent {
 public:
  SystolicArrayTPU(const std::string& name,
                   EventDriven::EventScheduler& scheduler, uint64_t period,
                   size_t array_size = 4)
      : Architecture::TickingComponent(name, scheduler, period),
        array_size_(array_size),
        current_state_(State::IDLE),
        gemm_m_(0),
        gemm_n_(0),
        gemm_k_(0),
        current_k_(0),
        operations_completed_(0) {
    // Create systolic array (2D grid of MAC units)
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

    // Create unified buffer (weight stationary)
    weight_buffer_.resize(array_size_ * array_size_, 0);
    input_buffer_.resize(array_size_ * array_size_, 0);
    output_buffer_.resize(array_size_ * array_size_, 0);

    // Create LSU for memory access
    lsu_ = std::make_shared<LoadStoreUnit>(name + "_LSU", scheduler, period, 8,
                                           4, 256);
    lsu_->start();

    // Create ports
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

  /**
   * @brief Start GEMM computation (blocking version for simplicity)
   *
   * Performs C = A * B where:
   * - A is stored at base_addr_a (M x K matrix)
   * - B is stored at base_addr_b (K x N matrix)
   * - C will be stored at base_addr_c (M x N matrix)
   */
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
                << ": Starting GEMM (" << M << "x" << K << ") * (" << K << "x"
                << N << ")" << std::endl;
    }

    // Reset all MAC units
    for (size_t i = 0; i < array_size_; ++i) {
      for (size_t j = 0; j < array_size_; ++j) {
        mac_array_[i][j]->resetAccumulator();
      }
    }

    // Perform computation directly (simplified for demonstration)
    performGEMM();

    current_state_ = State::DONE;
  }

  /**
   * @brief Perform GEMM computation directly
   */
  void performGEMM() {
    // Load A and B matrices from memory using direct access
    std::vector<int> A = readMatrixFromMemory(base_addr_a_, gemm_m_ * gemm_k_);
    std::vector<int> B = readMatrixFromMemory(base_addr_b_, gemm_k_ * gemm_n_);

    // Initialize result matrix
    std::vector<int> C(gemm_m_ * gemm_n_, 0);

    // Perform tiled GEMM computation
    for (size_t k_tile = 0; k_tile < gemm_k_; ++k_tile) {
      if (verbose_) {
        std::cout << "[" << scheduler_.getCurrentTime() << "] " << getName()
                  << ": Computing k_step=" << k_tile << "/" << gemm_k_
                  << std::endl;
      }

      // Feed data to MAC array and accumulate
      for (size_t i = 0; i < std::min(array_size_, gemm_m_); ++i) {
        for (size_t j = 0; j < std::min(array_size_, gemm_n_); ++j) {
          int a_val = A[i * gemm_k_ + k_tile];
          int b_val = B[k_tile * gemm_n_ + j];

          // Use MAC: accumulate += a * b
          mac_array_[i][j]->setInputA(a_val);
          mac_array_[i][j]->setInputB(b_val);

          // Accumulate in local buffer
          C[i * gemm_n_ + j] += a_val * b_val;

          if (verbose_) {
            std::cout << "[" << scheduler_.getCurrentTime() << "] "
                      << "MAC[" << i << "][" << j << "]: " << C[i * gemm_n_ + j]
                      << " (added " << a_val << " * " << b_val << ")"
                      << std::endl;
          }
        }
      }

      operations_completed_ +=
          std::min(array_size_, gemm_m_) * std::min(array_size_, gemm_n_);
    }

    // Store results to memory using direct access
    for (size_t i = 0; i < gemm_m_; ++i) {
      for (size_t j = 0; j < gemm_n_; ++j) {
        int result = C[i * gemm_n_ + j];
        uint32_t addr = base_addr_c_ + i * gemm_n_ + j;
        lsu_->directWrite(addr, result);
      }
    }
  }

  /**
   * @brief Load matrix data into memory (for testing)
   */
  void loadMatrixToMemory(const std::vector<int>& matrix, uint32_t base_addr) {
    for (size_t i = 0; i < matrix.size(); ++i) {
      lsu_->directWrite(base_addr + i, matrix[i]);
    }
  }

  /**
   * @brief Read matrix data from memory (for verification)
   */
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
    // Load B matrix tile from memory using LSU
    if (verbose_) {
      std::cout << "[" << scheduler_.getCurrentTime() << "] " << getName()
                << ": Loading weights (tile of B matrix)" << std::endl;
    }

    // Load B matrix tile via LSU
    for (size_t i = 0; i < std::min(array_size_, gemm_k_); ++i) {
      for (size_t j = 0; j < std::min(array_size_, gemm_n_); ++j) {
        uint32_t addr = base_addr_b_ + i * gemm_n_ + j;

        // Use LSU to load data
        auto load_req = std::make_shared<MemoryRequestPacket>(LSUOp::LOAD, addr,
                                                              0, 1, 1, true);

        auto lsu_req = lsu_->getPort("req_in");
        auto lsu_valid = lsu_->getPort("valid");
        auto lsu_resp = lsu_->getPort("resp_out");
        auto valid_signal = std::make_shared<IntDataPacket>(1);

        lsu_req->write(
            std::static_pointer_cast<Architecture::DataPacket>(load_req));
        lsu_valid->write(
            std::static_pointer_cast<Architecture::DataPacket>(valid_signal));

        // Wait for response
        for (int cycle = 0; cycle < 10; ++cycle) {
          scheduler_.run(1);
        }

        auto response = lsu_resp->read();
        auto mem_resp =
            std::dynamic_pointer_cast<MemoryResponsePacket>(response);
        if (mem_resp) {
          weight_buffer_[i * array_size_ + j] = mem_resp->getData();
        }
      }
    }

    current_state_ = State::LOAD_INPUT;
  }

  void handleLoadInput() {
    // Load A matrix tile from memory using LSU
    if (verbose_) {
      std::cout << "[" << scheduler_.getCurrentTime() << "] " << getName()
                << ": Loading input (tile of A matrix)" << std::endl;
    }

    // Load A matrix tile via LSU
    for (size_t i = 0; i < std::min(array_size_, gemm_m_); ++i) {
      for (size_t j = 0; j < std::min(array_size_, gemm_k_); ++j) {
        uint32_t addr = base_addr_a_ + i * gemm_k_ + j;

        // Use LSU to load data
        auto load_req = std::make_shared<MemoryRequestPacket>(LSUOp::LOAD, addr,
                                                              0, 1, 1, true);

        auto lsu_req = lsu_->getPort("req_in");
        auto lsu_valid = lsu_->getPort("valid");
        auto lsu_resp = lsu_->getPort("resp_out");
        auto valid_signal = std::make_shared<IntDataPacket>(1);

        lsu_req->write(
            std::static_pointer_cast<Architecture::DataPacket>(load_req));
        lsu_valid->write(
            std::static_pointer_cast<Architecture::DataPacket>(valid_signal));

        // Wait for response
        for (int cycle = 0; cycle < 10; ++cycle) {
          scheduler_.run(1);
        }

        auto response = lsu_resp->read();
        auto mem_resp =
            std::dynamic_pointer_cast<MemoryResponsePacket>(response);
        if (mem_resp) {
          input_buffer_[i * array_size_ + j] = mem_resp->getData();
        }
      }
    }

    current_state_ = State::COMPUTE;
  }

  void handleCompute() {
    if (verbose_) {
      std::cout << "[" << scheduler_.getCurrentTime() << "] " << getName()
                << ": Computing (k_step=" << current_k_ << "/" << gemm_k_ << ")"
                << std::endl;
    }

    // Perform systolic array computation
    // In real systolic array, data flows through the array
    // Here we simplify by directly feeding inputs to MACs

    for (size_t i = 0; i < std::min(array_size_, gemm_m_); ++i) {
      for (size_t j = 0; j < std::min(array_size_, gemm_n_); ++j) {
        if (current_k_ < gemm_k_) {
          // MAC computes: C[i][j] += A[i][current_k_] * B[current_k_][j]
          int input_a = input_buffer_[i * array_size_ + current_k_];
          int input_b = weight_buffer_[current_k_ * array_size_ + j];

          mac_array_[i][j]->setInputA(input_a);
          mac_array_[i][j]->setInputB(input_b);
          // Note: MAC units tick automatically as TickingComponents
        }
      }
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
                << ": Storing output (C matrix)" << std::endl;
    }

    // Collect results from MAC units
    for (size_t i = 0; i < std::min(array_size_, gemm_m_); ++i) {
      for (size_t j = 0; j < std::min(array_size_, gemm_n_); ++j) {
        int result = mac_array_[i][j]->getAccumulator();
        output_buffer_[i * array_size_ + j] = result;

        // Store to memory via LSU
        uint32_t addr = base_addr_c_ + i * gemm_n_ + j;
        auto store_req = std::make_shared<MemoryRequestPacket>(
            LSUOp::STORE, addr, result, 1, 1, true);

        auto lsu_req = lsu_->getPort("req_in");
        auto lsu_valid = lsu_->getPort("valid");
        auto valid_signal = std::make_shared<IntDataPacket>(1);

        lsu_req->write(
            std::static_pointer_cast<Architecture::DataPacket>(store_req));
        lsu_valid->write(
            std::static_pointer_cast<Architecture::DataPacket>(valid_signal));
      }
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
  std::shared_ptr<LoadStoreUnit> lsu_;

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

#endif  // TPU_H
