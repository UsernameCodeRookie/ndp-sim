#ifndef RVV_DECODER_H
#define RVV_DECODER_H

#include <cstdint>
#include <memory>
#include <vector>

#include "rvv_interface.h"

namespace Architecture {

/**
 * @brief RVV Micro-operation (uop)
 *
 * Represents a single micro-operation generated from a vector instruction
 * after stripmining expansion.
 *
 * For LMUL > 1, a single instruction expands into multiple uops.
 * Example: LMUL=4 with VLEN=256 means register groups of 4:
 *   v0,v1,v2,v3 form one group
 *   v4,v5,v6,v7 form another group
 *   etc.
 */
struct RVVMicroOp {
  uint32_t inst_id;    // Original instruction ID
  uint32_t uop_id;     // Within-instruction uop sequence number
  uint32_t opcode;     // RVV operation code
  uint32_t vd;         // Physical destination register (after LMUL mapping)
  uint32_t vs1;        // Physical source register 1
  uint32_t vs2;        // Physical source register 2
  uint32_t vm;         // Mask register (v0)
  uint32_t sew;        // Selected element width (0=8b, 1=16b, 2=32b, 3=64b)
  uint32_t lmul;       // LMUL encoding (0=1, 1=2, 2=4, 3=8)
  uint32_t vl;         // Vector length
  uint32_t vstart;     // Start element
  bool is_masked;      // Use vector mask
  uint32_t uop_count;  // Total uops for this instruction (for cleanup)

  RVVMicroOp()
      : inst_id(0),
        uop_id(0),
        opcode(0),
        vd(0),
        vs1(0),
        vs2(0),
        vm(0),
        sew(0),
        lmul(0),
        vl(0),
        vstart(0),
        is_masked(false),
        uop_count(1) {}
};

/**
 * @brief RVV Instruction Decoder
 *
 * Converts RISC-V vector instructions into micro-operations (uops)
 * implementing stripmining for LMUL > 1.
 *
 * Stripmining Algorithm:
 * =====================
 * For vector instructions with LMUL > 1, a single instruction logically
 * operates on multiple physical registers. To simplify hardware, we expand
 * the instruction into multiple uops, one per register group.
 *
 * Example: VADD.VV v0, v2, v4 with LMUL=4
 *   Creates 4 uops (assuming 32 registers):
 *     uop0: VADD v0,  v2,  v4   (indices 0,2,4)
 *     uop1: VADD v1,  v3,  v5   (indices 1,3,5)
 *     uop2: VADD (skip, out of regs)
 *     uop3: VADD (skip, out of regs)
 *
 * Register Mapping Rules (RISC-V Spec):
 * =====================================
 * - LMUL=1: Direct mapping (v0→0, v1→1, ..., v31→31)
 * - LMUL=2: Pairs, interleaved (v0,v1→{0,1}, v2,v3→{2,3}, ..., v30,v31)
 * - LMUL=4: Quads (v0-3→{0,1,2,3}, v4-7→{4,5,6,7}, ..., v28-31)
 * - LMUL=8: Octets (v0-7→{0,1,...,7}, v8-15→{8,9,...,15}, v16-23, v24-31)
 *
 * Algorithm converts logical register index to physical based on LMUL.
 */
class RVVDecoder {
 public:
  /**
   * @brief Decode vector instruction to micro-operations
   *
   * @param inst Instruction request from Scalar Core
   * @return Vector of uops representing this instruction
   */
  static std::vector<RVVMicroOp> decodeToUops(
      const RVVCoreInterface::InstructionRequest& inst) {
    std::vector<RVVMicroOp> uops;

    // Extract LMUL and VLEN from instruction
    uint32_t lmul_code = inst.lmul;      // 0=1, 1=2, 2=4, 3=8
    uint32_t lmul_val = 1 << lmul_code;  // Actual LMUL value
    uint32_t sew_code = inst.sew;        // 0=8b, 1=16b, 2=32b, 3=64b
    uint32_t vl = inst.vl;
    uint32_t vm = inst.vm;

    // For LMUL=1, generate single uop
    if (lmul_code == 0) {
      RVVMicroOp uop;
      uop.inst_id = inst.opcode;
      uop.uop_id = 0;
      uop.opcode = inst.opcode;
      uop.vd = inst.vd_idx;
      uop.vs1 = inst.vs1_idx;
      uop.vs2 = inst.vs2_idx;
      uop.vm = vm;
      uop.sew = sew_code;
      uop.lmul = lmul_code;
      uop.vl = vl;
      uop.vstart = 0;
      uop.is_masked = (vm != 0);
      uop.uop_count = 1;
      uops.push_back(uop);
      return uops;
    }

    // For LMUL > 1, expand into multiple uops
    // Each uop operates on one register group
    // Max 8 uops for LMUL=8
    for (uint32_t group = 0; group < lmul_val; group++) {
      // Map logical register indices to physical with LMUL expansion
      uint32_t phys_vd = mapLogicalToPhysical(inst.vd_idx, group, lmul_val);
      uint32_t phys_vs1 = mapLogicalToPhysical(inst.vs1_idx, group, lmul_val);
      uint32_t phys_vs2 = mapLogicalToPhysical(inst.vs2_idx, group, lmul_val);

      // Skip if mapped register would exceed 31 (only 32 vector registers)
      if (phys_vd > 31 || phys_vs1 > 31 || phys_vs2 > 31) {
        continue;  // Skip invalid register mapping
      }

      RVVMicroOp uop;
      uop.inst_id = inst.opcode;
      uop.uop_id = group;
      uop.opcode = inst.opcode;
      uop.vd = phys_vd;
      uop.vs1 = phys_vs1;
      uop.vs2 = phys_vs2;
      uop.vm = vm;
      uop.sew = sew_code;
      uop.lmul = lmul_code;
      uop.vl = vl;
      uop.vstart = 0;
      uop.is_masked = (vm != 0);
      uop.uop_count = lmul_val;
      uops.push_back(uop);
    }

    return uops;
  }

