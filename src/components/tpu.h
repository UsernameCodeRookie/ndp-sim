#ifndef TPU_H
#define TPU_H

#include <iostream>
#include <memory>
#include <vector>

#include "../port.h"
#include "../tick.h"
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
 * @brief Systolic Array TPU - Unified TPU Class
 *
 * Provides low-level hardware primitives:
 * - 2D MAC array for matrix operations
 * - Unified memory access via LSU
 * - Basic control signals
 *
 * Does NOT contain high-level operations like GEMM.
 * Operators should use these primitives to implement algorithms.
 *
 * Use USE_DRAMSIM3 macro to enable DRAMsim3 integration for LSU.
 */
class SystolicArrayTPU : public Architecture::TickingComponent {
 public:
  SystolicArrayTPU(const std::string& name,
                   EventDriven::EventScheduler& scheduler, uint64_t period,
                   size_t array_size = 4
#ifdef USE_DRAMSIM3
                   ,
                   const std::string& config_file = "",
                   const std::string& output_dir = ""
#endif
                   )
      : Architecture::TickingComponent(name, scheduler, period),
        array_size_(array_size) {
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

    // Create LSU for memory access with large capacity
#ifdef USE_DRAMSIM3
    if (!config_file.empty()) {
      // LSU with DRAMsim3
      lsu_ =
          std::make_shared<LoadStoreUnit>(name + "_LSU", scheduler, period, 8,
                                          4, 8192, config_file, output_dir);
    } else {
      // LSU without DRAMsim3
      lsu_ = std::make_shared<LoadStoreUnit>(name + "_LSU", scheduler, period,
                                             8, 4, 8192);
    }
#else
    // LSU without DRAMsim3
    lsu_ = std::make_shared<LoadStoreUnit>(name + "_LSU", scheduler, period, 8,
                                           4, 8192);  // 8KB per bank
#endif
    lsu_->start();
  }

  void tick() override {
    // Basic hardware tick - no high-level logic here
  }

  // ========== Hardware Primitives ==========

  /**
   * @brief Get the systolic array size
   */
  size_t getArraySize() const { return array_size_; }

  /**
   * @brief Access a specific MAC unit
   */
  std::shared_ptr<MACUnit> getMAC(size_t row, size_t col) {
    if (row < array_size_ && col < array_size_) {
      return mac_array_[row][col];
    }
    return nullptr;
  }

  /**
   * @brief Reset all MAC units
   */
  void resetAllMACs() {
    for (size_t i = 0; i < array_size_; ++i) {
      for (size_t j = 0; j < array_size_; ++j) {
        mac_array_[i][j]->resetAccumulator();
      }
    }
  }

  /**
   * @brief Get the Load-Store Unit
   */
  std::shared_ptr<LoadStoreUnit> getLSU() { return lsu_; }

  /**
   * @brief Direct memory write (for initialization/testing)
   */
  void writeMemory(uint32_t address, int32_t data) {
    lsu_->directWrite(address, data);
  }

  /**
   * @brief Direct memory read (for testing/verification)
   */
  int32_t readMemory(uint32_t address) { return lsu_->directRead(address); }

  /**
   * @brief Write a vector to memory
   */
  void writeMemoryBlock(uint32_t base_addr, const std::vector<int>& data) {
    for (size_t i = 0; i < data.size(); ++i) {
      writeMemory(base_addr + i, data[i]);
    }
  }

  /**
   * @brief Read a vector from memory
   */
  std::vector<int> readMemoryBlock(uint32_t base_addr, size_t size) {
    std::vector<int> result(size);
    for (size_t i = 0; i < size; ++i) {
      result[i] = readMemory(base_addr + i);
    }
    return result;
  }

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
  size_t array_size_;
  std::vector<std::vector<std::shared_ptr<MACUnit>>> mac_array_;
  std::shared_ptr<LoadStoreUnit> lsu_;
  bool verbose_ = false;
};

#endif  // TPU_H
