#ifndef CORE_DECODE_H
#define CORE_DECODE_H

#include <cstdint>
#include <memory>

#include "../../packet.h"
#include "../alu.h"
#include "../bru.h"
#include "../lsu.h"

namespace Architecture {

/**
 * @brief Simplified instruction representation for dispatch
 *
 * Contains decoded instruction fields needed for dispatch decisions
 */
struct DecodedInstruction {
  uint32_t addr;  // Program counter
  uint32_t word;  // Instruction word

  // Operation classification (simplified)
  enum class OpType {
    ALU,     // Arithmetic/Logic
    BRU,     // Branch
    MLU,     // Multiply
    DVU,     // Divide
    LSU,     // Load/Store
    CSR,     // Control/Status Register
    FENCE,   // Fence/barrier
    INVALID  // Invalid instruction
  };

  OpType op_type;
  uint32_t rd;   // Destination register
  uint32_t rs1;  // Source register 1
  uint32_t rs2;  // Source register 2
  uint32_t imm;  // Immediate value

  // Operation-specific code
  uint32_t opcode;

  DecodedInstruction()
      : addr(0),
        word(0),
        op_type(OpType::INVALID),
        rd(0),
        rs1(0),
        rs2(0),
        imm(0),
        opcode(0) {}
};

/**
 * @brief Simple decoder for instruction words
 *
 * Simplified version - only recognizes basic RV32I patterns
 */
class DecodeStage {
 public:
  static DecodedInstruction decode(uint32_t pc, uint32_t word) {
    DecodedInstruction inst;
    inst.addr = pc;
    inst.word = word;

    // Extract fields (RV32I format)
    uint32_t opcode_bits = word & 0x7F;
    inst.rd = (word >> 7) & 0x1F;
    inst.rs1 = (word >> 15) & 0x1F;
    inst.rs2 = (word >> 20) & 0x1F;
    inst.opcode = (word >> 25) & 0x7F;

    // Decode immediate fields
    inst.imm = (word >> 20) & 0xFFF;  // I-type immediate

    // Classify instruction type based on opcode
    switch (opcode_bits) {
      // ALU immediate instructions
      case 0x13:  // ADDI, SLTI, XORI, ORI, ANDI, SLLI, SRLI, SRAI
        inst.op_type = DecodedInstruction::OpType::ALU;
        inst.opcode = static_cast<uint32_t>(ALUOp::ADD);
        break;

      // ALU register instructions
      case 0x33:  // ADD, SUB, SLL, SLT, SLTU, XOR, SRL, SRA, OR, AND
        inst.op_type = DecodedInstruction::OpType::ALU;
        inst.opcode = static_cast<uint32_t>(ALUOp::ADD);
        break;

      // Branch instructions
      case 0x63:  // BEQ, BNE, BLT, BGE, BLTU, BGEU
        inst.op_type = DecodedInstruction::OpType::BRU;
        inst.opcode = static_cast<uint32_t>(BruOp::BEQ);
        break;

      // JAL
      case 0x6F:
        inst.op_type = DecodedInstruction::OpType::BRU;
        inst.opcode = static_cast<uint32_t>(BruOp::JAL);
        break;

      // JALR
      case 0x67:
        inst.op_type = DecodedInstruction::OpType::BRU;
        inst.opcode = static_cast<uint32_t>(BruOp::JALR);
        break;

      // Load instructions
      case 0x03:  // LB, LH, LW, LBU, LHU
        inst.op_type = DecodedInstruction::OpType::LSU;
        inst.opcode = static_cast<uint32_t>(LSUOp::LOAD);
        break;

      // Store instructions
      case 0x23:  // SB, SH, SW
        inst.op_type = DecodedInstruction::OpType::LSU;
        inst.opcode = static_cast<uint32_t>(LSUOp::STORE);
        break;

      // CSR instructions
      case 0x73:  // CSRRW, CSRRS, CSRRC, EBREAK, ECALL
        inst.op_type = DecodedInstruction::OpType::CSR;
        break;

      // FENCE
      case 0x0F:
        inst.op_type = DecodedInstruction::OpType::FENCE;
        break;

      default:
        inst.op_type = DecodedInstruction::OpType::INVALID;
        break;
    }

    return inst;
  }
};

}  // namespace Architecture

#endif  // CORE_DECODE_H
