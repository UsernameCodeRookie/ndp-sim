#ifndef RVV_ALU_H
#define RVV_ALU_H

#include <bitset>
#include <cstring>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "../packet.h"
#include "../port.h"
#include "../tick.h"
#include "../trace.h"
#include "pipeline.h"

namespace Architecture {

/**
 * @brief RVV Vector ALU result packet
 *
 * Contains vector operation result and metadata
 */
class RVVALUResultPacket : public DataPacket {
 public:
  RVVALUResultPacket(uint32_t rd = 0, uint32_t eew = 32, uint32_t vlen = 128)
      : rd(rd), eew(eew), vlen(vlen), result_data(vlen / 8, 0) {}

  std::shared_ptr<DataPacket> clone() const override {
    return cloneWithVectors<RVVALUResultPacket>(
        [this](RVVALUResultPacket* p) { p->result_data = result_data; }, rd,
        eew, vlen);
  }

  uint32_t rd;                       // Destination vector register
  uint32_t eew;                      // Element width in bits (8/16/32/64)
  uint32_t vlen;                     // Vector length in bits
  std::vector<uint8_t> result_data;  // Result data (8-byte aligned)
};

}  // namespace Architecture

/**
 * @brief RVV Instruction Categories for functional modeling
 *
 * Categorizes RISC-V Vector instructions by operation type
 */
enum class RVVCategory {
  // Arithmetic operations (1-2 cycle latency)
  ARITHMETIC,  // VADD, VSUB, VMUL, VMADD, etc.

  // Shift/Rotate operations (1-2 cycle latency)
  SHIFT,  // VSLL, VSRL, VSRA, etc.

  // Logical operations (1 cycle latency)
  LOGICAL,  // VAND, VOR, VXOR, VNOT, etc.

  // Mask operations (1-2 cycle latency)
  MASK,  // VMAND, VMOR, VMXOR, VMSOF, VMSIF, VMSBF, etc.

  // Bit manipulation (1-2 cycle latency)
  BITMANIP,  // VCPOP, VFIRST, VIOTA, etc.

  // Comparison operations (1 cycle latency)
  COMPARE,  // VMSEQ, VMSNE, VMSLT, VMSLTU, etc.

  // Memory operations (3-4 cycle latency)
  MEMORY,  // VL, VS operations

  // Floating point operations (4-6 cycle latency)
  FLOAT,  // VFADD, VFSUB, VFMUL, VFDIV, etc.

  // Unknown/NOP
  UNKNOWN
};

/**
 * @brief RVV ALU Data Packet
 *
 * Contains vector operands and operation metadata
 */
class RVVALUDataPacket : public Architecture::DataPacket {
 public:
  RVVALUDataPacket(uint32_t rd = 0, uint32_t rs1 = 0, uint32_t rs2 = 0,
                   uint32_t eew = 32, uint32_t vlen = 128,
                   RVVCategory category = RVVCategory::ARITHMETIC)
      : rd(rd),
        rs1(rs1),
        rs2(rs2),
        eew(eew),
        vlen(vlen),
        category(category),
        operand_a(vlen / 8, 0),
        operand_b(vlen / 8, 0) {}

  std::shared_ptr<Architecture::DataPacket> clone() const override {
    return cloneWithVectors<RVVALUDataPacket>(
        [this](RVVALUDataPacket* p) {
          p->operand_a = operand_a;
          p->operand_b = operand_b;
        },
        rd, rs1, rs2, eew, vlen, category);
  }

  uint32_t rd;                     // Destination vector register
  uint32_t rs1, rs2;               // Source vector registers
  uint32_t eew;                    // Element width in bits (8/16/32/64)
  uint32_t vlen;                   // Vector length in bits
  RVVCategory category;            // Operation category for latency modeling
  std::vector<uint8_t> operand_a;  // First operand data
  std::vector<uint8_t> operand_b;  // Second operand data
};

/**
 * @brief RVV Vector ALU - Functional model with latency parameters
 *
 * Models the CoralNPU RVV backend ALU with simplified functionality:
 * - 4 parallel execution units (2 for arithmetic/logic, 1 for shift, 1 for
 * mask)
 * - Variable latency based on operation category
 * - No detailed hardware timing, only functional behavior and latency
 *
 * Pipeline structure:
 * - Stage 0: Decode/dispatch
 * - Stage 1: Execution (variable based on category)
 * - Stage 2: Writeback
 */
class RVVVectorALU : public Pipeline {
 public:
  /**
   * @brief Constructor
   * @param name Component name
   * @param scheduler Event scheduler
   * @param period Clock period
   * @param num_units Number of parallel ALU units (default 4)
   * @param vlen Vector length in bits (default 128)
   */
  RVVVectorALU(const std::string& name, EventDriven::EventScheduler& scheduler,
               uint64_t period, size_t num_units = 4, uint32_t vlen = 128)
      : Pipeline(name, scheduler, period, 3),
        num_units_(num_units),
        vlen_(vlen),
        unit_busy_(num_units, false),
        operations_executed_(0) {
    setupPipelineStages();
  }

