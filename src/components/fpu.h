#ifndef FPU_H
#define FPU_H

#include <cmath>
#include <cstring>
#include <memory>
#include <string>

#include "../packet.h"
#include "../port.h"
#include "../tick.h"
#include "../trace.h"
#include "pipeline.h"

/**
 * @brief FPU Operation Types
 *
 * Supports basic floating-point operations and fused multiply-add operations
 * Similar to Coral NPU's FpuOptype
 */
enum class FPUOp {
  // Basic floating-point operations
  FADD,  // Floating-point addition: rd = rs1 + rs2
  FSUB,  // Floating-point subtraction: rd = rs1 - rs2
  FMUL,  // Floating-point multiplication: rd = rs1 * rs2

  // Fused multiply-accumulate operations (more precise)
  FMA,   // Fused multiply-add: rd = (rs1 * rs2) + rs3
  FMS,   // Fused multiply-subtract: rd = (rs1 * rs2) - rs3
  FNMA,  // Fused negative multiply-add: rd = -(rs1 * rs2) + rs3
  FNMS,  // Fused negative multiply-subtract: rd = -(rs1 * rs2) - rs3

  // Comparison operations
  FCMP,  // Floating-point comparison
  FMIN,  // Minimum
  FMAX,  // Maximum

  // Conversion operations
  FCVT_W_S,   // Convert float to signed integer
  FCVT_WU_S,  // Convert float to unsigned integer
  FCVT_S_W,   // Convert signed integer to float
  FCVT_S_WU,  // Convert unsigned integer to float

  // Pass-through for testing
  PASS  // Pass operand A
};

/**
 * @brief FPU Command
 *
 * Contains FPU operation type and destination register address
 * Similar to Coral NPU's FpuCmd structure
 */
class FPUCmd {
 public:
  FPUCmd(FPUOp op = FPUOp::FADD, uint32_t waddr = 0) : op_(op), waddr_(waddr) {}

  FPUOp getOp() const { return op_; }
  uint32_t getWAddr() const { return waddr_; }

  void setOp(FPUOp op) { op_ = op; }
  void setWAddr(uint32_t addr) { waddr_ = addr; }

 private:
  FPUOp op_;        // FPU operation type
  uint32_t waddr_;  // Write address (5 bits for RISC-V)
};

/**
 * @brief FPU Data Packet
 *
 * Contains three floating-point operands (for FMA operations)
 * and the operation code
 */
class FPUDataPacket : public Architecture::DataPacket {
 public:
  FPUDataPacket(float ina, float inb, float inc, FPUOp op)
      : ina_(ina), inb_(inb), inc_(inc), op_(op) {}

  float getInputA() const { return ina_; }
  float getInputB() const { return inb_; }
  float getInputC() const { return inc_; }
  FPUOp getOperation() const { return op_; }

  void setInputA(float value) { ina_ = value; }
  void setInputB(float value) { inb_ = value; }
  void setInputC(float value) { inc_ = value; }
  void setOperation(FPUOp op) { op_ = op; }

  std::shared_ptr<Architecture::DataPacket> clone() const override {
    auto cloned = std::make_shared<FPUDataPacket>(ina_, inb_, inc_, op_);
    cloned->setTimestamp(timestamp_);
    cloned->setValid(valid_);
    return cloned;
  }

 private:
  float ina_;  // Input A (rs1)
  float inb_;  // Input B (rs2)
  float inc_;  // Input C (rs3, for FMA operations)
  FPUOp op_;   // Operation code
};

/**
 * @brief FPU Component (Floating-point Processing Unit)
 *
 * Implements floating-point operations with multiple pipeline stages:
 * - Stage 0: Decode - parse command
 * - Stage 1: Execute1 - FMA stage 1
 * - Stage 2: Execute2 - FMA stage 2
 * - Stage 3: Execute3 - FMA stage 3 / rounding
 * - Stage 4: Writeback - write result
 *
 * Similar to Coral NPU's FPU with 5-stage pipeline
 */
class FPUComponent : public PipelineComponent {
 public:
  static constexpr size_t pipeline_stages = 5;  // 5-stage pipeline

  FPUComponent(const std::string& name, EventDriven::EventScheduler& scheduler,
               uint64_t period)
      : PipelineComponent(name, scheduler, period, pipeline_stages),
        operations_executed_(0) {
    setupPipeline();
  }

 private:
  void setupPipeline() {
    // Stage 0: Decode - just pass through
    setStageFunction(0, [this](std::shared_ptr<Architecture::DataPacket> data) {
      return data;  // No processing needed in decode stage
    });

    // Stage 1: Execute1 - FMA stage 1 (multiplication)
    setStageFunction(1, [this](std::shared_ptr<Architecture::DataPacket> data) {
      return data;  // Pass through, actual computation in stage 3
    });

    // Stage 2: Execute2 - FMA stage 2 (addition preparation)
    setStageFunction(2, [this](std::shared_ptr<Architecture::DataPacket> data) {
      return data;  // Pass through
    });

    // Stage 3: Execute3 - FMA stage 3 (final computation and rounding)
    setStageFunction(3, [this](std::shared_ptr<Architecture::DataPacket> data) {
      return executeStage(data);
    });

    // Stage 4: Writeback - just pass through
    setStageFunction(4, [this](std::shared_ptr<Architecture::DataPacket> data) {
      return data;
    });
  }

