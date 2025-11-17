#ifndef CORE_DECODE_H
#define CORE_DECODE_H

#include <cstdint>
#include <memory>

#include "../../packet.h"
#include "alu.h"
#include "bru.h"
#include "lsu.h"
#include "mlu.h"

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
    VECTOR,  // Vector extension (RVV)
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

  /**
   * @brief Check if this instruction is a control flow instruction
   * @return true if instruction is BRU type (branch, jump, etc.)
   */
  bool isBranchInstruction() const { return op_type == OpType::BRU; }

  /**
   * @brief Check if this instruction is a conditional branch
   * @return true if instruction is conditional branch (BEQ, BNE, BLT, BGE,
   * BLTU, BGEU)
   */
  bool isConditionalBranch() const {
    if (op_type != OpType::BRU) return false;
    // BEQ, BNE, BLT, BGE, BLTU, BGEU are conditional (opcode 0x63)
    // JAL and JALR are unconditional
    uint32_t opcode_bits = word & 0x7F;
    return opcode_bits == 0x63;  // Conditional branch instruction
  }

  /**
   * @brief Check if this instruction is an unconditional jump
   * @return true if instruction is JAL or JALR
   */
  bool isUnconditionalJump() const {
    if (op_type != OpType::BRU) return false;
    uint32_t opcode_bits = word & 0x7F;
    return opcode_bits == 0x6F || opcode_bits == 0x67;  // JAL or JALR
  }
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

    // Extract common fields (RV32I format)
    uint32_t opcode_bits = word & 0x7F;
    inst.rd = (word >> 7) & 0x1F;
    inst.rs1 = (word >> 15) & 0x1F;
    inst.opcode = (word >> 25) & 0x7F;

    // For I-type instructions, bits [31:20] is the immediate
    // For R-type instructions, bits [31:25] is funct7, bits [24:20] is rs2
    // We'll set rs2 correctly based on instruction type
    inst.rs2 = 0;                     // Default to 0, override for R-type below
    inst.imm = (word >> 20) & 0xFFF;  // I-type immediate

    // Classify instruction type based on opcode
    switch (opcode_bits) {
      // ALU immediate instructions
      case 0x13:  // ADDI, SLTI, XORI, ORI, ANDI, SLLI, SRLI, SRAI
        inst.op_type = DecodedInstruction::OpType::ALU;
        inst.opcode = static_cast<uint32_t>(ALUOp::ADD);
        // rs2 stays 0 for I-type
        break;

      // ALU register instructions and MLU operations (both opcode 0x33)
      case 0x33: {                       // ADD, SUB, MUL, MULH, etc.
        inst.rs2 = (word >> 20) & 0x1F;  // R-type: extract rs2
        uint32_t funct7 = (word >> 25) & 0x7F;

        if (funct7 == 0x01) {
          // MUL operations (funct7=0x01)
          inst.op_type = DecodedInstruction::OpType::MLU;
          inst.opcode = static_cast<uint32_t>(MultiplyUnit::MulOp::MUL);
        } else {
          // ALU operations (funct7=0x00)
          inst.op_type = DecodedInstruction::OpType::ALU;
          inst.opcode = static_cast<uint32_t>(ALUOp::ADD);
        }
        break;
      }

      // Branch instructions
      case 0x63:  // BEQ, BNE, BLT, BGE, BLTU, BGEU
        inst.op_type = DecodedInstruction::OpType::BRU;
        inst.opcode = static_cast<uint32_t>(BruOp::BEQ);
        inst.rs2 = (word >> 20) & 0x1F;  // B-type: extract rs2
        break;

      // JAL
      case 0x6F:
        inst.op_type = DecodedInstruction::OpType::BRU;
        inst.opcode = static_cast<uint32_t>(BruOp::JAL);
        // rs2 stays 0 for J-type
        break;

      // JALR
      case 0x67:
        inst.op_type = DecodedInstruction::OpType::BRU;
        inst.opcode = static_cast<uint32_t>(BruOp::JALR);
        // rs2 stays 0 for I-type
        break;

      // Load instructions
      case 0x03:  // LB, LH, LW, LBU, LHU
        inst.op_type = DecodedInstruction::OpType::LSU;
        inst.opcode = static_cast<uint32_t>(LSUOp::LOAD);
        // rs2 stays 0 for I-type
        break;

      // Store instructions
      case 0x23:  // SB, SH, SW
        inst.op_type = DecodedInstruction::OpType::LSU;
        inst.opcode = static_cast<uint32_t>(LSUOp::STORE);
        inst.rs2 = (word >> 20) & 0x1F;  // S-type: extract rs2
        break;

      // CSR instructions
      case 0x73:  // CSRRW, CSRRS, CSRRC, EBREAK, ECALL
        inst.op_type = DecodedInstruction::OpType::CSR;
        // rs2 stays 0
        break;

      // FENCE
      case 0x0F:
        inst.op_type = DecodedInstruction::OpType::FENCE;
        // rs2 stays 0
        break;

      // Vector instructions (RVV extension)
      // VADD, VSUB, VMUL, VAND, VOR, VXOR, etc.
      case 0x57:  // VV format (vector-vector operations)
      case 0x77:  // VI format (vector-immediate operations)
      case 0x37:  // VL format (vector load operations)
      case 0x27:  // VS format (vector store operations)
        inst.op_type = DecodedInstruction::OpType::VECTOR;
        inst.rs2 = (word >> 20) & 0x1F;  // Extract vs2
        // opcode stays as extracted from bits [31:25]
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
