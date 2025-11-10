#ifndef OPERATOR_BASE_H
#define OPERATOR_BASE_H

#include <iostream>
#include <memory>
#include <string>
#include <vector>

namespace Operators {

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
    for (auto& val : data_) {
      val = min_val + (rand() % (max_val - min_val + 1));
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
 */
class OperatorBase {
 public:
  OperatorBase(const std::string& name) : name_(name), verbose_(false) {}
  virtual ~OperatorBase() = default;

  virtual void compute() = 0;
  virtual std::string getOpType() const = 0;

  const std::string& getName() const { return name_; }
  void setVerbose(bool verbose) { verbose_ = verbose; }
  bool isVerbose() const { return verbose_; }

  virtual void printInfo() const {
    std::cout << "Operator: " << name_ << " [" << getOpType() << "]\n";
  }

 protected:
  std::string name_;
  bool verbose_;
};

}  // namespace Operators

#endif  // OPERATOR_BASE_H