  /**
   * @brief Get latency for a specific operation category (in cycles)
   *
   * Based on CoralNPU RVV backend:
   * - Arithmetic: 1-2 cycles
   * - Shift: 1-2 cycles
   * - Logical: 1 cycle
   * - Mask: 1-2 cycles
   * - Bitmanip: 1-2 cycles
   * - Compare: 1 cycle
   * - Memory: 3-4 cycles (not handled in this ALU, for reference)
   * - Float: 4-6 cycles
   */
  static uint64_t getLatency(RVVCategory category) {
    switch (category) {
      case RVVCategory::ARITHMETIC:
        return 2;  // 2 cycles for VADD, VSUB, VMUL
      case RVVCategory::SHIFT:
        return 2;  // 2 cycles for VSLL, VSRL, VSRA
      case RVVCategory::LOGICAL:
        return 1;  // 1 cycle for VAND, VOR, VXOR
      case RVVCategory::MASK:
        return 2;  // 2 cycles for mask operations
      case RVVCategory::BITMANIP:
        return 2;  // 2 cycles for VCPOP, VIOTA
      case RVVCategory::COMPARE:
        return 1;  // 1 cycle for comparison
      case RVVCategory::MEMORY:
        return 4;  // 4 cycles (not in ALU)
      case RVVCategory::FLOAT:
        return 5;  // 5 cycles (not in ALU)
      default:
        return 1;
    }
  }

  /**
   * @brief Map RVV instruction opcode to operation category
   *
   * Decodes RISC-V Vector Extension opcodes to determine the operation
   * category, which is used to determine execution latency.
   *
   * Handles both full RISC-V encodings and simplified internal opcodes:
   * - Internal: 0x1=VADD, 0x5=VSUB, 0x13=VAND, 0x15=VOR, etc.
   * - Full RISC-V: opcode[6:0]=0x57/0x77 with funct6 fields
   *
   * @param opcode RVV instruction opcode
   * @return RVVCategory for the instruction
   */
  static RVVCategory getOpcodeCategory(uint32_t opcode) {
    // Check for simplified internal opcodes first
    switch (opcode) {
      // Arithmetic operations
      case 0x1:  // VADD
      case 0x5:  // VSUB
      case 0x9:  // VMUL
        return RVVCategory::ARITHMETIC;

      // Logical operations
      case 0x13:  // VAND
      case 0x15:  // VOR
      case 0x17:  // VXOR
        return RVVCategory::LOGICAL;

      // Shift operations
      case 0x21:  // VSLL
      case 0x25:  // VSRL
      case 0x27:  // VSRA
        return RVVCategory::SHIFT;

      default:
        break;  // Fall through to full RISC-V decoding
    }

    // RISC-V Vector Extension opcodes
    // Reference: https://riscv.org/technical/specifications/
    uint32_t base_opcode = opcode & 0x7F;

    // Vector opcode space (0x57, 0x77, 0x37, 0x27, etc.)
    if (base_opcode == 0x57 || base_opcode == 0x77 || base_opcode == 0x37 ||
        base_opcode == 0x27) {
      uint32_t funct6 = (opcode >> 26) & 0x3F;

      // Arithmetic operations (funct6 = 0, 2, 9)
      if (funct6 == 0x00 || funct6 == 0x02 || funct6 == 0x09) {
        return RVVCategory::ARITHMETIC;
      }

      // Logical operations (funct6 = 9, 10, 11, etc.)
      if (funct6 == 0x0A || funct6 == 0x0B) {
        return RVVCategory::LOGICAL;
      }

      // Shift operations (funct6 = 4, 5, 6)
      if (funct6 == 0x04 || funct6 == 0x05 || funct6 == 0x06) {
        return RVVCategory::SHIFT;
      }

      // Compare operations (funct6 = 24-31 range)
      if (funct6 >= 0x18 && funct6 <= 0x1F) {
        return RVVCategory::COMPARE;
      }
    }

    // Default: UNKNOWN
    return RVVCategory::UNKNOWN;
  }

  /**
   * @brief Get execution latency in cycles for a specific opcode
   *
   * @param opcode RVV instruction opcode
   * @return Latency in cycles
   */
  static uint64_t getOpcodeLatency(uint32_t opcode) {
    return getLatency(getOpcodeCategory(opcode));
  }
  size_t getAvailableUnits() const {
    size_t available = 0;
    for (size_t i = 0; i < num_units_; ++i) {
      if (!unit_busy_[i]) {
        available++;
      }
    }
    return available;
  }

