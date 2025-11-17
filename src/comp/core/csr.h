#ifndef CORE_CSR_H
#define CORE_CSR_H

#include <cstdint>
#include <stdexcept>
#include <string>

namespace Architecture {

/**
 * @brief RISC-V CSR Address Space (12-bit addresses)
 *
 * Following RISC-V ISA specification (RV32I/RV64I/RVV extensions)
 */
enum class CsrAddress : uint16_t {
  // Floating-Point CSRs (RV32F/RV64F)
  FFLAGS = 0x001,  ///< Floating-point flags
  FRM = 0x002,     ///< Floating-point rounding mode
  FCSR = 0x003,    ///< Floating-point control and status

  // Vector CSRs (RVV)
  VSTART = 0x008,  ///< Vector start register
  VXSAT = 0x009,   ///< Vector fixed-point saturation flag
  VXRM = 0x00A,    ///< Vector fixed-point rounding mode
  VL = 0xC20,      ///< Vector length
  VTYPE = 0xC21,   ///< Vector data type
  VLENB = 0xC22,   ///< Vector length in bytes

  // Machine Mode CSRs
  MSTATUS = 0x300,   ///< Machine status
  MISA = 0x301,      ///< Machine ISA
  MIE = 0x304,       ///< Machine interrupt enable
  MTVEC = 0x305,     ///< Machine trap vector
  MSCRATCH = 0x340,  ///< Machine scratch
  MEPC = 0x341,      ///< Machine exception PC
  MCAUSE = 0x342,    ///< Machine exception cause
  MTVAL = 0x343,     ///< Machine trap value

  // Debug Mode CSRs
  TSELECT = 0x7A0,    ///< Trigger select
  TDATA1 = 0x7A1,     ///< Trigger data 1
  TDATA2 = 0x7A2,     ///< Trigger data 2
  TINFO = 0x7A4,      ///< Trigger info
  DCSR = 0x7B0,       ///< Debug control and status
  DPC = 0x7B1,        ///< Debug PC
  DSCRATCH0 = 0x7B2,  ///< Debug scratch 0
  DSCRATCH1 = 0x7B3,  ///< Debug scratch 1

  // Context Registers (CoralNPU-specific)
  MCONTEXT0 = 0x7C0,  ///< Machine context 0
  MCONTEXT1 = 0x7C1,  ///< Machine context 1
  MCONTEXT2 = 0x7C2,  ///< Machine context 2
  MCONTEXT3 = 0x7C3,  ///< Machine context 3
  MCONTEXT4 = 0x7C4,  ///< Machine context 4
  MCONTEXT5 = 0x7C5,  ///< Machine context 5
  MCONTEXT6 = 0x7C6,  ///< Machine context 6
  MCONTEXT7 = 0x7C7,  ///< Machine context 7
  MPC = 0x7E0,        ///< Machine program counter
  MSP = 0x7E1,        ///< Machine stack pointer

  // Performance Counter CSRs
  MCYCLE = 0xB00,     ///< Machine cycle counter (low)
  MINSTRET = 0xB02,   ///< Machine instructions retired (low)
  MCYCLEH = 0xB80,    ///< Machine cycle counter (high)
  MINSTRETH = 0xB82,  ///< Machine instructions retired (high)

  // Machine Information CSRs
  MVENDORID = 0xF11,  ///< Machine vendor ID
  MARCHID = 0xF12,    ///< Machine architecture ID
  MIMPID = 0xF13,     ///< Machine implementation ID
  MHARTID = 0xF14,    ///< Machine hart ID

