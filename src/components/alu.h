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

/**
 * @brief Precision traits for different data types
 *
 * Defines the data type, pipeline stages, and type name for precision
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

/**
 * @brief ALU operation types
 *
 * Supports RV32I base instruction set and ZBB bit manipulation extension
 */
enum class ALUOp {
  // RV32I Base Integer Instruction Set
  ADD,   // Addition: rd = rs1 + rs2
  SUB,   // Subtraction: rd = rs1 - rs2
  SLT,   // Set less than (signed): rd = (rs1 < rs2) ? 1 : 0
  SLTU,  // Set less than unsigned: rd = (rs1 <u rs2) ? 1 : 0
  XOR,   // Bitwise XOR: rd = rs1 ^ rs2
  OR,    // Bitwise OR: rd = rs1 | rs2
  AND,   // Bitwise AND: rd = rs1 & rs2
  SLL,   // Shift left logical: rd = rs1 << rs2[4:0]
  SRL,   // Shift right logical: rd = rs1 >> rs2[4:0]
  SRA,   // Shift right arithmetic: rd = rs1 >> rs2[4:0] (sign-ext)
  LUI,   // Load upper immediate: rd = rs2 (rs1 is ignored)

  // RV32M Multiply Extension
  MUL,  // Multiplication: rd = (rs1 * rs2)[31:0]
  DIV,  // Division (signed): rd = rs1 / rs2

  // ZBB Bit Manipulation Extension
  ANDN,   // AND with NOT: rd = rs1 & ~rs2
  ORN,    // OR with NOT: rd = rs1 | ~rs2
  XNOR,   // XNOR: rd = ~(rs1 ^ rs2)
  CLZ,    // Count leading zeros: rd = clz(rs1)
  CTZ,    // Count trailing zeros: rd = ctz(rs1)
  CPOP,   // Count population: rd = popcount(rs1)
  MAX,    // Maximum (signed): rd = (rs1 > rs2) ? rs1 : rs2
  MAXU,   // Maximum unsigned: rd = (rs1 >u rs2) ? rs1 : rs2
  MIN,    // Minimum (signed): rd = (rs1 < rs2) ? rs1 : rs2
  MINU,   // Minimum unsigned: rd = (rs1 <u rs2) ? rs1 : rs2
  SEXTB,  // Sign extend byte: rd = sign_ext(rs1[7:0])
  SEXTH,  // Sign extend half-word: rd = sign_ext(rs1[15:0])
  ROL,    // Rotate left: rd = rs1 rotl rs2[4:0]
  ROR,    // Rotate right: rd = rs1 rotr rs2[4:0]
  ORCB,   // OR combine bytes: rd = orcb(rs1)
  REV8,   // Reverse bytes: rd = reverse_bytes(rs1)
  ZEXTH,  // Zero extend half-word: rd = rs1[15:0]

  // Extra operations for compatibility
  MAC,     // Multiply-Accumulate: accumulator = accumulator + (rs1 * rs2)
  PASS_A,  // Pass operand A (for pipeline testing)
  PASS_B   // Pass operand B (for pipeline testing)
};

/**
 * @brief ALU Command
 *
 * Contains the ALU operation and destination register address
 * Similar to Coral NPU's AluCmd structure
 */
class ALUCmd {
 public:
  ALUCmd(uint32_t addr = 0, ALUOp op = ALUOp::ADD) : addr_(addr), op_(op) {}

  uint32_t getAddr() const { return addr_; }
  ALUOp getOp() const { return op_; }

  void setAddr(uint32_t addr) { addr_ = addr; }
  void setOp(ALUOp op) { op_ = op; }

