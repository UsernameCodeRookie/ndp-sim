#ifndef ALU_H
#define ALU_H

#include <cmath>
#include <cstring>
#include <memory>
#include <string>
#include <type_traits>

#include "../packet.h"
#include "../port.h"
#include "../tick.h"
#include "../trace.h"
#include "pipeline.h"

/**
 * @brief Precision type tags
 */
struct Int32Precision {};
struct Float32Precision {};

/**
 * @brief Precision traits for different data types
 *
 * Defines the data type, pipeline stages, and type name for each precision
 */
template <typename PrecisionType>
struct PrecisionTraits;

// Specialization for Int32
template <>
struct PrecisionTraits<Int32Precision> {
  using DataType = int32_t;
  static constexpr size_t pipeline_stages = 3;  // Decode, Execute, Writeback
  static constexpr const char* name = "INT32";
};

// Specialization for Float32
template <>
struct PrecisionTraits<Float32Precision> {
  using DataType = float;
  static constexpr size_t pipeline_stages =
      5;  // Decode, Execute1, Execute2, Execute3, Writeback
  static constexpr const char* name = "FP32";
};

/**
 * @brief ALU operation types
 */
enum class ALUOp {
  ADD,     // Addition
  SUB,     // Subtraction
  MUL,     // Multiplication
  DIV,     // Division
  MAC,     // Multiply-Accumulate (a = a + b * c, uses accumulator)
  AND,     // Bitwise AND
  OR,      // Bitwise OR
  XOR,     // Bitwise XOR
  SLL,     // Shift left logical
  SRL,     // Shift right logical
  SRA,     // Shift right arithmetic
  SLT,     // Set less than
  SLTU,    // Set less than unsigned
  MAX,     // Maximum
  MIN,     // Minimum
  ABS,     // Absolute value
  NEG,     // Negation
  PASS_A,  // Pass operand A
  PASS_B   // Pass operand B
};

/**
 * @brief ALU Data Packet (template version)
 *
 * Contains two operands and operation code
 */
template <typename PrecisionType>
class ALUDataPacket : public Architecture::DataPacket {
 public:
  using DataType = typename PrecisionTraits<PrecisionType>::DataType;

  ALUDataPacket(DataType operand_a, DataType operand_b, ALUOp op)
      : operand_a_(operand_a), operand_b_(operand_b), op_(op) {}

  DataType getOperandA() const { return operand_a_; }
  DataType getOperandB() const { return operand_b_; }
  ALUOp getOperation() const { return op_; }

  void setOperandA(DataType value) { operand_a_ = value; }
  void setOperandB(DataType value) { operand_b_ = value; }
  void setOperation(ALUOp op) { op_ = op; }

  std::shared_ptr<Architecture::DataPacket> clone() const override {
    auto cloned = std::make_shared<ALUDataPacket<PrecisionType>>(
        operand_a_, operand_b_, op_);
    cloned->setTimestamp(timestamp_);
    cloned->setValid(valid_);
    return cloned;
  }

 private:
  DataType operand_a_;
  DataType operand_b_;
  ALUOp op_;
};

/**
 * @brief ALU Component with accumulator support (template version)
 *
 * Arithmetic Logic Unit implemented as a pipeline
 * Pipeline stages depend on precision type:
 *   Int32: 3 stages (Decode, Execute, Writeback)
 *   Float32: 5 stages (Decode, Execute1, Execute2, Execute3, Writeback)
 */
template <typename PrecisionType>
class ALUComponent : public PipelineComponent {
 public:
  using DataType = typename PrecisionTraits<PrecisionType>::DataType;
  static constexpr size_t pipeline_stages =
      PrecisionTraits<PrecisionType>::pipeline_stages;

  ALUComponent(const std::string& name, EventDriven::EventScheduler& scheduler,
               uint64_t period)
      : PipelineComponent(name, scheduler, period, pipeline_stages),
        operations_executed_(0),
        accumulator_(0) {
    setupPipelineStages();
  }

 private:
  void setupPipelineStages() {
    if constexpr (std::is_same_v<PrecisionType, Int32Precision>) {
      setupInt32Pipeline();
    } else if constexpr (std::is_same_v<PrecisionType, Float32Precision>) {
      setupFloat32Pipeline();
    }
  }

  void setupInt32Pipeline() {
    // Stage 0: Decode - just pass through
    setStageFunction(0, [this](std::shared_ptr<Architecture::DataPacket> data) {
      return data;
    });

    // Stage 1: Execute - perform the actual operation
    setStageFunction(1, [this](std::shared_ptr<Architecture::DataPacket> data) {
      return executeStage(data);
    });

    // Stage 2: Write back - just pass through
    setStageFunction(2, [this](std::shared_ptr<Architecture::DataPacket> data) {
      return data;
    });
  }

