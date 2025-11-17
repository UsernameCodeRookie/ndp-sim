#ifndef ALU_H
#define ALU_H

#include <cstring>
#include <memory>
#include <string>

#include "../../packet.h"
#include "../../pipeline.h"
#include "../../port.h"
#include "../../tick.h"
#include "../../trace.h"

namespace Architecture {

/**
 * @brief ALU result packet with destination register info
 */
class ALUResultPacket : public DataPacket {
 public:
  ALUResultPacket(int val = 0, uint32_t rd = 0) : value(val), rd(rd) {}

  std::shared_ptr<DataPacket> clone() const override {
    return cloneImpl<ALUResultPacket>(value, rd);
  }

  int value;
  uint32_t rd;  // Destination register
};

}  // namespace Architecture

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
 * @brief ALU Data Packet for int32 operations
 *
 * Contains two int32 operands and operation code
 */
class ALUDataPacket : public Architecture::DataPacket {
 public:
  ALUDataPacket(int32_t a = 0, int32_t b = 0, ALUOp op = ALUOp::ADD,
                uint32_t rd = 0)
      : operand_a(a), operand_b(b), op(op), rd(rd) {}

  std::shared_ptr<Architecture::DataPacket> clone() const override {
    return cloneImpl<ALUDataPacket>(operand_a, operand_b, op, rd);
  }

  int32_t operand_a;
  int32_t operand_b;
  ALUOp op;
  uint32_t rd;  // Destination register
};

/**
 * @brief ALU Component - 3-stage pipelined integer arithmetic unit
 *
 * Supports RV32I base instruction set and ZBB bit manipulation extension.
 * Implements a 3-stage pipeline: Decode -> Execute -> Writeback
 */
class ArithmeticLogicUnit : public Pipeline {
 public:
  ArithmeticLogicUnit(const std::string& name,
                      EventDriven::EventScheduler& scheduler, uint64_t period)
      : Pipeline(name, scheduler, period,
                 3),  // Default latency=0 for direct tick() mode
        operations_executed_(0),
        accumulator_(0),
        last_rd_(0),
        last_value_(0) {
    // Create output ports for writeback
    addPort("rd_out", Architecture::PortDirection::OUTPUT);
    addPort("data_out", Architecture::PortDirection::OUTPUT);
    setupPipelineStages();
  }

  /**
   * @brief Execute ALU operation with accumulator support
   */
  static int32_t executeOperationWithAccumulator(int32_t a, int32_t b, ALUOp op,
                                                 int64_t& accumulator) {
    if (op == ALUOp::MAC) {
      // Multiply-Accumulate: accumulator = accumulator + (a * b)
      accumulator =
          accumulator + (static_cast<int64_t>(a) * static_cast<int64_t>(b));
      return static_cast<int32_t>(accumulator);
    }
    return executeOperation(a, b, op);
  }