 private:
  uint32_t addr_;  // Destination register address (5 bits in RISC-V)
  ALUOp op_;       // Operation code
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
        result_packet = std::make_shared<Architecture::IntDataPacket>(
            static_cast<int>(result));
      } else {
        // For Float32, we still use IntDataPacket but with float bits
        // In a real implementation, you'd want a FloatDataPacket
        int float_as_int;
        std::memcpy(&float_as_int, &result, sizeof(float));
        result_packet =
            std::make_shared<Architecture::IntDataPacket>(float_as_int);
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
    }
    return DataType{};
  }

 private:
  static DataType executeInt32Operation(DataType a, DataType b, ALUOp op) {
    switch (op) {
      // RV32I Base Instructions
      case ALUOp::ADD:
        return a + b;
      case ALUOp::SUB:
        return a - b;
      case ALUOp::SLT:
        return (a < b) ? 1 : 0;
      case ALUOp::SLTU:
        return (static_cast<uint32_t>(a) < static_cast<uint32_t>(b)) ? 1 : 0;
      case ALUOp::XOR:
        return a ^ b;
      case ALUOp::OR:
        return a | b;
      case ALUOp::AND:
        return a & b;
      case ALUOp::SLL:
        return a << (b & 0x1F);  // Mask to 5 bits (shift amount)
      case ALUOp::SRL:
        return static_cast<uint32_t>(a) >> (b & 0x1F);  // Logical shift
      case ALUOp::SRA:
        return a >> (b & 0x1F);  // Arithmetic shift (sign-extended)
      case ALUOp::LUI:
        return b;  // Load upper immediate: just use rs2

      // RV32M Multiply Extension
      case ALUOp::MUL:
        return a * b;
      case ALUOp::DIV:
        return (b != 0) ? a / b : 0;  // Avoid division by zero

      // ZBB Bit Manipulation Extension
      case ALUOp::ANDN:
        return a & ~b;  // AND with NOT
      case ALUOp::ORN:
        return a | ~b;  // OR with NOT
      case ALUOp::XNOR:
        return ~(a ^ b);  // XNOR
      case ALUOp::CLZ:
        return __builtin_clz(static_cast<uint32_t>(a));  // Count leading zeros
      case ALUOp::CTZ:
        return __builtin_ctz(static_cast<uint32_t>(a));  // Count trailing zeros
      case ALUOp::CPOP:
        return __builtin_popcount(
            static_cast<uint32_t>(a));  // Population count
      case ALUOp::MAX:
        return (a > b) ? a : b;
      case ALUOp::MAXU:
        return (static_cast<uint32_t>(a) > static_cast<uint32_t>(b)) ? a : b;
      case ALUOp::MIN:
        return (a < b) ? a : b;
      case ALUOp::MINU:
        return (static_cast<uint32_t>(a) < static_cast<uint32_t>(b)) ? a : b;
      case ALUOp::SEXTB:
        // Sign extend byte (8-bit to 32-bit)
        return static_cast<int8_t>(a & 0xFF);
      case ALUOp::SEXTH:
        // Sign extend half-word (16-bit to 32-bit)
        return static_cast<int16_t>(a & 0xFFFF);
      case ALUOp::ROL:
        // Rotate left
        return (a << (b & 0x1F)) |
               (static_cast<uint32_t>(a) >> ((32 - (b & 0x1F)) & 0x1F));
      case ALUOp::ROR:
        // Rotate right
        return (static_cast<uint32_t>(a) >> (b & 0x1F)) |
               (a << ((32 - (b & 0x1F)) & 0x1F));
      case ALUOp::ORCB:
        // OR combine bytes: each byte becomes 0x00 or 0xFF
        {
          uint32_t ua = static_cast<uint32_t>(a);
          uint32_t result = 0;
          for (int i = 0; i < 4; i++) {
            uint8_t byte = (ua >> (i * 8)) & 0xFF;
            if (byte != 0) result |= (0xFF << (i * 8));
          }
          return static_cast<int32_t>(result);
        }
      case ALUOp::REV8:
        // Reverse bytes in 32-bit word
        {
          uint32_t ua = static_cast<uint32_t>(a);
          return static_cast<int32_t>(
              ((ua & 0xFF000000) >> 24) | ((ua & 0x00FF0000) >> 8) |
              ((ua & 0x0000FF00) << 8) | ((ua & 0x000000FF) << 24));
        }
      case ALUOp::ZEXTH:
        // Zero extend half-word (keep only lower 16 bits)
        return a & 0xFFFF;

      // Extra operations
      case ALUOp::MAC:
        // MAC without accumulator context, just multiply
        return a * b;
      case ALUOp::PASS_A:
        return a;
      case ALUOp::PASS_B:
        return b;
      default:
        return 0;
    }
  }

 public:
  /**
   * @brief Get operation name as string
   */
  static std::string getOpName(ALUOp op) {
    switch (op) {
      // RV32I Base Instructions
      case ALUOp::ADD:
        return "ADD";
      case ALUOp::SUB:
        return "SUB";
      case ALUOp::SLT:
        return "SLT";
      case ALUOp::SLTU:
        return "SLTU";
      case ALUOp::XOR:
        return "XOR";
      case ALUOp::OR:
        return "OR";
      case ALUOp::AND:
        return "AND";
      case ALUOp::SLL:
        return "SLL";
      case ALUOp::SRL:
        return "SRL";
      case ALUOp::SRA:
        return "SRA";
      case ALUOp::LUI:
        return "LUI";

      // RV32M Multiply
      case ALUOp::MUL:
        return "MUL";
      case ALUOp::DIV:
        return "DIV";

      // ZBB Bit Manipulation
      case ALUOp::ANDN:
        return "ANDN";
      case ALUOp::ORN:
        return "ORN";
      case ALUOp::XNOR:
        return "XNOR";
      case ALUOp::CLZ:
        return "CLZ";
      case ALUOp::CTZ:
        return "CTZ";
      case ALUOp::CPOP:
        return "CPOP";
      case ALUOp::MAX:
        return "MAX";
      case ALUOp::MAXU:
        return "MAXU";
      case ALUOp::MIN:
        return "MIN";
      case ALUOp::MINU:
        return "MINU";
      case ALUOp::SEXTB:
        return "SEXTB";
      case ALUOp::SEXTH:
        return "SEXTH";
      case ALUOp::ROL:
        return "ROL";
      case ALUOp::ROR:
        return "ROR";
      case ALUOp::ORCB:
        return "ORCB";
      case ALUOp::REV8:
        return "REV8";
      case ALUOp::ZEXTH:
        return "ZEXTH";

      // Extra
      case ALUOp::MAC:
        return "MAC";
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
      case ALUOp::ANDN:
        return "&~";
      case ALUOp::ORN:
        return "|~";
      case ALUOp::XNOR:
        return "~^";
      case ALUOp::SLL:
        return "<<";
      case ALUOp::SRL:
        return ">>>";
      case ALUOp::SRA:
        return ">>";
      case ALUOp::ROL:
        return "<<<";
      case ALUOp::ROR:
        return ">>>";
      case ALUOp::SLT:
        return "<";
      case ALUOp::SLTU:
        return "<u";
      case ALUOp::MAX:
        return "max";
      case ALUOp::MAXU:
        return "maxu";
      case ALUOp::MIN:
        return "min";
      case ALUOp::MINU:
        return "minu";
      case ALUOp::CLZ:
        return "clz";
      case ALUOp::CTZ:
        return "ctz";
      case ALUOp::CPOP:
        return "popcount";
      case ALUOp::SEXTB:
        return "sext.b";
      case ALUOp::SEXTH:
        return "sext.h";
      case ALUOp::ZEXTH:
        return "zext.h";
      case ALUOp::REV8:
        return "rev8";
      case ALUOp::ORCB:
        return "orcb";
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

// Type alias for integer ALU
using INTUComponent = ALUComponent<Int32Precision>;

#endif  // ALU_H