  std::shared_ptr<Architecture::DataPacket> executeStage(
      std::shared_ptr<Architecture::DataPacket> data) {
    auto fpu_data = std::dynamic_pointer_cast<FPUDataPacket>(data);
    if (fpu_data) {
      float ina = fpu_data->getInputA();
      float inb = fpu_data->getInputB();
      float inc = fpu_data->getInputC();
      FPUOp op = fpu_data->getOperation();

      float result = executeOperation(ina, inb, inc, op);
      operations_executed_++;

      TRACE_COMPUTE(scheduler_.getCurrentTime(), getName(), getOpName(op),
                    "a=" << ina << " b=" << inb << " c=" << inc << " result="
                         << result << " | ops=" << operations_executed_);

      // Create result packet
      int float_as_int;
      std::memcpy(&float_as_int, &result, sizeof(float));
      auto result_packet =
          std::make_shared<Architecture::IntDataPacket>(float_as_int);
      result_packet->setTimestamp(scheduler_.getCurrentTime());
      return result_packet;
    }
    return data;
  }

 public:
  /**
   * @brief Execute FPU operation
   *
   * Converts operations similar to Coral NPU:
   * - FADD: b = 1.0, c = 0 -> (a * 1) + 0
   * - FSUB: b = 1.0, c = -rs2 -> (a * 1) - rs2
   * - FMUL: c = 0 -> (a * b) + 0
   * - FMA: -> (a * b) + c
   * - FMS: -> (a * b) - c
   * - FNMA: -> -(a * b) + c
   * - FNMS: -> -(a * b) - c
   */
  static float executeOperation(float ina, float inb, float inc, FPUOp op) {
    // Direct handling for basic operations first
    switch (op) {
      case FPUOp::FADD:
        // a + b
        return ina + inb;

      case FPUOp::FSUB:
        // a - b
        return ina - inb;

      case FPUOp::FMUL:
        // a * b
        return ina * inb;

      case FPUOp::FMA:
        // (a * b) + c
        return std::fma(ina, inb, inc);

      case FPUOp::FMS:
        // (a * b) - c
        return std::fma(ina, inb, -inc);

      case FPUOp::FNMA:
        // -(a * b) + c = (-a * b) + c
        return std::fma(-ina, inb, inc);

      case FPUOp::FNMS:
        // -(a * b) - c = (-a * b) - c
        return std::fma(-ina, inb, -inc);

      case FPUOp::FMIN:
        return (ina < inb) ? ina : inb;
      case FPUOp::FMAX:
        return (ina > inb) ? ina : inb;

      case FPUOp::FCVT_W_S:
        // Convert float to signed 32-bit integer
        return static_cast<float>(static_cast<int32_t>(ina));
      case FPUOp::FCVT_WU_S:
        // Convert float to unsigned 32-bit integer
        return static_cast<float>(static_cast<uint32_t>(ina));
      case FPUOp::FCVT_S_W:
        // Convert signed 32-bit integer to float
        return ina;  // Already in float form
      case FPUOp::FCVT_S_WU:
        // Convert unsigned 32-bit integer to float
        return ina;  // Already in float form

      case FPUOp::FCMP:
        // Comparison: return 1.0 if equal, 0.0 otherwise
        return (ina == inb) ? 1.0f : 0.0f;

      case FPUOp::PASS:
        return ina;

      default:
        return 0.0f;
    }
  }

 public:
  /**
   * @brief Get operation name as string
   */
  static std::string getOpName(FPUOp op) {
    switch (op) {
      case FPUOp::FADD:
        return "FADD";
      case FPUOp::FSUB:
        return "FSUB";
      case FPUOp::FMUL:
        return "FMUL";
      case FPUOp::FMA:
        return "FMA";
      case FPUOp::FMS:
        return "FMS";
      case FPUOp::FNMA:
        return "FNMA";
      case FPUOp::FNMS:
        return "FNMS";
      case FPUOp::FCMP:
        return "FCMP";
      case FPUOp::FMIN:
        return "FMIN";
      case FPUOp::FMAX:
        return "FMAX";
      case FPUOp::FCVT_W_S:
        return "FCVT.W.S";
      case FPUOp::FCVT_WU_S:
        return "FCVT.WU.S";
      case FPUOp::FCVT_S_W:
        return "FCVT.S.W";
      case FPUOp::FCVT_S_WU:
        return "FCVT.S.WU";
      case FPUOp::PASS:
        return "PASS";
      default:
        return "UNKNOWN";
    }
  }

  /**
   * @brief Get operation symbol
   */
  static std::string getOpSymbol(FPUOp op) {
    switch (op) {
      case FPUOp::FADD:
        return "+f";
      case FPUOp::FSUB:
        return "-f";
      case FPUOp::FMUL:
        return "*f";
      case FPUOp::FMA:
        return "fma";
      case FPUOp::FMS:
        return "fms";
      case FPUOp::FNMA:
        return "-fma";
      case FPUOp::FNMS:
        return "-fms";
      case FPUOp::FMIN:
        return "min";
      case FPUOp::FMAX:
        return "max";
      case FPUOp::FCMP:
        return "==";
      case FPUOp::FCVT_W_S:
        return "cvt.w";
      case FPUOp::FCVT_WU_S:
        return "cvt.wu";
      case FPUOp::FCVT_S_W:
        return "cvt.s";
      case FPUOp::FCVT_S_WU:
        return "cvt.su";
      default:
        return "?";
    }
  }

  // Statistics
  uint64_t getOperationsExecuted() const { return operations_executed_; }

  void printStatistics() const {
    PipelineComponent::printStatistics();
    std::cout << "Pipeline stages: " << pipeline_stages << std::endl;
    std::cout << "Floating-point operations executed: " << operations_executed_
              << std::endl;
  }

 private:
  uint64_t operations_executed_;
};

#endif  // FPU_H
