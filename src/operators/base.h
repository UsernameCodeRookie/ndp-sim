#ifndef OPERATOR_BASE_H
#define OPERATOR_BASE_H

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <type_traits>
#include <vector>

// Forward declarations
class SystolicArrayTPUBase;

namespace Operators {

/**
 * @brief Execution backend type
 */
enum class ExecutionBackend {
  CPU,  // CPU execution
  TPU,  // TPU hardware acceleration
  AUTO  // Automatically select based on availability
};

/**
 * @brief Tensor shape descriptor
 */
struct TensorShape {
  std::vector<size_t> dims;

  TensorShape() = default;
  TensorShape(std::initializer_list<size_t> init) : dims(init) {}

  size_t size() const {
    size_t total = 1;
    for (auto d : dims) total *= d;
    return total;
  }

  size_t rank() const { return dims.size(); }

  size_t operator[](size_t idx) const { return dims[idx]; }
  size_t& operator[](size_t idx) { return dims[idx]; }

  std::string toString() const {
    std::string result = "[";
    for (size_t i = 0; i < dims.size(); ++i) {
      result += std::to_string(dims[i]);
      if (i < dims.size() - 1) result += ", ";
    }
    result += "]";
    return result;
  }
};

/**
 * @brief Tensor wrapper for operator inputs/outputs
 */
template <typename T = int>
class Tensor {
 public:
  Tensor() = default;
  Tensor(const TensorShape& shape) : shape_(shape) {
    data_.resize(shape.size());
  }
  Tensor(const TensorShape& shape, T init_val) : shape_(shape) {
    data_.resize(shape.size(), init_val);
  }

  const TensorShape& shape() const { return shape_; }
  std::vector<T>& data() { return data_; }
  const std::vector<T>& data() const { return data_; }

  size_t size() const { return data_.size(); }

  T& operator[](size_t idx) { return data_[idx]; }
  const T& operator[](size_t idx) const { return data_[idx]; }

  // 2D matrix access
  T& at(size_t row, size_t col) {
    if (shape_.rank() != 2) {
      throw std::runtime_error("Tensor is not 2D");
    }
    return data_[row * shape_[1] + col];
  }

  const T& at(size_t row, size_t col) const {
    if (shape_.rank() != 2) {
      throw std::runtime_error("Tensor is not 2D");
    }
    return data_[row * shape_[1] + col];
  }

  void fill(T value) { std::fill(data_.begin(), data_.end(), value); }

  void fillRandom(T min_val = 0, T max_val = 10) {
    if constexpr (std::is_integral_v<T>) {
      int min_i = static_cast<int>(min_val);
      int max_i = static_cast<int>(max_val);
      if (max_i < min_i) std::swap(max_i, min_i);
      int span = max_i - min_i;
      for (auto& val : data_) {
        int offset = span > 0 ? (rand() % (span + 1)) : 0;
        val = static_cast<T>(min_i + offset);
      }
    } else if constexpr (std::is_floating_point_v<T>) {
      for (auto& val : data_) {
        float r = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
        val = static_cast<T>(min_val + r * (max_val - min_val));
      }
    }
  }

  void print(const std::string& name = "Tensor") const {
    std::cout << name << " " << shape_.toString() << ":\n";
    if (shape_.rank() == 2) {
      for (size_t i = 0; i < shape_[0]; ++i) {
        for (size_t j = 0; j < shape_[1]; ++j) {
          std::cout << at(i, j) << " ";
        }
        std::cout << "\n";
      }
    } else {
      for (size_t i = 0; i < std::min(size(), size_t(20)); ++i) {
        std::cout << data_[i] << " ";
      }
      if (size() > 20) std::cout << "...";
      std::cout << "\n";
    }
  }

 private:
  TensorShape shape_;
  std::vector<T> data_;
};

/**
 * @brief Base class for all operators
 *
 * All operators support multiple execution backends (CPU, TPU).
 * Hardware scheduling logic resides in operators, not in hardware components.
 */
class OperatorBase {
 public:
  OperatorBase(const std::string& name)
      : name_(name),
        verbose_(false),
        tpu_(nullptr),
        backend_(ExecutionBackend::CPU) {}
  virtual ~OperatorBase() = default;

  virtual void compute() = 0;
  virtual std::string getOpType() const = 0;

  const std::string& getName() const { return name_; }
  void setVerbose(bool verbose) { verbose_ = verbose; }
  bool isVerbose() const { return verbose_; }

  /**
   * @brief Bind to a TPU for hardware acceleration
   */
  virtual void bindTPU(std::shared_ptr<SystolicArrayTPUBase> tpu) {
    tpu_ = tpu;
    backend_ = ExecutionBackend::TPU;
  }

  /**
   * @brief Unbind from TPU (use CPU)
   */
  virtual void unbindTPU() {
    tpu_ = nullptr;
    backend_ = ExecutionBackend::CPU;
  }

  /**
   * @brief Set execution backend
   */
  void setBackend(ExecutionBackend backend) { backend_ = backend; }

  /**
   * @brief Get current execution backend
   */
  ExecutionBackend getBackend() const {
    if (backend_ == ExecutionBackend::AUTO) {
      return tpu_ ? ExecutionBackend::TPU : ExecutionBackend::CPU;
    }
    return backend_;
  }

  /**
   * @brief Check if using TPU backend
   */
  bool isTPUBound() const {
    return getBackend() == ExecutionBackend::TPU && tpu_ != nullptr;
  }

  /**
   * @brief Check if TPU is available
   */
  bool hasTPU() const { return tpu_ != nullptr; }

  /**
   * @brief Get bound TPU
   */
  std::shared_ptr<SystolicArrayTPUBase> getTPU() const { return tpu_; }

  virtual void printInfo() const {
    std::cout << "Operator: " << name_ << " [" << getOpType() << "]\n";
    std::cout << "  Backend: ";
    switch (getBackend()) {
      case ExecutionBackend::CPU:
        std::cout << "CPU";
        break;
      case ExecutionBackend::TPU:
        std::cout << "TPU";
        break;
      case ExecutionBackend::AUTO:
        std::cout << "AUTO";
        break;
    }
    std::cout << "\n";
    if (hasTPU()) {
      std::cout << "  TPU Precision: " << tpu_->getPrecisionName() << "\n";
    }
  }

 protected:
  std::string name_;
  bool verbose_;
  std::shared_ptr<SystolicArrayTPUBase> tpu_;
  ExecutionBackend backend_;
};

}  // namespace Operators

#endif  // OPERATOR_BASE_H