  // Custom CSRs (CoralNPU)
  KISA = 0xFC0,   ///< Custom ISA register
  KSCM0 = 0xFC4,  ///< Custom SCM info 0
  KSCM1 = 0xFC8,  ///< Custom SCM info 1
  KSCM2 = 0xFCC,  ///< Custom SCM info 2
  KSCM3 = 0xFD0,  ///< Custom SCM info 3
  KSCM4 = 0xFD4,  ///< Custom SCM info 4
};

/**
 * @brief CSR Operation Type
 *
 * Following RISC-V CSR instruction encoding
 */
enum class CsrOperation : uint8_t {
  CSRRW = 0b001,  ///< Atomic Read/Write (write rs1 to CSR, rd = old CSR value)
  CSRRS = 0b010,  ///< Atomic Read and Set Bits (rd = old CSR, CSR |= rs1)
  CSRRC = 0b011,  ///< Atomic Read and Clear Bits (rd = old CSR, CSR &= ~rs1)
};

/**
 * @brief Control and Status Register (CSR) Suite for Scalar Core
 *
 * Following RISC-V ISA specification and Coral NPU CSR implementation.
 * These registers control core behavior and provide status information.
 *
 * CSR Categories:
 * 1. Machine Mode Privilege Registers (mstatus, mie, mtvec, mepc, mcause, etc.)
 * 2. Performance Counter CSRs (mcycle, minstret and their high 32-bit
 * counterparts)
 * 3. Privilege Mode and Status (privilege_mode, halted, fault, wfi)
 * 4. Context Registers (mcontext0-7 for multi-context execution)
 * 5. Vector Extension CSRs (vstart, vl, vtype, vxrm, vxsat - RVV)
 * 6. Floating-Point CSRs (fflags, frm - RV32F)
 * 7. Custom CoralNPU CSRs (mvendorid, marchid, mimpid, mhartid, kisa)
 */
class CoreCSRs {
 public:
  /**
   * @brief Privilege Mode Enumeration
   *
   * Values (RISC-V standard):
   * - MACHINE (0): Machine mode (M) - highest privilege
   * - USER (1): User mode (U) - lowest privilege
   * - DEBUG (2): Debug mode - for debug module
   */
  enum class PrivilegeMode { MACHINE = 0, USER = 1, DEBUG = 2 };

  // ========== Machine Mode Privilege Registers ==========

  /**
   * @brief mstatus - Machine Status Register
   *
   * Bit Layout (32-bit):
   * - [1:0] (MIE, SIE, UIE): Interrupt enable bits (not used in simplified)
   * - [4:3] (MPIE, SPIE): Prior interrupt enable (not used)
   * - [6:5] (MPP, SPP): Prior privilege mode
   * - [7]: TSR (Trap SRET)
   * - [8]: TW (Timeout Wait)
   * - [9]: TVM (Trap Virtual Memory)
   * - [12:11]: MPP - Machine previous privilege
   * - [14:13]: FS - Floating-point status (dirty, clean, off)
   * - [16:15]: XS - Extension status
   * - [18:17]: SD - Dirty state
   *
   * CoralNPU use: Track MPP and FS bits
   */
  struct MStatus {
    uint32_t value = 0;

    /// Extract MPP (Machine Previous Privilege) bits [12:11]
    uint32_t getMPP() const { return (value >> 11) & 0x3; }

    /// Set MPP bits
    void setMPP(uint32_t mpp) {
      value = (value & ~(0x3UL << 11)) | ((mpp & 0x3) << 11);
    }

    /// Extract FS (Floating-point Status) bits [14:13]
    uint32_t getFS() const { return (value >> 13) & 0x3; }

    /// Set FS bits
    void setFS(uint32_t fs) {
      value = (value & ~(0x3UL << 13)) | ((fs & 0x3) << 13);
    }

    /// Extract VS (Vector Status) bits [16:15]
    uint32_t getVS() const { return (value >> 15) & 0x3; }

    /// Set VS bits
    void setVS(uint32_t vs) {
      value = (value & ~(0x3UL << 15)) | ((vs & 0x3) << 15);
    }
  } mstatus;

  /**
   * @brief misa - Machine ISA Register (read-only after reset)
   *
   * Bit Layout (32-bit):
   * - [30:0]: ISA bits (A, B, C, D, E, F, I, M, Q, U, V, etc.)
   * - [31:30]: MXL - Max XLEN (01 = 32-bit, 10 = 64-bit, 11 = 128-bit)
   *
   * CoralNPU: Indicates supported extensions (I, M, V, etc.)
   */
  struct Misa {
    // Default: 32-bit RISC-V with I, M extensions
    // Bit 30 = 1 (XLEN_32), Bit 8 = 1 (I), Bit 12 = 1 (M)
    uint32_t value = 0x40001100;
  } misa;

  /**
   * @brief mie - Machine Interrupt Enable Register
   *
   * Bit Layout (32-bit):
   * - [3]: MSIE - Machine software interrupt enable
   * - [7]: MTIE - Machine timer interrupt enable
   * - [11]: MEIE - Machine external interrupt enable
   */
  uint32_t mie = 0;

  /**
   * @brief mtvec - Machine Trap Vector Base Address Register
   *
   * Bit Layout (32-bit):
   * - [31:2]: Base address of trap vector (4-byte aligned)
   * - [1:0]: Mode (0 = direct, 1 = vectored)
   *
   * Points to exception/interrupt handler code.
   * When a trap occurs, PC is set to (mtvec & ~0x3) or (mtvec & ~0x3) + 4 *
   * cause
   */
  uint32_t mtvec = 0;

