#ifndef ALU_H
#define ALU_H

#include <iostream>
#include <memory>
#include <string>

#include "../port.h"
#include "../tick_component.h"
#include "int_packet.h"
#include "pipeline.h"

/**
 * @brief ALU operation types
 */
enum class ALUOp {
  ADD,     // Addition
  SUB,     // Subtraction
  MUL,     // Multiplication
  DIV,     // Division
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
 * @brief ALU Data Packet
 *
 * Contains two operands and operation code
 */
class ALUDataPacket : public Architecture::DataPacket {
 public:
  ALUDataPacket(int operand_a, int operand_b, ALUOp op)
      : operand_a_(operand_a), operand_b_(operand_b), op_(op) {}

  int getOperandA() const { return operand_a_; }
  int getOperandB() const { return operand_b_; }
  ALUOp getOperation() const { return op_; }

  void setOperandA(int value) { operand_a_ = value; }
  void setOperandB(int value) { operand_b_ = value; }
  void setOperation(ALUOp op) { op_ = op; }

  std::shared_ptr<Architecture::DataPacket> clone() const override {
    auto cloned = std::make_shared<ALUDataPacket>(operand_a_, operand_b_, op_);
    cloned->setTimestamp(timestamp_);
    cloned->setValid(valid_);
    return cloned;
  }

 private:
  int operand_a_;
  int operand_b_;
  ALUOp op_;
};

/**
 * @brief ALU Component
 *
 * Arithmetic Logic Unit implemented as a pipeline
 * Stages:
 *   Stage 0: Decode operation
 *   Stage 1: Execute operation
 *   Stage 2: Write back result
 */
class ALUComponent : public PipelineComponent {
 public:
  ALUComponent(const std::string& name, EventDriven::EventScheduler& scheduler,
               uint64_t period)
      : PipelineComponent(name, scheduler, period, 3), operations_executed_(0) {
    // Stage 0: Decode - just pass through (could add operation validation)
    setStageFunction(0, [this](std::shared_ptr<Architecture::DataPacket> data) {
      auto alu_data = std::dynamic_pointer_cast<ALUDataPacket>(data);
      if (alu_data && verbose_) {
        std::cout << "[" << scheduler_.getCurrentTime() << "] " << getName()
                  << " [Decode]: Op=" << getOpName(alu_data->getOperation())
                  << ", A=" << alu_data->getOperandA()
                  << ", B=" << alu_data->getOperandB() << std::endl;
      }
      return data;
    });

    // Stage 1: Execute - perform the actual operation
    setStageFunction(1, [this](std::shared_ptr<Architecture::DataPacket> data) {
      auto alu_data = std::dynamic_pointer_cast<ALUDataPacket>(data);
      if (alu_data) {
        int a = alu_data->getOperandA();
        int b = alu_data->getOperandB();
        int result = executeOperation(a, b, alu_data->getOperation());

        operations_executed_++;

        if (verbose_) {
          std::cout << "[" << scheduler_.getCurrentTime() << "] " << getName()
                    << " [Execute]: " << a << " "
                    << getOpSymbol(alu_data->getOperation()) << " " << b
                    << " = " << result << std::endl;
        }

        // Convert result to IntDataPacket for output
        auto result_packet = std::make_shared<IntDataPacket>(result);
        result_packet->setTimestamp(scheduler_.getCurrentTime());
        return std::static_pointer_cast<Architecture::DataPacket>(
            result_packet);
      }
      return data;
    });

    // Stage 2: Write back - just pass through (could add result validation)
    setStageFunction(2, [this](std::shared_ptr<Architecture::DataPacket> data) {
      auto int_data = std::dynamic_pointer_cast<IntDataPacket>(data);
      if (int_data && verbose_) {
        std::cout << "[" << scheduler_.getCurrentTime() << "] " << getName()
                  << " [WriteBack]: Result=" << int_data->getValue()
                  << std::endl;
      }
      return data;
    });
  }

  /**
   * @brief Execute ALU operation
   */
  static int executeOperation(int a, int b, ALUOp op) {
    switch (op) {
      case ALUOp::ADD:
        return a + b;
      case ALUOp::SUB:
        return a - b;
      case ALUOp::MUL:
        return a * b;
      case ALUOp::DIV:
        return (b != 0) ? a / b : 0;  // Avoid division by zero
      case ALUOp::AND:
        return a & b;
      case ALUOp::OR:
        return a | b;
      case ALUOp::XOR:
        return a ^ b;
      case ALUOp::SLL:
        return a << (b & 0x1F);  // Mask to 5 bits
      case ALUOp::SRL:
        return static_cast<unsigned int>(a) >> (b & 0x1F);
      case ALUOp::SRA:
        return a >> (b & 0x1F);
      case ALUOp::SLT:
        return (a < b) ? 1 : 0;
      case ALUOp::SLTU:
        return (static_cast<unsigned int>(a) < static_cast<unsigned int>(b))
                   ? 1
                   : 0;
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

  void printStatistics() const {
    PipelineComponent::printStatistics();
    std::cout << "Operations executed: " << operations_executed_ << std::endl;
  }

 private:
  uint64_t operations_executed_;
};

#endif  // ALU_H
