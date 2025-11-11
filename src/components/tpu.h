#ifndef TPU_H
#define TPU_H

#include <iostream>
#include <memory>
#include <vector>

#include "../port.h"
#include "../tick.h"
#include "int_packet.h"
#include "lsu.h"
#include "precision.h"

template <typename PrecisionTraits>
class MACUnit : public Architecture::TickingComponent {
 public:
  using Traits = PrecisionTraits;
  using ValueType = typename Traits::ValueType;
  using AccumulatorType = typename Traits::AccumulatorType;

  MACUnit(const std::string& name, EventDriven::EventScheduler& scheduler,
          uint64_t period, int row, int col)
      : Architecture::TickingComponent(name, scheduler, period),
        row_(row),
        col_(col),
        accumulator_(Traits::zeroAccumulator()),
        input_a_(Traits::zeroValue()),
        input_b_(Traits::zeroValue()),
        output_valid_(false) {}

  void tick() override {
    accumulator_ = Traits::accumulate(accumulator_, input_a_, input_b_);
    output_valid_ = true;

    if (verbose_) {
      const auto acc_value = Traits::fromAccumulator(accumulator_);
      std::cout << "[" << scheduler_.getCurrentTime() << "] " << getName()
                << " [" << row_ << "," << col_
                << "]: " << Traits::toString(acc_value)
                << " += " << Traits::toString(input_a_) << " * "
                << Traits::toString(input_b_) << std::endl;
    }
  }

  void setInputA(ValueType value) { input_a_ = value; }
  void setInputB(ValueType value) { input_b_ = value; }

  void resetAccumulator() {
    accumulator_ = Traits::zeroAccumulator();
    output_valid_ = false;
  }

  AccumulatorType getAccumulator() const { return accumulator_; }
  bool isOutputValid() const { return output_valid_; }

  int getRow() const { return row_; }
  int getCol() const { return col_; }

  void setVerbose(bool verbose) { verbose_ = verbose; }

 private:
  int row_;
  int col_;
  AccumulatorType accumulator_;
  ValueType input_a_;
  ValueType input_b_;
  bool output_valid_;
  bool verbose_ = false;
};

class SystolicArrayTPUBase : public Architecture::TickingComponent {
 public:
  SystolicArrayTPUBase(const std::string& name,
                       EventDriven::EventScheduler& scheduler, uint64_t period)
      : Architecture::TickingComponent(name, scheduler, period) {}

  ~SystolicArrayTPUBase() override = default;

  virtual size_t getArraySize() const = 0;
  virtual void resetAllMACs() = 0;
  virtual void setVerbose(bool verbose) = 0;
  virtual const char* getPrecisionName() const = 0;
};

template <typename PrecisionTraits>
class SystolicArrayTPU : public SystolicArrayTPUBase {
 public:
  using Traits = PrecisionTraits;
  using ValueType = typename Traits::ValueType;

  SystolicArrayTPU(const std::string& name,
                   EventDriven::EventScheduler& scheduler, uint64_t period,
                   size_t array_size = 4
#ifdef USE_DRAMSIM3
                   ,
                   const std::string& config_file = "",
                   const std::string& output_dir = ""
#endif
                   )
      : SystolicArrayTPUBase(name, scheduler, period), array_size_(array_size) {
    mac_array_.resize(array_size_);
    for (size_t i = 0; i < array_size_; ++i) {
      mac_array_[i].resize(array_size_);
      for (size_t j = 0; j < array_size_; ++j) {
        auto mac = std::make_shared<MACUnit<PrecisionTraits>>(
            name + "_MAC_" + std::to_string(i) + "_" + std::to_string(j),
            scheduler, period, static_cast<int>(i), static_cast<int>(j));
        mac_array_[i][j] = mac;
        mac->start();
      }
    }

#ifdef USE_DRAMSIM3
    if (!config_file.empty()) {
      lsu_ =
          std::make_shared<LoadStoreUnit>(name + "_LSU", scheduler, period, 8,
                                          4, 8192, config_file, output_dir);
    } else {
      lsu_ = std::make_shared<LoadStoreUnit>(name + "_LSU", scheduler, period,
                                             8, 4, 8192);
    }
#else
    lsu_ = std::make_shared<LoadStoreUnit>(name + "_LSU", scheduler, period, 8,
                                           4, 8192);
#endif
    lsu_->start();
  }

  void tick() override {
    // Hardware tick placeholder
  }

  size_t getArraySize() const override { return array_size_; }

  std::shared_ptr<MACUnit<PrecisionTraits>> getMAC(size_t row, size_t col) {
    if (row < array_size_ && col < array_size_) {
      return mac_array_[row][col];
    }
    return nullptr;
  }

  void resetAllMACs() override {
    for (size_t i = 0; i < array_size_; ++i) {
      for (size_t j = 0; j < array_size_; ++j) {
        mac_array_[i][j]->resetAccumulator();
      }
    }
  }

  std::shared_ptr<LoadStoreUnit> getLSU() { return lsu_; }

  void writeMemory(uint32_t address, ValueType data) {
    lsu_->directWrite(address, Traits::encode(data));
  }

  ValueType readMemory(uint32_t address) {
    return Traits::decode(lsu_->directRead(address));
  }

  void writeMemoryBlock(uint32_t base_addr,
                        const std::vector<ValueType>& data) {
    for (size_t i = 0; i < data.size(); ++i) {
      writeMemory(base_addr + static_cast<uint32_t>(i), data[i]);
    }
  }

  std::vector<ValueType> readMemoryBlock(uint32_t base_addr, size_t size) {
    std::vector<ValueType> result(size);
    for (size_t i = 0; i < size; ++i) {
      result[i] = readMemory(base_addr + static_cast<uint32_t>(i));
    }
    return result;
  }

  void setVerbose(bool verbose) override {
    verbose_ = verbose;
    lsu_->setVerbose(verbose);
    for (size_t i = 0; i < array_size_; ++i) {
      for (size_t j = 0; j < array_size_; ++j) {
        mac_array_[i][j]->setVerbose(verbose);
      }
    }
  }

  const char* getPrecisionName() const override { return Traits::name(); }

 private:
  size_t array_size_;
  std::vector<std::vector<std::shared_ptr<MACUnit<PrecisionTraits>>>>
      mac_array_;
  std::shared_ptr<LoadStoreUnit> lsu_;
  bool verbose_ = false;
};

#endif  // TPU_H