  /**
   * @brief mscratch - Machine Scratch Register
   *
   * General-purpose register for machine mode code.
   * Typically used to save a register value before accessing CSRs in trap
   * handlers. Not used by hardware, purely for software convenience.
   */
  uint32_t mscratch = 0;

  /**
   * @brief mepc - Machine Exception Program Counter
   *
   * Address of instruction that encountered exception or interrupt.
   * Saved by hardware when trap occurs, used by handler to resume execution.
   * For synchronous exceptions: points to faulting instruction
   * For asynchronous interrupts: points to next instruction after interrupted
   * one
   */
  uint32_t mepc = 0;

  /**
   * @brief mcause - Machine Cause Register
   *
   * Bit Layout (32-bit):
   * - [30:0]: Exception code:
   *   - 0: Instruction address misaligned
   *   - 1: Instruction access fault
   *   - 2: Illegal instruction
   *   - 3: Breakpoint
   *   - 4: Load address misaligned
   *   - 5: Load access fault
   *   - 6: Store/AMO address misaligned
   *   - 7: Store/AMO access fault
   *   - 8-11: Environment calls/breaks
   *   - 12-15: Instruction page fault/load page fault/store page fault/reserved
   * - [31]: Interrupt flag (1 = interrupt, 0 = exception)
   *
   * Indicates cause of trap (interrupt or exception type)
   */
  uint32_t mcause = 0;

  /**
   * @brief mtval - Machine Trap Value Register
   *
   * Additional trap information:
   * - For page faults: virtual address that caused fault
   * - For illegal instructions: instruction encoding
   * - For misaligned accesses: address that caused misalignment
   * - For other traps: implementation-defined or zero
   */
  uint32_t mtval = 0;

  // ========== Performance Counter CSRs ==========

  /**
   * @brief mcycle - Machine Cycle Counter (low 32-bits)
   *
   * 64-bit counter (mcycle:mcycleh) that increments on every clock cycle.
   * Used for performance measurement, timing, and profiling.
   * Wraps around after 2^64 cycles.
   */
  uint32_t mcycle = 0;

  /**
   * @brief mcycleh - Machine Cycle Counter (high 32-bits)
   *
   * Upper 32 bits of 64-bit cycle counter.
   * Together with mcycle forms a 64-bit counter: (mcycleh << 32) | mcycle
   */
  uint32_t mcycleh = 0;

  /**
   * @brief minstret - Machine Instructions Retired (low 32-bits)
   *
   * 64-bit counter (minstret:minstreth) that increments when instruction
   * completes execution (retires from execution pipeline).
   * Used to measure instruction throughput and CPI (cycles per instruction).
   * Wraps around after 2^64 instructions.
   */
  uint32_t minstret = 0;

  /**
   * @brief minstreth - Machine Instructions Retired (high 32-bits)
   *
   * Upper 32 bits of 64-bit instruction counter.
   * Together with minstret forms a 64-bit counter: (minstreth << 32) | minstret
   */
  uint32_t minstreth = 0;

  // ========== Privilege Mode and Status Control ==========

  /**
   * @brief privilege_mode - Current privilege mode
   *
   * Values (RISC-V standard):
   * - MACHINE (0): Machine mode (M) - highest privilege, all instructions
   * available
   * - USER (1): User mode (U) - lower privilege, restricted instructions
   * - DEBUG (2): Debug mode - for debug module operations
   *
   * Determines which instructions and CSRs are accessible, and privilege level
   * of memory access. Changed by trap/return and debug entry/exit.
   */
  PrivilegeMode privilege_mode = PrivilegeMode::MACHINE;

  /**
   * @brief halted - Instruction fetch/execution halted
   *
   * Set by reset, explicit halt command, or halt-on-ebreak (when ebreak_halt
   * enabled). When true, no new instructions are fetched or executed. Used for
   * simulation termination or debug halt conditions.
   *
   * Cleared by: explicit reset or debug resume (if debug module present)
   */
  bool halted = false;

  /**
   * @brief fault - Fault/exception occurred
   *
   * Set when an unrecoverable fault or exception occurs that cannot be handled
   * by the trap/exception mechanism. Indicates the core has encountered a
   * critical error condition from which recovery may be impossible.
   *
   * Examples: double fault, bus error during exception handling
   */
  bool fault = false;

