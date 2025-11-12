#ifndef TPU_H
#define TPU_H

#include <iostream>
#include <memory>
#include <vector>

#include "../packet.h"
#include "../port.h"
#include "../tick.h"
#include "../trace.h"
#include "lsu.h"
#include "pe.h"
#include "precision.h"

template <typename PrecisionTraits>
class MACUnit : public ProcessingElement {
 public:
  using Traits = PrecisionTraits;
  using ValueType = typename Traits::ValueType;
  using AccumulatorType = typename Traits::AccumulatorType;

  // Register allocation for MAC operation
  static constexpr int REG_INPUT_A = 0;  // Input A register
  static constexpr int REG_INPUT_B = 1;  // Input B register
  static constexpr int REG_RESULT = 2;   // Result register

  // Timing parameters
  static constexpr uint64_t MAC_LATENCY = 1;  // Cycles for MAC operation

  MACUnit(const std::string& name, EventDriven::EventScheduler& scheduler,
          uint64_t period, int row, int col)
      : ProcessingElement(name, scheduler, period, 32, 4),
        row_(row),
        col_(col),
        output_valid_(false) {
    // Initialize accumulator to zero
    resetAccumulator();
  }

  uint64_t getLatency() const { return MAC_LATENCY; }

  void tick() override {
    // Execute MAC operation using the MAC instruction
    // MAC: accumulator += input_a * input_b

    auto mac_inst = std::make_shared<PEInstructionPacket>(
        ALUOp::MAC, REG_INPUT_A, REG_INPUT_B, REG_RESULT);

    auto inst_in = getPort("inst_in");
    inst_in->write(mac_inst);

    // Call base class tick to execute the MAC operation
    ProcessingElement::tick();

    output_valid_ = true;

    // Trace MAC operation
    int acc_value = getMACAccumulator();
    int input_a = readRegister(REG_INPUT_A);
    int input_b = readRegister(REG_INPUT_B);
    EventDriven::Tracer::getInstance().traceMAC(scheduler_.getCurrentTime(),
                                                getName() + "[" +
                                                    std::to_string(row_) + "," +
                                                    std::to_string(col_) + "]",
                                                acc_value, input_a, input_b);
  }

  void setInputA(ValueType value) {
    // Convert value to int representation for PE register
    int int_value = static_cast<int>(value);
    writeRegister(REG_INPUT_A, int_value);
  }

  void setInputB(ValueType value) {
    // Convert value to int representation for PE register
    int int_value = static_cast<int>(value);
    writeRegister(REG_INPUT_B, int_value);
  }

  void resetAccumulator() {
    resetMACAccumulator();
    writeRegister(REG_RESULT, 0);
    output_valid_ = false;
  }

  AccumulatorType getAccumulator() const {
    // Read accumulator from PE MAC accumulator
    return static_cast<AccumulatorType>(getMACAccumulator());
  }

  bool isOutputValid() const { return output_valid_; }

  int getRow() const { return row_; }
  int getCol() const { return col_; }

 private:
  int row_;
  int col_;
  bool output_valid_;
};

class SystolicArrayTPUBase : public Architecture::TickingComponent {
 public:
  SystolicArrayTPUBase(const std::string& name,
                       EventDriven::EventScheduler& scheduler, uint64_t period)
      : Architecture::TickingComponent(name, scheduler, period) {}

  ~SystolicArrayTPUBase() override = default;

  virtual size_t getArraySize() const = 0;
  virtual void resetAllMACs() = 0;
  virtual const char* getPrecisionName() const = 0;
  virtual EventDriven::EventScheduler& getScheduler() = 0;
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

  // Get timing information from components
  uint64_t getMACLatency() const {
    return MACUnit<PrecisionTraits>::MAC_LATENCY;
  }
  uint64_t getMemoryReadLatency() const {
    return LoadStoreUnit::MEMORY_READ_LATENCY;
  }
  uint64_t getMemoryWriteLatency() const {
    return LoadStoreUnit::MEMORY_WRITE_LATENCY;
  }