  /**
   * @brief Mark ALU unit as busy for specified cycles
   */
  void markUnitBusy(size_t unit_index, uint64_t /* cycles */) {
    if (unit_index < num_units_) {
      unit_busy_[unit_index] = true;
    }
  }

  /**
   * @brief Mark ALU unit as free
   */
  void markUnitFree(size_t unit_index) {
    if (unit_index < num_units_) {
      unit_busy_[unit_index] = false;
    }
  }

  /**
   * @brief Get the total number of operations executed
   */
  uint64_t getOperationsExecuted() const { return operations_executed_; }

 private:
  size_t num_units_;  // Number of parallel execution units
  uint32_t vlen_;     // Vector length in bits

  std::vector<bool> unit_busy_;  // Track busy state of each unit
  uint64_t operations_executed_;

  /**
   * @brief Setup pipeline stages with functional operations
   *
   * Stage 0: Input - capture operands
   * Stage 1: Execute - perform vector operation
   * Stage 2: Output - prepare results
   */
  void setupPipelineStages() {
    // Stage 0: Decode stage - prepare operands
    setStageFunction(0, [this](std::shared_ptr<Architecture::DataPacket> data) {
      auto alu_data = std::dynamic_pointer_cast<RVVALUDataPacket>(data);
      if (!alu_data) return data;

      // In real implementation, read from register file here
      // For now, just pass through
      return data;
    });

    // Stage 1: Execute stage - perform the operation
    setStageFunction(1, [this](std::shared_ptr<Architecture::DataPacket> data) {
      auto alu_data = std::dynamic_pointer_cast<RVVALUDataPacket>(data);
      if (!alu_data) return data;

      // Execute vector operation based on category
      auto result = std::make_shared<Architecture::RVVALUResultPacket>(
          alu_data->rd, alu_data->eew, alu_data->vlen);

      executeVectorOperation(alu_data, result);
      operations_executed_++;

      // Return result as DataPacket
      return std::static_pointer_cast<Architecture::DataPacket>(result);
    });

    // Stage 2: Writeback stage - prepare for output
    setStageFunction(2, [this](std::shared_ptr<Architecture::DataPacket> data) {
      // Just pass through - result is ready
      return data;
    });
  }

  /**
   * @brief Execute vector operation based on category
   *
   * This is a simplified functional model that doesn't perform actual
   * vector computation, just demonstrates the functional structure
   */
  void executeVectorOperation(
      std::shared_ptr<RVVALUDataPacket> op,
      std::shared_ptr<Architecture::RVVALUResultPacket> result) {
    // Simplified execution - in real implementation, perform actual computation
    // For now, copy data to demonstrate structure
    uint32_t num_elements = op->vlen / op->eew;

    switch (op->category) {
      case RVVCategory::ARITHMETIC: {
        // VADD, VSUB, VMUL, VMADD
        for (uint32_t i = 0; i < num_elements && i < op->operand_a.size();
             ++i) {
          result->result_data[i] = op->operand_a[i] + op->operand_b[i];
        }
        break;
      }

      case RVVCategory::SHIFT: {
        // VSLL, VSRL, VSRA
        for (uint32_t i = 0; i < num_elements && i < op->operand_a.size();
             ++i) {
          result->result_data[i] = op->operand_a[i] << 1;
        }
        break;
      }

      case RVVCategory::LOGICAL: {
        // VAND, VOR, VXOR, VNOT
        for (uint32_t i = 0; i < num_elements && i < op->operand_a.size();
             ++i) {
          result->result_data[i] = op->operand_a[i] & op->operand_b[i];
        }
        break;
      }

      case RVVCategory::MASK: {
        // VMAND, VMOR, VMXOR, VMSOF, VMSIF, VMSBF
        for (uint32_t i = 0; i < num_elements && i < op->operand_a.size();
             ++i) {
          result->result_data[i] = op->operand_a[i] & op->operand_b[i];
        }
        break;
      }

      case RVVCategory::BITMANIP: {
        // VCPOP, VFIRST, VIOTA
        // Just copy for now
        result->result_data = op->operand_a;
        break;
      }

      case RVVCategory::COMPARE: {
        // VMSEQ, VMSNE, VMSLT, VMSLTU
        for (uint32_t i = 0; i < num_elements && i < op->operand_a.size();
             ++i) {
          result->result_data[i] =
              (op->operand_a[i] == op->operand_b[i]) ? 0xFF : 0x00;
        }
        break;
      }

      default: {
        // Copy input as result
        result->result_data = op->operand_a;
        break;
      }
    }

    // Trace execution
    // (Tracing would be done through the EventScheduler if needed)
  }
};

#endif  // RVV_ALU_H