  /**
   * @brief wfi - Wait For Interrupt flag
   *
   * Set by WFI (Wait for Interrupt) instruction. Core stops instruction
   * execution and enters low-power state, waiting for an interrupt to resume.
   * Cleared when an interrupt arrives via IRQ signal.
   *
   * Implementation: When wfi=true:
   * - No new instructions are fetched
   * - Clock may be gated (for power efficiency)
   * - Interrupts are sampled
   */
  bool wfi = false;

  // ========== Context Registers for Multi-Context Execution ==========

  /**
   * @brief mcontext0-7 - Machine Context Registers
   *
   * 8 general-purpose 32-bit registers for:
   * - Saving/restoring processor state during context switches
   * - Thread-local data storage in multi-threaded systems
   * - Extended state information not covered by main registers
   * - Task identifiers or context IDs
   *
   * CoralNPU feature: Additional context storage for multi-context execution
   * These are typically written by software during context save operations
   * and read during context restore.
   */
  uint32_t mcontext0 = 0;
  uint32_t mcontext1 = 0;
  uint32_t mcontext2 = 0;
  uint32_t mcontext3 = 0;
  uint32_t mcontext4 = 0;
  uint32_t mcontext5 = 0;
  uint32_t mcontext6 = 0;
  uint32_t mcontext7 = 0;

  /**
   * @brief mpc - Machine Program Counter (read from mepc)
   *
   * Copy or shadow of instruction address for debug/trace purposes.
   * Read by debug module or trace infrastructure.
   * Updated along with mepc when exceptions occur.
   */
  uint32_t mpc = 0;

  /**
   * @brief msp - Machine Stack Pointer (software convention)
   *
   * By convention, register x2 is used as stack pointer.
   * This CSR provides a place to save/restore SP across context switches.
   * Not enforced by hardware, purely for software convenience.
   */
  uint32_t msp = 0;

  // ========== Vector Extension (RVV) Related CSRs ==========

  /**
   * @brief vstart - Vector Start Register
   *
   * Index of first element to be executed in current vector instruction.
   * Used for precise exception handling and fault recovery in vector
   * operations. Set to 0 at beginning of instruction, updated by hardware on
   * fault.
   *
   * Range: 0 to VLEN-1
   * On vector fault: Records which element caused exception, allowing
   * instruction to be retried from that element using vstart.
   */
  uint32_t vstart = 0;

  /**
   * @brief vl - Vector Length
   *
   * Number of active elements in current vector operation.
   * Set by VSETVL/VSETVLI instructions based on AVL (Application Vector
   * Length). Determines how many elements are processed by vector instructions.
   *
   * Range: 0 to VLEN (where VLEN is vector length in bits, typically 256)
   * Usage: Vector instructions process vl elements, ignoring tail elements
   */
  uint32_t vl = 0;

  /**
   * @brief vtype - Vector Data Type Register
   *
   * Encodes vector element properties and operation modes.
   *
   * Bit Layout (32-bit):
   * - [2:0]: SEW - Selected Element Width:
   *   - 3 = 8-bit elements
   *   - 4 = 16-bit elements
   *   - 5 = 32-bit elements
   *   - 6 = 64-bit elements
   *   - other = invalid
   * - [5:3]: LMUL - Vector Register Group Multiplier (LMUL = 2^lmul):
   *   - 0-7: LMUL = 1, 2, 4, 8, 1/2, 1/4, 1/8, reserved
   *   - Determines vector register grouping
   * - [6]: TA - Tail Agnostic
   *   - 0: Tail elements become undefined
   *   - 1: Tail elements remain unchanged
   * - [7]: MA - Mask Agnostic
   *   - 0: Masked-out elements undefined
   *   - 1: Masked-out elements unchanged
   * - [31:8]: Reserved (must be zero)
   *
   * Configures vector element properties and operation behavior
   */
  uint32_t vtype = 0;

  /**
   * @brief vxrm - Vector Fixed-Point Rounding Mode
   *
   * Controls rounding behavior for fixed-point vector operations
   * (especially important for narrowing operations).
   *
   * Bit Layout (2-bit):
   * - 00 (0): RNE - Round to nearest, ties to even (recommended default)
   * - 01 (1): RTZ - Round towards zero (truncate)
   * - 10 (2): RDN - Round down (towards negative infinity)
   * - 11 (3): RUP - Round up (towards positive infinity)
   *
   * Applied to: vssra, vnclip, vnclipu operations on fixed-point data
   */
  uint32_t vxrm = 0;