  void setupFloat32Pipeline() {
    // Stage 0: Decode - just pass through
    setStageFunction(0, [this](std::shared_ptr<Architecture::DataPacket> data) {
      return data;
    });

    // Stage 1-3: Execute stages (floating point operations take multiple
    // cycles)
    setStageFunction(1, [this](std::shared_ptr<Architecture::DataPacket> data) {
      return data;  // Pipeline stage 1
    });

    setStageFunction(2, [this](std::shared_ptr<Architecture::DataPacket> data) {
      return data;  // Pipeline stage 2
    });

    setStageFunction(3, [this](std::shared_ptr<Architecture::DataPacket> data) {
      return executeStage(data);  // Actual execution in stage 3
    });

    // Stage 4: Write back
    setStageFunction(4, [this](std::shared_ptr<Architecture::DataPacket> data) {
      return data;
    });
  }

  std::shared_ptr<Architecture::DataPacket> executeStage(
      std::shared_ptr<Architecture::DataPacket> data) {
    auto alu_data =
        std::dynamic_pointer_cast<ALUDataPacket<PrecisionType>>(data);
    if (alu_data) {
      DataType a = alu_data->getOperandA();
      DataType b = alu_data->getOperandB();
      DataType result =
          executeOperationWithAccumulator(a, b, alu_data->getOperation());

      operations_executed_++;

      TRACE_COMPUTE(scheduler_.getCurrentTime(), getName(),
                    getOpName(alu_data->getOperation()),
                    a << " " << getOpSymbol(alu_data->getOperation()) << " "
                      << b << " = " << result
                      << " | ops=" << operations_executed_
                      << " type=" << PrecisionTraits<PrecisionType>::name);

      // Create result packet based on precision type
      std::shared_ptr<Architecture::DataPacket> result_packet;
      if constexpr (std::is_same_v<PrecisionType, Int32Precision>) {
        result_packet =
            std::make_shared<IntDataPacket>(static_cast<int>(result));
      } else {
        // For Float32, we still use IntDataPacket but with float bits
        // In a real implementation, you'd want a FloatDataPacket
        int float_as_int;
        std::memcpy(&float_as_int, &result, sizeof(float));
        result_packet = std::make_shared<IntDataPacket>(float_as_int);
      }
      result_packet->setTimestamp(scheduler_.getCurrentTime());
      return result_packet;
    }
    return data;
  }

 public:
  /**
   * @brief Execute ALU operation with accumulator support
   */
  DataType executeOperationWithAccumulator(DataType a, DataType b, ALUOp op) {
    switch (op) {
      case ALUOp::MAC:
        // Multiply-Accumulate: accumulator = accumulator + (a * b)
        accumulator_ = accumulator_ + (a * b);
        return accumulator_;
      default:
        return executeOperation(a, b, op);
    }
  }

  /**
   * @brief Execute ALU operation
   */
  static DataType executeOperation(DataType a, DataType b, ALUOp op) {
    if constexpr (std::is_same_v<PrecisionType, Int32Precision>) {
      return executeInt32Operation(a, b, op);
    } else if constexpr (std::is_same_v<PrecisionType, Float32Precision>) {
      return executeFloat32Operation(a, b, op);
    }
    return DataType{};
  }

 private:
  static DataType executeInt32Operation(DataType a, DataType b, ALUOp op) {
    switch (op) {
      case ALUOp::ADD:
        return a + b;
      case ALUOp::SUB:
        return a - b;
      case ALUOp::MUL:
        return a * b;
      case ALUOp::DIV:
        return (b != 0) ? a / b : 0;  // Avoid division by zero
      case ALUOp::MAC:
        // MAC without accumulator context, just multiply
        return a * b;
      case ALUOp::AND:
        return a & b;
      case ALUOp::OR:
        return a | b;
      case ALUOp::XOR:
        return a ^ b;
      case ALUOp::SLL:
        return a << (b & 0x1F);  // Mask to 5 bits
      case ALUOp::SRL:
        return static_cast<uint32_t>(a) >> (b & 0x1F);
      case ALUOp::SRA:
        return a >> (b & 0x1F);
      case ALUOp::SLT:
        return (a < b) ? 1 : 0;
      case ALUOp::SLTU:
        return (static_cast<uint32_t>(a) < static_cast<uint32_t>(b)) ? 1 : 0;
      case ALUOp::MAX:
        return (a > b) ? a : b;
      case ALUOp::MIN:
        return (a < b) ? a : b;
      case ALUOp::ABS:
        return (a < 0) ? -a : a;
      case ALUOp::NEG:
        return -a;
      case ALUOp::PASS_A:
        return a;
      case ALUOp::PASS_B:
        return b;
      default:
        return 0;
    }
  }