  /**
   * @brief Get LMUL multiplier from LMUL code
   * @param lmul_code LMUL encoding (0=1, 1=2, 2=4, 3=8)
   * @return LMUL value (1, 2, 4, or 8)
   */
  static uint32_t getLMULValue(uint32_t lmul_code) {
    if (lmul_code > 3) return 1;  // Invalid, default to 1
    return 1 << lmul_code;
  }

  /**
   * @brief Check if instruction requires stripmining
   * @param lmul_code LMUL encoding
   * @return true if LMUL > 1
   */
  static bool requiresStripmining(uint32_t lmul_code) {
    return lmul_code > 0;  // LMUL > 1
  }

 private:
  /**
   * @brief Map logical vector register index to physical register
   *
   * Implements RISC-V vector register mapping based on LMUL.
   *
   * @param logical_idx Logical register index (0-31)
   * @param group Register group number (0 to LMUL-1)
   * @param lmul LMUL multiplier (1, 2, 4, or 8)
   * @return Physical register index
   *
   * Examples with 32 registers total:
   *   LMUL=1: v0→0, v1→1, ..., v31→31
   *   LMUL=2, v0, group=0: physical=0 (v0)
   *   LMUL=2, v0, group=1: physical=1 (v1)
   *   LMUL=2, v2, group=0: physical=2 (v2)
   *   LMUL=2, v2, group=1: physical=3 (v3)
   *
   *   LMUL=4, v0, group=0: physical=0
   *   LMUL=4, v0, group=1: physical=1
   *   LMUL=4, v0, group=2: physical=2
   *   LMUL=4, v0, group=3: physical=3
   *   LMUL=4, v4, group=0: physical=4
   */
  static uint32_t mapLogicalToPhysical(uint32_t logical_idx, uint32_t group,
                                       uint32_t lmul) {
    // Logical register must be aligned to LMUL boundary
    // For example: with LMUL=4, only v0, v4, v8, v12, ... are valid starts

    // Calculate base register (must be LMUL-aligned)
    uint32_t base = (logical_idx / lmul) * lmul;

    // Physical register = base + group offset
    return base + group;
  }
};

}  // namespace Architecture

#endif  // RVV_DECODER_H