  /**
   * @brief vxsat - Vector Fixed-Point Saturation Flag
   *
   * Sticky flag set when any vector fixed-point operation saturates.
   * Indicates saturation occurred, without tracking per-element saturation.
   * Cleared by software writing 0; set by hardware when saturation detected.
   *
   * Used for: vsadd, vssub, vsmul (saturating operations)
   * Allows software to detect when output range is exceeded without
   * storing per-element saturation flags.
   */
  bool vxsat = false;

  // ========== Floating-Point CSRs (RV32F extension) ==========

  /**
   * @brief fflags - Floating-Point Flags
   *
   * Set of 5 sticky flags indicating floating-point exceptional conditions.
   * Flags are set by FP operations and persist until explicitly cleared.
   *
   * Bit Layout (5-bit):
   * - [0]: NX - Inexact exception flag
   *   Set when FP operation produces inexact result (rounded)
   * - [1]: UF - Underflow exception flag
   *   Set when FP operation underflows to zero or denormal
   * - [2]: OF - Overflow exception flag
   *   Set when FP operation overflows (result > largest representable)
   * - [3]: DZ - Divide by zero exception flag
   *   Set when dividing by zero produces infinity
   * - [4]: NV - Invalid operation exception flag
   *   Set for invalid operations: 0/0, sqrt(negative), etc.
   *
   * These flags accumulate; clearing requires explicit write to fflags.
   */
  uint32_t fflags = 0;

  /**
   * @brief frm - Floating-Point Rounding Mode
   *
   * Specifies default rounding mode for floating-point operations.
   * Can be overridden by individual FP instruction encoding.
   *
   * Bit Layout (3-bit):
   * - 000 (0): RNE - Round to nearest, ties to even (IEEE 754 default)
   * - 001 (1): RTZ - Round towards zero (truncate)
   * - 010 (2): RDN - Round down (towards negative infinity, floor)
   * - 011 (3): RUP - Round up (towards positive infinity, ceil)
   * - 100 (4): RMM - Round to nearest, ties to max magnitude
   * - 101-111: Reserved (implementation-defined behavior)
   *
   * Usage: Applied to all FP operations that don't have explicit rounding mode
   */
  uint32_t frm = 0;

  // ========== Custom CoralNPU CSRs ==========

  /**
   * @brief mvendorid - Machine Vendor ID (read-only)
   *
   * Value: 0x426 (Google's assigned JEDEC manufacturer ID)
   * Identifies the core implementation vendor.
   * Cannot be modified by software; read-only register.
   *
   * Used by: Bootloaders, OS kernels, diagnostics to identify hardware vendor
   */
  const uint32_t mvendorid = 0x426;

  /**
   * @brief marchid - Machine Architecture ID (read-only)
   *
   * Implementation-defined value identifying specific core architecture family.
   * Allows software to distinguish between different core microarchitectures.
   *
   * CoralNPU: Identifies which Coral NPU variant (scalar only, scalar+vector,
   * etc.)
   */
  uint32_t marchid = 0;

  /**
   * @brief mimpid - Machine Implementation ID (read-only)
   *
   * Version/revision number of core implementation.
   * Incremented for updated implementations or bug fixes.
   *
   * CoralNPU: Version field identifying specific implementation revision
   */
  uint32_t mimpid = 0;

  /**
   * @brief mhartid - Machine Hart ID (read-only)
   *
   * Unique identifier for this hardware thread (hart) within the system.
   * Used in multi-core/multi-threaded systems to identify which core/thread
   * this is. Typically set at reset and never changes.
   *
   * Value: Usually 0 for single-core, 0-N for N-core systems
   * Allows software to determine core identity for bootstrapping and
   * core-specific code
   */
  uint32_t mhartid = 0;

  /**
   * @brief kisa - Custom ISA Features Register (CoralNPU-specific)
   *
   * Custom register indicating CoralNPU-specific extension support.
   *
   * Bit Layout (32-bit):
   * - [0]: RVV support (Vector extension enabled)
   * - [1]: RV32F support (Floating-point extension enabled)
   * - [2]: Extended memory support
   * - [3]: Custom instruction set extensions
   * - [31:4]: Reserved for future use
   *
   * Used by: Software to detect available CoralNPU features at runtime
   */
  uint32_t kisa = 0;