  // Port-based memory access with buffering for timing simulation
  void writeMemory(uint32_t address, ValueType data) {
    auto req = std::make_shared<MemoryRequestPacket>(LSUOp::STORE, address,
                                                     Traits::encode(data));

    auto req_port = lsu_->getPort("req_in");
    auto valid_port = lsu_->getPort("valid");
    auto ready_port = lsu_->getPort("ready");
    auto resp_port = lsu_->getPort("resp_out");

    // Trace memory write
    EventDriven::Tracer::getInstance().traceMemoryWrite(
        scheduler_.getCurrentTime(), getName(), address,
        static_cast<int>(data));

    // Clear any stale responses
    while (resp_port->hasData()) {
      resp_port->read();
    }

    // Wait for LSU to be ready
    for (int wait = 0; wait < 100; ++wait) {
      auto ready_data = ready_port->read();
      auto ready_int = std::dynamic_pointer_cast<Architecture::IntDataPacket>(ready_data);
      if (ready_int && ready_int->getValue() == 1) {
        break;
      }
      lsu_->tick();
    }

    valid_port->write(std::make_shared<Architecture::IntDataPacket>(1));
    req_port->write(req);
    lsu_->tick();  // Enqueue the request
    valid_port->write(std::make_shared<Architecture::IntDataPacket>(0));

    // Process until write completes
    for (int i = 0; i < 50; ++i) {
      lsu_->tick();
    }
  }

  ValueType readMemory(uint32_t address) {
    auto req = std::make_shared<MemoryRequestPacket>(LSUOp::LOAD, address);

    auto req_port = lsu_->getPort("req_in");
    auto valid_port = lsu_->getPort("valid");
    auto resp_port = lsu_->getPort("resp_out");
    auto ready_port = lsu_->getPort("ready");

    // Clear any stale responses
    while (resp_port->hasData()) {
      resp_port->read();
    }

    // Wait for LSU to be ready
    for (int wait = 0; wait < 100; ++wait) {
      auto ready_data = ready_port->read();
      auto ready_int = std::dynamic_pointer_cast<Architecture::IntDataPacket>(ready_data);
      if (ready_int && ready_int->getValue() == 1) {
        break;
      }
      lsu_->tick();
    }

    valid_port->write(std::make_shared<Architecture::IntDataPacket>(1));
    req_port->write(req);
    lsu_->tick();  // Enqueue the request
    valid_port->write(std::make_shared<Architecture::IntDataPacket>(0));

    // Process until response is available
    ValueType result = Traits::zeroValue();
    for (int i = 0; i < 50; ++i) {
      lsu_->tick();
      if (resp_port->hasData()) {
        auto resp_data = resp_port->read();
        auto resp = std::dynamic_pointer_cast<MemoryResponsePacket>(resp_data);
        if (resp && resp->getAddress() == address) {
          result = Traits::decode(resp->getData());

          // Trace memory read
          EventDriven::Tracer::getInstance().traceMemoryRead(
              scheduler_.getCurrentTime(), getName(), address,
              static_cast<int>(result));

          return result;
        }
      }
    }

    // Timeout - trace the error
    EventDriven::Tracer::getInstance().trace(
        scheduler_.getCurrentTime(), EventDriven::TraceEventType::MEMORY_READ,
        getName(), "memory_read_timeout",
        "addr=0x" + std::to_string(address) + " TIMEOUT");

    return result;
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

  const char* getPrecisionName() const override { return Traits::name(); }

  EventDriven::EventScheduler& getScheduler() override { return scheduler_; }

 private:
  size_t array_size_;
  std::vector<std::vector<std::shared_ptr<MACUnit<PrecisionTraits>>>>
      mac_array_;
  std::shared_ptr<LoadStoreUnit> lsu_;
};

#endif  // TPU_H
