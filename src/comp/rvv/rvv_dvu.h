#ifndef RVV_DVU_H
#define RVV_DVU_H

#include <cstring>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "../../packet.h"
#include "../../pipeline.h"
#include "../../port.h"
#include "../../tick.h"
#include "../../trace.h"

namespace Architecture {

/**
 * @brief RVV Vector Division result packet
 *
 * Contains division result (quotient or remainder) and metadata
 */
class RVVDVUResultPacket : public DataPacket {
 public:
  RVVDVUResultPacket(uint32_t rd = 0, uint32_t eew = 32, uint32_t vlen = 128,
                     bool is_remainder = false)
      : rd(rd),
        eew(eew),
        vlen(vlen),
        is_remainder(is_remainder),
        result_data(vlen / 8, 0) {}

  std::shared_ptr<DataPacket> clone() const override {
    return cloneWithVectors<RVVDVUResultPacket>(
        [this](RVVDVUResultPacket* p) { p->result_data = result_data; }, rd,
        eew, vlen, is_remainder);
  }

  uint32_t rd;                       // Destination vector register
  uint32_t eew;                      // Element width in bits (8/16/32/64)
  uint32_t vlen;                     // Vector length in bits
  bool is_remainder;                 // True if remainder, false if quotient
  std::vector<uint8_t> result_data;  // Result data
};

}  // namespace Architecture

/**
 * @brief RVV Division operation types
 */
enum class RVVDVUOp {
  VDIVU,  // Vector unsigned divide
  VDIV,   // Vector signed divide
  VREMU,  // Vector unsigned remainder
  VREM,   // Vector signed remainder
  UNKNOWN
};

/**
 * @brief RVV DVU Data Packet
 *
 * Contains vector operands and division operation metadata
 */
class RVVDVUDataPacket : public Architecture::DataPacket {
 public:
  RVVDVUDataPacket(uint32_t rd = 0, uint32_t rs1 = 0, uint32_t rs2 = 0,
                   uint32_t eew = 32, uint32_t vlen = 128,
                   RVVDVUOp op = RVVDVUOp::VDIVU)
      : rd(rd),
        rs1(rs1),
        rs2(rs2),
        eew(eew),
        vlen(vlen),
        op(op),
        dividend(vlen / 8, 0),
        divisor(vlen / 8, 0) {}

  std::shared_ptr<Architecture::DataPacket> clone() const override {
    return cloneWithVectors<RVVDVUDataPacket>(
        [this](RVVDVUDataPacket* p) {
          p->dividend = dividend;
          p->divisor = divisor;
        },
        rd, rs1, rs2, eew, vlen, op);
  }

  uint32_t rd;                    // Destination vector register
  uint32_t rs1, rs2;              // Source vector registers
  uint32_t eew;                   // Element width in bits (8/16/32/64)
  uint32_t vlen;                  // Vector length in bits
  RVVDVUOp op;                    // Division operation type
  std::vector<uint8_t> dividend;  // Dividend data
  std::vector<uint8_t> divisor;   // Divisor data
};

/**
 * @brief RVV Vector Division Unit - Functional model with latency parameters
 *
 * Models the CoralNPU RVV backend Division Unit with simplified functionality:
 * - Multiple division operations (VDIVU, VDIV, VREMU, VREM)
 * - Variable latency based on element width
 * - Supports different element widths (8, 16, 32 bits)
 * - No detailed hardware timing, only functional behavior and latency
 *
 * Pipeline structure (multi-cycle division):
 * - Stage 0: Decode/dispatch
 * - Stage 1-N: Iterative division (state machine driven)
 * - Stage N+1: Writeback
 *
 * Latency depends on divisor magnitude:
 * - 8-bit:  9-17 cycles
 * - 16-bit: 17-33 cycles
 * - 32-bit: 33-65 cycles
 */