  static DataType executeFloat32Operation(DataType a, DataType b, ALUOp op) {
    switch (op) {
      case ALUOp::ADD:
        return a + b;
      case ALUOp::SUB:
        return a - b;
      case ALUOp::MUL:
        return a * b;
      case ALUOp::DIV:
        return (b != 0.0f) ? a / b : 0.0f;  // Avoid division by zero
      case ALUOp::MAC:
        // MAC without accumulator context, just multiply
        return a * b;
      case ALUOp::MAX:
        return (a > b) ? a : b;
      case ALUOp::MIN:
        return (a < b) ? a : b;
      case ALUOp::ABS:
        return std::fabs(a);
      case ALUOp::NEG:
        return -a;
      case ALUOp::PASS_A:
        return a;
      case ALUOp::PASS_B:
        return b;
      // Unsupported operations for floating point
      case ALUOp::AND:
      case ALUOp::OR:
      case ALUOp::XOR:
      case ALUOp::SLL:
      case ALUOp::SRL:
      case ALUOp::SRA:
      case ALUOp::SLT:
      case ALUOp::SLTU:
      default:
        return 0.0f;
    }
  }

 public:
  /**
   * @brief Get operation name as string
   */
  static std::string getOpName(ALUOp op) {
    switch (op) {
      case ALUOp::ADD:
        return "ADD";
      case ALUOp::SUB:
        return "SUB";
      case ALUOp::MUL:
        return "MUL";
      case ALUOp::DIV:
        return "DIV";
      case ALUOp::MAC:
        return "MAC";
      case ALUOp::AND:
        return "AND";
      case ALUOp::OR:
        return "OR";
      case ALUOp::XOR:
        return "XOR";
      case ALUOp::SLL:
        return "SLL";
      case ALUOp::SRL:
        return "SRL";
      case ALUOp::SRA:
        return "SRA";
      case ALUOp::SLT:
        return "SLT";
      case ALUOp::SLTU:
        return "SLTU";
      case ALUOp::MAX:
        return "MAX";
      case ALUOp::MIN:
        return "MIN";
      case ALUOp::ABS:
        return "ABS";
      case ALUOp::NEG:
        return "NEG";
      case ALUOp::PASS_A:
        return "PASS_A";
      case ALUOp::PASS_B:
        return "PASS_B";
      default:
        return "UNKNOWN";
    }
  }

  /**
   * @brief Get operation symbol
   */
  static std::string getOpSymbol(ALUOp op) {
    switch (op) {
      case ALUOp::ADD:
        return "+";
      case ALUOp::SUB:
        return "-";
      case ALUOp::MUL:
        return "*";
      case ALUOp::DIV:
        return "/";
      case ALUOp::MAC:
        return "*+";
      case ALUOp::AND:
        return "&";
      case ALUOp::OR:
        return "|";
      case ALUOp::XOR:
        return "^";
      case ALUOp::SLL:
        return "<<";
      case ALUOp::SRL:
        return ">>>";
      case ALUOp::SRA:
        return ">>";
      case ALUOp::SLT:
        return "<";
      case ALUOp::MAX:
        return "max";
      case ALUOp::MIN:
        return "min";
      default:
        return "?";
    }
  }

  // Statistics
  uint64_t getOperationsExecuted() const { return operations_executed_; }

  // Accumulator access
  DataType getAccumulator() const { return accumulator_; }
  void resetAccumulator() { accumulator_ = DataType{}; }
  void setAccumulator(DataType value) { accumulator_ = value; }

  void printStatistics() const {
    PipelineComponent::printStatistics();
    std::cout << "Precision: " << PrecisionTraits<PrecisionType>::name
              << std::endl;
    std::cout << "Pipeline stages: " << pipeline_stages << std::endl;
    std::cout << "Operations executed: " << operations_executed_ << std::endl;
    std::cout << "Accumulator value: " << accumulator_ << std::endl;
  }

 private:
  uint64_t operations_executed_;
  DataType accumulator_;  // Internal accumulator for MAC operations
};

// Type aliases for specific ALU types
using INTUComponent = ALUComponent<Int32Precision>;
using FPUComponent = ALUComponent<Float32Precision>;

#endif  // ALU_H