  /**
   * @brief scm_info - Specialized Compute Module (SCM) Information
   *
   * 64-bit register encoding specialized compute module capabilities
   * and implementation revision information.
   *
   * CoralNPU feature: Provides hardware-level details about:
   * - Available specialized compute units (DSP, crypto, etc.)
   * - Module revision and generation
   * - Feature availability matrix
   */
  uint64_t scm_info = 0;

  // ========== Helper Methods ==========

  /**
   * @brief Get full 64-bit cycle counter
   */
  uint64_t getMcycle() const {
    return (static_cast<uint64_t>(mcycleh) << 32) | mcycle;
  }

  /**
   * @brief Set full 64-bit cycle counter
   */
  void setMcycle(uint64_t value) {
    mcycle = value & 0xFFFFFFFF;
    mcycleh = (value >> 32) & 0xFFFFFFFF;
  }

  /**
   * @brief Get full 64-bit instruction counter
   */
  uint64_t getMinstret() const {
    return (static_cast<uint64_t>(minstreth) << 32) | minstret;
  }

  /**
   * @brief Set full 64-bit instruction counter
   */
  void setMinstret(uint64_t value) {
    minstret = value & 0xFFFFFFFF;
    minstreth = (value >> 32) & 0xFFFFFFFF;
  }

  /**
   * @brief Reset all CSRs to their default values
   */
  void reset() {
    mstatus.value = 0;
    misa.value = 0x40001100;
    mie = 0;
    mtvec = 0;
    mscratch = 0;
    mepc = 0;
    mcause = 0;
    mtval = 0;
    mcycle = 0;
    mcycleh = 0;
    minstret = 0;
    minstreth = 0;
    privilege_mode = PrivilegeMode::MACHINE;
    halted = false;
    fault = false;
    wfi = false;
    mcontext0 = 0;
    mcontext1 = 0;
    mcontext2 = 0;
    mcontext3 = 0;
    mcontext4 = 0;
    mcontext5 = 0;
    mcontext6 = 0;
    mcontext7 = 0;
    mpc = 0;
    msp = 0;
    vstart = 0;
    vl = 0;
    vtype = 0;
    vxrm = 0;
    vxsat = false;
    fflags = 0;
    frm = 0;
    marchid = 0;
    mimpid = 0;
    mhartid = 0;
    kisa = 0;
    scm_info = 0;
  }

  // ========== CSR Read/Write Operations (Following RISC-V ISA) ==========

  /**
   * @brief Read a CSR register by address
   *
   * @param address CSR address (12-bit encoding from RISC-V ISA)
   * @return Current value of the CSR, or 0 if address is invalid
   *
   * Following Coral NPU CSR implementation:
   * - Supports all machine-mode and vector CSRs
   * - Read-only CSRs return their current value
   * - Invalid addresses return 0
   */
  uint32_t readCSR(CsrAddress address) {
    switch (address) {
      // Floating-Point CSRs
      case CsrAddress::FFLAGS:
        return fflags & 0x1F;  // 5-bit flags
      case CsrAddress::FRM:
        return frm & 0x7;  // 3-bit mode
      case CsrAddress::FCSR:
        return ((frm & 0x7) << 5) | (fflags & 0x1F);

      // Vector CSRs (RVV)
      case CsrAddress::VSTART:
        return vstart;
      case CsrAddress::VXSAT:
        return vxsat ? 1 : 0;
      case CsrAddress::VXRM:
        return vxrm & 0x3;
      case CsrAddress::VL:
        return vl;
      case CsrAddress::VTYPE:
        return vtype;
      case CsrAddress::VLENB:
        return 32;  // CoralNPU: 256-bit vector = 32 bytes

      // Machine Mode CSRs
      case CsrAddress::MSTATUS:
        return mstatus.value;
      case CsrAddress::MISA:
        return misa.value;
      case CsrAddress::MIE:
        return mie;
      case CsrAddress::MTVEC:
        return mtvec;
      case CsrAddress::MSCRATCH:
        return mscratch;
      case CsrAddress::MEPC:
        return mepc;
      case CsrAddress::MCAUSE:
        return mcause;
      case CsrAddress::MTVAL:
        return mtval;

      // Context Registers
      case CsrAddress::MCONTEXT0:
        return mcontext0;
      case CsrAddress::MCONTEXT1:
        return mcontext1;
      case CsrAddress::MCONTEXT2:
        return mcontext2;
      case CsrAddress::MCONTEXT3:
        return mcontext3;
      case CsrAddress::MCONTEXT4:
        return mcontext4;
      case CsrAddress::MCONTEXT5:
        return mcontext5;
      case CsrAddress::MCONTEXT6:
        return mcontext6;
      case CsrAddress::MCONTEXT7:
        return mcontext7;
      case CsrAddress::MPC:
        return mpc;
      case CsrAddress::MSP:
        return msp;

      // Performance Counters (lower 32 bits)
      case CsrAddress::MCYCLE:
        return mcycle;
      case CsrAddress::MINSTRET:
        return minstret;
      case CsrAddress::MCYCLEH:
        return mcycleh;
      case CsrAddress::MINSTRETH:
        return minstreth;

      // Machine Information (read-only)
      case CsrAddress::MVENDORID:
        return mvendorid;
      case CsrAddress::MARCHID:
        return marchid;
      case CsrAddress::MIMPID:
        return mimpid;
      case CsrAddress::MHARTID:
        return mhartid;

      // Custom CSRs (CoralNPU)
      case CsrAddress::KISA:
        return kisa;
      case CsrAddress::KSCM0:
        return scm_info & 0xFFFFFFFF;
      case CsrAddress::KSCM1:
        return (scm_info >> 32) & 0xFFFFFFFF;
      case CsrAddress::KSCM2:
        return (scm_info >> 64) & 0xFFFFFFFF;
      case CsrAddress::KSCM3:
        return 0;  // Not implemented in 64-bit
      case CsrAddress::KSCM4:
        return 0;  // Not implemented in 64-bit

      // Debug CSRs - not fully implemented
      case CsrAddress::TSELECT:
      case CsrAddress::TDATA1:
      case CsrAddress::TDATA2:
      case CsrAddress::TINFO:
      case CsrAddress::DCSR:
      case CsrAddress::DPC:
      case CsrAddress::DSCRATCH0:
      case CsrAddress::DSCRATCH1:
        return 0;  // Debug CSRs not implemented

      default:
        return 0;  // Invalid address
    }
  }