class RVVVectorDVU : public Pipeline {
 public:
  /**
   * @brief Constructor
   * @param name Component name
   * @param scheduler Event scheduler
   * @param period Clock period
   * @param vlen Vector length in bits (default 128)
   */
  RVVVectorDVU(const std::string& name, EventDriven::EventScheduler& scheduler,
               uint64_t period, uint32_t vlen = 128)
      : Pipeline(name, scheduler, period, 3),
        vlen_(vlen),
        operations_executed_(0),
        division_by_zero_count_(0) {
    setupPipelineStages();
  }

  /**
   * @brief Get latency for a specific division operation (in cycles)
   *
   * Based on CoralNPU RVV backend division unit:
   * - 8-bit:  N cycles (9-17 depending on divisor)
   * - 16-bit: N cycles (17-33 depending on divisor)
   * - 32-bit: N cycles (33-65 depending on divisor)
   *
   * For functional modeling, use worst-case latencies
   */
  static uint64_t getLatency(uint32_t element_width) {
    switch (element_width) {
      case 8:
        return 17;  // 8-bit division: max 17 cycles
      case 16:
        return 33;  // 16-bit division: max 33 cycles
      case 32:
        return 65;  // 32-bit division: max 65 cycles
      case 64:
        return 129;  // 64-bit division: max 129 cycles
      default:
        return 17;
    }
  }

  /**
   * @brief Check if operation is signed or unsigned
   */
  static bool isSigned(RVVDVUOp op) {
    return (op == RVVDVUOp::VDIV) || (op == RVVDVUOp::VREM);
  }

  /**
   * @brief Check if operation returns remainder
   */
  static bool isRemainder(RVVDVUOp op) {
    return (op == RVVDVUOp::VREM) || (op == RVVDVUOp::VREMU);
  }

  /**
   * @brief Get operation name
   */
  static std::string getOpName(RVVDVUOp op) {
    switch (op) {
      case RVVDVUOp::VDIVU:
        return "VDIVU";
      case RVVDVUOp::VDIV:
        return "VDIV";
      case RVVDVUOp::VREMU:
        return "VREMU";
      case RVVDVUOp::VREM:
        return "VREM";
      default:
        return "UNKNOWN";
    }
  }

  /**
   * @brief Get the total number of operations executed
   */
  uint64_t getOperationsExecuted() const { return operations_executed_; }

  /**
   * @brief Get the count of division-by-zero operations
   */
  uint64_t getDivisionByZeroCount() const { return division_by_zero_count_; }

 private:
  uint32_t vlen_;  // Vector length in bits

  uint64_t operations_executed_;
  uint64_t division_by_zero_count_;

  /**
   * @brief Setup pipeline stages with functional operations
   *
   * Stage 0: Input - capture operands
   * Stage 1: Execute - perform iterative division
   * Stage 2: Output - prepare results
   */
  void setupPipelineStages() {
    // Stage 0: Decode stage - prepare operands
    setStageFunction(0, [this](std::shared_ptr<Architecture::DataPacket> data) {
      auto dvu_data = std::dynamic_pointer_cast<RVVDVUDataPacket>(data);
      if (!dvu_data) return data;

      // In real implementation, read from register file here
      // For now, just pass through
      return data;
    });

    // Stage 1: Execute stage - perform division
    setStageFunction(1, [this](std::shared_ptr<Architecture::DataPacket> data) {
      auto dvu_data = std::dynamic_pointer_cast<RVVDVUDataPacket>(data);
      if (!dvu_data) return data;

      // Create quotient result
      auto quotient_result = std::make_shared<Architecture::RVVDVUResultPacket>(
          dvu_data->rd, dvu_data->eew, dvu_data->vlen, false);

      // Create remainder result
      auto remainder_result =
          std::make_shared<Architecture::RVVDVUResultPacket>(
              dvu_data->rd, dvu_data->eew, dvu_data->vlen, true);

      // Execute division
      executeDivision(dvu_data, quotient_result, remainder_result);
      operations_executed_++;

      // Return quotient as primary result
      return std::static_pointer_cast<Architecture::DataPacket>(
          quotient_result);
    });

    // Stage 2: Writeback stage - prepare for output
    setStageFunction(2, [this](std::shared_ptr<Architecture::DataPacket> data) {
      // Just pass through - result is ready
      return data;
    });
  }