  /**
   * @brief Execute ALU operation
   */
  static int32_t executeOperation(int32_t a, int32_t b, ALUOp op) {
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
        return a << (b & 0x1F);
      case ALUOp::SRL:
        return static_cast<int32_t>(static_cast<uint32_t>(a) >> (b & 0x1F));
      case ALUOp::SRA:
        return a >> (b & 0x1F);
      case ALUOp::LUI:
        return b;

      // RV32M Multiply Extension
      case ALUOp::MUL:
        return a * b;
      case ALUOp::DIV:
        return (b != 0) ? a / b : 0;

      // ZBB Bit Manipulation Extension
      case ALUOp::ANDN:
        return a & ~b;
      case ALUOp::ORN:
        return a | ~b;
      case ALUOp::XNOR:
        return ~(a ^ b);
      case ALUOp::CLZ:
        return __builtin_clz(static_cast<uint32_t>(a));
      case ALUOp::CTZ:
        return __builtin_ctz(static_cast<uint32_t>(a));
      case ALUOp::CPOP:
        return __builtin_popcount(static_cast<uint32_t>(a));
      case ALUOp::MAX:
        return (a > b) ? a : b;
      case ALUOp::MAXU:
        return (static_cast<uint32_t>(a) > static_cast<uint32_t>(b)) ? a : b;
      case ALUOp::MIN:
        return (a < b) ? a : b;
      case ALUOp::MINU:
        return (static_cast<uint32_t>(a) < static_cast<uint32_t>(b)) ? a : b;
      case ALUOp::SEXTB:
        return static_cast<int8_t>(a & 0xFF);
      case ALUOp::SEXTH:
        return static_cast<int16_t>(a & 0xFFFF);
      case ALUOp::ROL:
        return (a << (b & 0x1F)) |
               (static_cast<int32_t>(static_cast<uint32_t>(a) >>
                                     ((32 - (b & 0x1F)) & 0x1F)));
      case ALUOp::ROR:
        return static_cast<int32_t>(
            (static_cast<uint32_t>(a) >> (b & 0x1F)) |
            (static_cast<uint32_t>(a) << ((32 - (b & 0x1F)) & 0x1F)));
      case ALUOp::ORCB: {
        uint32_t ua = static_cast<uint32_t>(a);
        uint32_t result = 0;
        for (int i = 0; i < 4; i++) {
          uint8_t byte = (ua >> (i * 8)) & 0xFF;
          if (byte != 0) result |= (0xFF << (i * 8));
        }
        return static_cast<int32_t>(result);
      }
      case ALUOp::REV8: {
        uint32_t ua = static_cast<uint32_t>(a);
        return static_cast<int32_t>(
            ((ua & 0xFF000000) >> 24) | ((ua & 0x00FF0000) >> 8) |
            ((ua & 0x0000FF00) << 8) | ((ua & 0x000000FF) << 24));
      }
      case ALUOp::ZEXTH:
        return a & 0xFFFF;

      // Extra operations
      case ALUOp::MAC:
        return a * b;
      case ALUOp::PASS_A:
        return a;
      case ALUOp::PASS_B:
        return b;
      default:
        return 0;
    }
  }

  /**
   * @brief Get operation name as string
   */
  static std::string getOpName(ALUOp op) {
    switch (op) {
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
      case ALUOp::MUL:
        return "MUL";
      case ALUOp::DIV:
        return "DIV";
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
  int64_t getAccumulator() const { return accumulator_; }
  void resetAccumulator() { accumulator_ = 0; }
  void setAccumulator(int64_t value) { accumulator_ = value; }

  void printStatistics() const {
    Pipeline::printStatistics();
    std::cout << "Precision: INT32" << std::endl;
    std::cout << "Pipeline stages: 3" << std::endl;
    std::cout << "Operations executed: " << operations_executed_ << std::endl;
    std::cout << "Accumulator value: " << accumulator_ << std::endl;
  }

  /**
   * @brief Get the rd (register destination) output port
   * @return Shared pointer to the rd output port
   */
  std::shared_ptr<Architecture::Port> getRdPort() { return getPort("rd_out"); }

  /**
   * @brief Get the value output port
   * @return Shared pointer to the value output port
   */
  std::shared_ptr<Architecture::Port> getValuePort() {
    return getPort("data_out");
  }

 private:
  uint64_t operations_executed_;
  int64_t accumulator_;
  uint32_t last_rd_;     // Track last rd to avoid duplicate outputs
  uint32_t last_value_;  // Track last value to avoid duplicate outputs

  void setupPipelineStages() {
    // Stage 0: Decode - trace rd value and pass through
    setStageFunction(0, [this](std::shared_ptr<Architecture::DataPacket> data) {
      auto alu_data = std::dynamic_pointer_cast<ALUDataPacket>(data);
      if (alu_data) {
        TRACE_COMPUTE(scheduler_.getCurrentTime(), getName(),
                      "ALU_STAGE0_ENTRY",
                      "rd=" << alu_data->rd << " op1=" << alu_data->operand_a
                            << " op2=" << alu_data->operand_b);
      }
      return data;
    });

    // Stage 1: Execute - perform the actual operation
    setStageFunction(1, [this](std::shared_ptr<Architecture::DataPacket> data) {
      auto alu_data = std::dynamic_pointer_cast<ALUDataPacket>(data);
      if (alu_data) {
        int32_t a = alu_data->operand_a;
        int32_t b = alu_data->operand_b;
        int32_t result =
            executeOperationWithAccumulator(a, b, alu_data->op, accumulator_);

        operations_executed_++;

        TRACE_COMPUTE(scheduler_.getCurrentTime(), getName(), "ALU_STAGE1_EXEC",
                      "rd=" << alu_data->rd << " result=" << result);

        // Create result packet with rd information
        auto result_packet = std::make_shared<Architecture::ALUResultPacket>(
            result, alu_data->rd);
        result_packet->timestamp = scheduler_.getCurrentTime();
        return std::static_pointer_cast<Architecture::DataPacket>(
            result_packet);
      }
      return data;
    });

    // Stage 2: Write back - trace rd value and output to ports
    setStageFunction(2, [this](std::shared_ptr<Architecture::DataPacket> data) {
      auto alu_result =
          std::dynamic_pointer_cast<Architecture::ALUResultPacket>(data);
      if (alu_result) {
        TRACE_COMPUTE(
            scheduler_.getCurrentTime(), getName(), "ALU_STAGE2_OUT",
            "rd=" << alu_result->rd << " value=" << alu_result->value);

        // Output rd and value to ports for RegisterFileWire connection
        auto rd_port = getPort("rd_out");
        auto value_port = getPort("data_out");

        if (rd_port && value_port) {
          auto rd_packet = std::make_shared<Architecture::IntDataPacket>(
              static_cast<int64_t>(alu_result->rd));
          auto value_packet = std::make_shared<Architecture::IntDataPacket>(
              static_cast<int64_t>(alu_result->value));

          rd_port->setData(rd_packet);
          value_port->setData(value_packet);

          last_rd_ = alu_result->rd;
          last_value_ = alu_result->value;
        }
      }
      return data;
    });
  }
};

#endif  // ALU_H