  /**
   * @brief Write a CSR register by address
   *
   * @param address CSR address
   * @param value Value to write
   *
   * Following Coral NPU CSR implementation:
   * - Performs raw write (no masking)
   * - Some CSRs have partial write restrictions
   * - Read-only CSRs silently ignore writes
   */
  void writeCSR(CsrAddress address, uint32_t value) {
    switch (address) {
      // Floating-Point CSRs
      case CsrAddress::FFLAGS:
        fflags = value & 0x1F;
        break;
      case CsrAddress::FRM:
        frm = value & 0x7;
        break;
      case CsrAddress::FCSR:
        fflags = value & 0x1F;
        frm = (value >> 5) & 0x7;
        break;

      // Vector CSRs (RVV)
      case CsrAddress::VSTART:
        vstart = value;
        break;
      case CsrAddress::VXSAT:
        vxsat = (value & 0x1) != 0;
        break;
      case CsrAddress::VXRM:
        vxrm = value & 0x3;
        break;
      case CsrAddress::VL:
        vl = value;
        break;
      case CsrAddress::VTYPE:
        vtype = value;
        break;
      case CsrAddress::VLENB:
        // Read-only, ignore write
        break;

      // Machine Mode CSRs
      case CsrAddress::MSTATUS:
        mstatus.value = value;
        break;
      case CsrAddress::MISA:
        // Typically read-only after reset
        break;
      case CsrAddress::MIE:
        mie = value;
        break;
      case CsrAddress::MTVEC:
        mtvec = value;
        break;
      case CsrAddress::MSCRATCH:
        mscratch = value;
        break;
      case CsrAddress::MEPC:
        mepc = value;
        break;
      case CsrAddress::MCAUSE:
        mcause = value;
        break;
      case CsrAddress::MTVAL:
        mtval = value;
        break;

      // Context Registers
      case CsrAddress::MCONTEXT0:
        mcontext0 = value;
        break;
      case CsrAddress::MCONTEXT1:
        mcontext1 = value;
        break;
      case CsrAddress::MCONTEXT2:
        mcontext2 = value;
        break;
      case CsrAddress::MCONTEXT3:
        mcontext3 = value;
        break;
      case CsrAddress::MCONTEXT4:
        mcontext4 = value;
        break;
      case CsrAddress::MCONTEXT5:
        mcontext5 = value;
        break;
      case CsrAddress::MCONTEXT6:
        mcontext6 = value;
        break;
      case CsrAddress::MCONTEXT7:
        mcontext7 = value;
        break;
      case CsrAddress::MPC:
        mpc = value;
        break;
      case CsrAddress::MSP:
        msp = value;
        break;

      // Performance Counters
      case CsrAddress::MCYCLE:
        mcycle = value;
        break;
      case CsrAddress::MINSTRET:
        minstret = value;
        break;
      case CsrAddress::MCYCLEH:
        mcycleh = value;
        break;
      case CsrAddress::MINSTRETH:
        minstreth = value;
        break;

      // Machine Information (read-only)
      case CsrAddress::MVENDORID:
      case CsrAddress::MARCHID:
      case CsrAddress::MIMPID:
      case CsrAddress::MHARTID:
        // Read-only, ignore writes
        break;

      // Custom CSRs (CoralNPU)
      case CsrAddress::KISA:
        kisa = value;
        break;
      case CsrAddress::KSCM0:
      case CsrAddress::KSCM1:
      case CsrAddress::KSCM2:
      case CsrAddress::KSCM3:
      case CsrAddress::KSCM4:
        // Custom SCM info, ignore writes in simplified implementation
        break;

      // Debug CSRs - not fully implemented
      case CsrAddress::TSELECT:
      case CsrAddress::TDATA1:
      case CsrAddress::TDATA2:
      case CsrAddress::TINFO:
      case CsrAddress::DCSR:
      case CsrAddress::DPC:
      case CsrAddress::DSCRATCH0:
      case CsrAddress::DSCRATCH1:
        // Debug CSRs not implemented
        break;

      default:
        // Invalid address, silently ignore
        break;
    }
  }