  /**
   * @brief Execute division operation (simplified functional model)
   */
  void executeDivision(
      std::shared_ptr<RVVDVUDataPacket> op,
      std::shared_ptr<Architecture::RVVDVUResultPacket> quotient,
      std::shared_ptr<Architecture::RVVDVUResultPacket> remainder) {
    // Simplified execution - in real implementation, perform actual computation
    // For now, demonstrate the functional structure

    uint32_t num_elements = op->vlen / op->eew;

    // Check for division by zero
    bool has_div_by_zero = false;
    for (uint32_t i = 0; i < num_elements && i < op->divisor.size(); ++i) {
      if (op->divisor[i] == 0) {
        has_div_by_zero = true;
        break;
      }
    }

    if (has_div_by_zero) {
      division_by_zero_count_++;
      // In case of division by zero:
      // - Quotient: all 1s (for signed: -1 in two's complement)
      // - Remainder: dividend (unchanged)
      for (uint32_t i = 0; i < num_elements && i < op->dividend.size(); ++i) {
        quotient->result_data[i] = 0xFF;  // All bits set
        remainder->result_data[i] = op->dividend[i];
      }
    } else {
      // Perform actual division for each element
      switch (op->eew) {
        case 8: {
          for (uint32_t i = 0; i < num_elements && i < op->dividend.size();
               ++i) {
            uint8_t dividend_val = op->dividend[i];
            uint8_t divisor_val = op->divisor[i];

            if (divisor_val != 0) {
              uint8_t q = dividend_val / divisor_val;
              uint8_t r = dividend_val % divisor_val;

              quotient->result_data[i] = q;
              remainder->result_data[i] = r;
            }
          }
          break;
        }

        case 16: {
          for (uint32_t i = 0; i < num_elements && i + 1 < op->dividend.size();
               i += 2) {
            uint16_t dividend_val =
                (static_cast<uint16_t>(op->dividend[i + 1]) << 8) |
                op->dividend[i];
            uint16_t divisor_val =
                (static_cast<uint16_t>(op->divisor[i + 1]) << 8) |
                op->divisor[i];

            if (divisor_val != 0) {
              uint16_t q = dividend_val / divisor_val;
              uint16_t r = dividend_val % divisor_val;

              quotient->result_data[i] = q & 0xFF;
              quotient->result_data[i + 1] = (q >> 8) & 0xFF;
              remainder->result_data[i] = r & 0xFF;
              remainder->result_data[i + 1] = (r >> 8) & 0xFF;
            }
          }
          break;
        }

        case 32: {
          for (uint32_t i = 0; i < num_elements && i + 3 < op->dividend.size();
               i += 4) {
            uint32_t dividend_val =
                (static_cast<uint32_t>(op->dividend[i + 3]) << 24) |
                (static_cast<uint32_t>(op->dividend[i + 2]) << 16) |
                (static_cast<uint32_t>(op->dividend[i + 1]) << 8) |
                op->dividend[i];
            uint32_t divisor_val =
                (static_cast<uint32_t>(op->divisor[i + 3]) << 24) |
                (static_cast<uint32_t>(op->divisor[i + 2]) << 16) |
                (static_cast<uint32_t>(op->divisor[i + 1]) << 8) |
                op->divisor[i];

            if (divisor_val != 0) {
              uint32_t q = dividend_val / divisor_val;
              uint32_t r = dividend_val % divisor_val;

              quotient->result_data[i] = q & 0xFF;
              quotient->result_data[i + 1] = (q >> 8) & 0xFF;
              quotient->result_data[i + 2] = (q >> 16) & 0xFF;
              quotient->result_data[i + 3] = (q >> 24) & 0xFF;
              remainder->result_data[i] = r & 0xFF;
              remainder->result_data[i + 1] = (r >> 8) & 0xFF;
              remainder->result_data[i + 2] = (r >> 16) & 0xFF;
              remainder->result_data[i + 3] = (r >> 24) & 0xFF;
            }
          }
          break;
        }

        default:
          break;
      }
    }
  }
};

#endif  // RVV_DVU_H
