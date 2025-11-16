#ifndef RVV_INTERFACE_H
#define RVV_INTERFACE_H

#include <cstdint>
#include <memory>
#include <vector>

#include "rvv_rob.h"

namespace Architecture {

/**
 * @brief RVV Configuration State
 *
 * Mirrors the RvvConfigState from CoralNPU Chisel
 * Tracks vector configuration and state CSRs
 */
struct RVVConfigState {
  uint32_t vl;        // Vector length
  uint32_t vstart;    // Vector start index
  bool ma;            // Mask agnostic
  bool ta;            // Tail agnostic
  uint8_t xrm;        // Fixed-point rounding mode (2 bits)
  uint8_t sew;        // Standard element width (3 bits: 0=8b, 1=16b, 2=32b)
  uint8_t lmul;       // LMUL (3 bits)
  uint8_t lmul_orig;  // Original LMUL from vset
  bool vill;          // Illegal vtype flag

  RVVConfigState()
      : vl(0),
        vstart(0),
        ma(false),
        ta(false),
        xrm(0),
        sew(0),
        lmul(0),
        lmul_orig(0),
        vill(false) {}

  /**
   * @brief Construct vtype CSR value
   *
   * See Section 3.4 of RISC-V Vector Specification v1.0
   * Format: [vill(1)|reserved(23)|ma(1)|ta(1)|sew(3)|lmul(3)]
   */
  uint32_t getVtype() const {
    uint32_t vtype = 0;
    vtype |= (vill ? 1U : 0U) << 31;  // vill at bit 31
    vtype |= (ma ? 1U : 0U) << 7;     // ma at bit 7
    vtype |= (ta ? 1U : 0U) << 6;     // ta at bit 6
    vtype |= (sew & 0x7) << 3;        // sew at bits [5:3]
    vtype |= (lmul_orig & 0x7);       // lmul at bits [2:0]
    return vtype;
  }
};

/**
 * @brief ROB-to-Retire Stage Write (scalar core facing)
 *
 * Data structure for committing RVV results back to scalar register file
 */
struct Rob2Rt {
  bool w_valid;               // Write valid
  uint32_t w_index;           // Register index
  uint64_t w_data;            // Write data
  bool w_type;                // 0=VRF, 1=XRF (scalar register file)
  uint8_t vd_type;            // Byte enable mask
  bool trap_flag;             // Exception occurred
  RVVConfigState vector_csr;  // Updated vector CSR state
  uint64_t vxsaturate;        // Saturation flags

  Rob2Rt()
      : w_valid(false),
        w_index(0),
        w_data(0),
        w_type(false),
        vd_type(0),
        trap_flag(false),
        vxsaturate(0) {}
};

/**
 * @brief Scalar to Vector Core Interface
 *
 * Mirrors RvvCoreIO from CoralNPU Chisel
 * Provides instruction decode, execution, and result write-back paths
 */
class RVVCoreInterface {
 public:
  /**
   * @brief Instruction input from Scalar Core Decode stage
   *
   * RVV instructions decoded and queued by scalar core
   * Can be multiple lanes for future multi-issue support
   */
  struct InstructionRequest {
    uint64_t inst_id;  // Instruction ID from scalar core
    uint32_t opcode;   // RVV compressed opcode
    uint32_t bits;     // 25-bit instruction payload
    uint32_t vs1_idx;  // Vector/scalar source 1
    uint32_t vs2_idx;  // Vector source 2
    uint32_t vd_idx;   // Vector destination
    uint32_t vm;       // Mask bit
    uint32_t sew;      // Element width
    uint32_t lmul;     // Element multiplier
    uint32_t vl;       // Vector length
    uint64_t pc;       // Program counter

    InstructionRequest()
        : inst_id(0),
          opcode(0),
          bits(0),
          vs1_idx(0),
          vs2_idx(0),
          vd_idx(0),
          vm(0),
          sew(0),
          lmul(0),
          vl(0),
          pc(0) {}
  };

  /**
   * @brief Register read request from Vector Core
   *
   * For reading scalar registers (rs1/rs2) from scalar core
   */
  struct ScalarRegReadReq {
    uint32_t addr;  // Scalar register address
    bool valid;     // Request valid

    ScalarRegReadReq() : addr(0), valid(false) {}
  };

  /**
   * @brief Register read response from Scalar Core
   *
   * Scalar register data for VIX operations
   */
  struct ScalarRegReadRsp {
    uint64_t data;  // Register data
    bool valid;     // Response valid

    ScalarRegReadRsp() : data(0), valid(false) {}
  };

  /**
   * @brief Vector register write request to VRF
   *
   * Vector core committing results to vector register file
   */
  struct VectorRegWrite {
    uint32_t addr;                  // Register address
    std::vector<uint8_t> data;      // Register data (vlen/8 bytes)
    std::vector<bool> byte_enable;  // Per-byte write enable
    bool valid;                     // Write valid

    VectorRegWrite() : addr(0), valid(false) {}
  };

  virtual ~RVVCoreInterface() = default;

  /**
   * @brief Issue instruction from scalar core to vector core
   *
   * @param inst Instruction request with all fields populated
   * @return true if accepted, false if vector core busy
   */
  virtual bool issueInstruction(const InstructionRequest& inst) = 0;

  /**
   * @brief Read scalar register from scalar core
   *
   * Used for VI[X] and other operations that read scalar registers
   *
   * @param addr Register address
   * @return Register data
   */
  virtual uint64_t readScalarRegister(uint32_t addr) const = 0;

  /**
   * @brief Write result to scalar register (vector->scalar path)
   *
   * For instructions like vmv.x.s that write to scalar registers
   *
   * @param addr Register address
   * @param data Data to write
   * @param mask Byte enable mask (for 64-bit reg, mask width varies)
   */
  virtual void writeScalarRegister(uint32_t addr, uint64_t data,
                                   uint8_t mask = 0xFF) = 0;

  /**
   * @brief Get current vector configuration state
   *
   * @return Current vl, vtype, vstart, etc.
   */
  virtual RVVConfigState getConfigState() const = 0;

  /**
   * @brief Update vector configuration state (from vset* instructions)
   *
   * @param config New configuration state
   */
  virtual void setConfigState(const RVVConfigState& config) = 0;

  /**
   * @brief Get vector register file write ports for result writeback
   *
   * Returns 4 parallel write ports for in-order retirement
   *
   * @return Array of 4 Rob2Rt structures
   */
  virtual std::vector<Rob2Rt> getRetireWrites() = 0;

  /**
   * @brief Check if vector core is idle
   *
   * Used by scalar core to determine when it's safe to issue vset* instructions
   *
   * @return true if no in-flight instructions
   */
  virtual bool isIdle() const = 0;

  /**
   * @brief Get queue capacity for instruction buffering
   *
   * Scalar core uses this to throttle instruction issue
   *
   * @return Available instruction queue slots (0-255)
   */
  virtual uint32_t getQueueCapacity() const = 0;

  /**
   * @brief Trap signal from vector core to scalar core
   *
   * Asynchronous exception: illegal instruction, invalid vtype, etc.
   *
   * @return true if trap active, instruction causing trap
   */
  virtual bool getTrap(InstructionRequest& trap_inst) const = 0;
};

}  // namespace Architecture

#endif  // RVV_INTERFACE_H