  /**
   * @brief Perform CSR read-modify-write operation
   *
   * @param address CSR address
   * @param operation RMW operation type (CSRRW, CSRRS, CSRRC)
   * @param value Operand for the operation
   * @return Old value of CSR before modification
   *
   * Following RISC-V ISA semantics:
   * - CSRRW: Write value, return old CSR value (unconditional write)
   * - CSRRS: Set bits (CSR |= value), return old CSR value
   * - CSRRC: Clear bits (CSR &= ~value), return old CSR value
   */
  uint32_t modifyCSR(CsrAddress address, CsrOperation operation,
                     uint32_t value) {
    uint32_t old_value = readCSR(address);

    switch (operation) {
      case CsrOperation::CSRRW:
        // Atomic Read/Write: write value, return old value
        writeCSR(address, value);
        break;

      case CsrOperation::CSRRS:
        // Atomic Read and Set Bits: CSR |= value
        writeCSR(address, old_value | value);
        break;

      case CsrOperation::CSRRC:
        // Atomic Read and Clear Bits: CSR &= ~value
        writeCSR(address, old_value & ~value);
        break;
    }

    return old_value;
  }

  /**
   * @brief Increment cycle counter (called once per clock cycle)
   *
   * Updates mcycle and mcycleh as a 64-bit counter with automatic overflow
   */
  void incrementCycle() {
    uint64_t full_mcycle = getMcycle();
    full_mcycle++;
    setMcycle(full_mcycle);
  }

  /**
   * @brief Increment instruction counter
   *
   * @param count Number of instructions to add (default 1)
   *
   * Updates minstret and minstreth as a 64-bit counter with automatic overflow
   */
  void incrementInstret(uint32_t count = 1) {
    uint64_t full_minstret = getMinstret();
    full_minstret += count;
    setMinstret(full_minstret);
  }

  /**
   * @brief Set trap information (called when exception/interrupt occurs)
   *
   * @param pc Program counter of faulting instruction
   * @param cause Exception/interrupt cause code
   * @param trap_value Additional trap information (address, instruction, etc.)
   */
  void setTrap(uint32_t pc, uint32_t cause, uint32_t trap_value = 0) {
    mepc = pc;
    mcause = cause;
    mtval = trap_value;
  }

  /**
   * @brief Update mstatus on exception entry
   *
   * Sets privilege mode change and interrupt enable updates
   * (Simplified: only updates basic fields)
   */
  void enterException() {
    // Save current privilege mode and interrupt state
    // (In real RISC-V, would save more state)
    // For simplified implementation, just mark as in exception
  }

  /**
   * @brief Restore mstatus on exception return
   *
   * Restores previous privilege mode and interrupt enables
   * (Simplified: not fully implemented)
   */
  void exitException() {
    // Restore previous privilege mode and interrupt state
    // (In real RISC-V, would restore saved state)
  }
};

}  // namespace Architecture

#endif  // CORE_CSR_H
