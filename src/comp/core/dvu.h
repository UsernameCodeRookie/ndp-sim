#ifndef DVU_H
#define DVU_H

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "../../pipeline.h"

/**
 * @brief DVU (Division/Remainder Logic Unit) Component
 *
 * A pipelined divider that performs signed and unsigned division operations.
 * Inspired by Coral NPU's DVU design.
 *
 * Operations:
 * - DIV:   32-bit signed division
 * - DIVU:  32-bit unsigned division
 * - REM:   32-bit signed remainder
 * - REMU:  32-bit unsigned remainder
 *
 * Algorithm:
 * Uses iterative bit-by-bit division (restoring division algorithm).
 * Uses stage stall predicates to keep data in a stage until computation
 * completes.
 * - Stage 0: Decode and validation
 * - Stage 1: Multi-cycle division computation (stalled until done)
 * - Stage 2: Result formatting and output
 *
 * Inherits from PipelineComponent to reuse pipeline infrastructure.
 */
class DivideUnit : public Pipeline {
 public:
  enum class DivOp : uint8_t {
    DIV = 0,   // Signed division
    DIVU = 1,  // Unsigned division
    REM = 2,   // Signed remainder
    REMU = 3,  // Unsigned remainder
  };

  /**
   * @brief DVU stage data packet with embedded state machine
   */
  struct DvuData : public Architecture::DataPacket {
    uint32_t rd_addr;
    DivOp op;
    int32_t dividend;
    int32_t divisor;
    bool div_by_zero;
    bool dividend_neg;
    bool divisor_neg;
    uint32_t quotient;
    uint32_t remainder;
    uint32_t iteration;
    bool computation_done;
    uint32_t result;

    DvuData(uint32_t addr = 0, DivOp op_code = DivOp::DIV, int32_t div = 0,
            int32_t divisor_val = 0)
        : rd_addr(addr),
          op(op_code),
          dividend(div),
          divisor(divisor_val),
          div_by_zero(divisor_val == 0),
          dividend_neg(div < 0),
          divisor_neg(divisor_val < 0),
          quotient(0),
          remainder(0),
          iteration(0),
          computation_done(false),
          result(0) {}

    std::shared_ptr<Architecture::DataPacket> clone() const override {
      return cloneImpl<DvuData>(rd_addr, op, dividend, divisor);
    }
  };

  DivideUnit(const std::string& name, EventDriven::EventScheduler& scheduler,
             uint64_t period, uint32_t num_lanes = 4)
      : Pipeline(name, scheduler, period, 3),
        num_lanes_(num_lanes),
        requests_processed_(0),
        results_output_(0),
        div_by_zero_count_(0) {
    createPorts();
    setupPipelineStages();
  }

  /**
   * @brief Submit a division request
   */
  void processRequest(uint32_t rd_addr, DivOp op, int32_t dividend,
                      int32_t divisor) {
    auto cmd = std::make_shared<DvuData>(rd_addr, op, dividend, divisor);
    auto in_port = getPort("in");
    if (in_port) {
      in_port->write(cmd);
      requests_processed_++;
    }
  }

  uint64_t getRequestsProcessed() const { return requests_processed_; }
  uint64_t getResultsOutput() const { return results_output_; }
  uint64_t getDivByZeroCount() const { return div_by_zero_count_; }

  void printStatistics() const {
    std::cout << "\n=== Pipeline Statistics: " << name_ << " ===" << std::endl;
    std::cout << "Number of stages: " << num_stages_ << std::endl;
    std::cout << "Total processed: " << total_processed_ << std::endl;
    std::cout << "Total stalls: " << total_stalls_ << std::endl;
    std::cout << "Current occupancy: " << getOccupancy() << "/" << num_stages_
              << std::endl;
    std::cout << "Pipeline full: " << (isFull() ? "Yes" : "No") << std::endl;
    std::cout << "Pipeline empty: " << (isEmpty() ? "Yes" : "No") << std::endl;
    std::cout << "DVU Requests processed: " << requests_processed_ << std::endl;
    std::cout << "DVU Results output: " << results_output_ << std::endl;
    std::cout << "DVU Div-by-zero: " << div_by_zero_count_ << std::endl;
  }

 private:
  uint32_t num_lanes_;
  uint64_t requests_processed_;
  uint64_t results_output_;
  uint64_t div_by_zero_count_;

  void createPorts() {
    if (!getPort("in")) {
      addPort("in", Architecture::PortDirection::INPUT);
    }
    if (!getPort("out")) {
      addPort("out", Architecture::PortDirection::OUTPUT);
    }
  }

  /**
   * @brief Perform one bit-iteration of division
   */
  static std::pair<uint32_t, uint32_t> divideBitIteration(
      uint32_t prev_quotient, uint32_t prev_remainder, uint32_t divisor,
      uint32_t dividend_bit) {
    uint32_t shifted_remainder = (prev_remainder << 1) | dividend_bit;
    if (shifted_remainder >= divisor) {
      return {(prev_quotient << 1) | 1, shifted_remainder - divisor};
    } else {
      return {(prev_quotient << 1) | 0, shifted_remainder};
    }
  }

  void setupPipelineStages() {
    // Stage 0: Decode and initialization
    setStageFunction(0, [this](std::shared_ptr<Architecture::DataPacket> data) {
      auto dvu_data = std::dynamic_pointer_cast<DvuData>(data);
      if (!dvu_data) return data;

      // Check for division by zero
      if (dvu_data->divisor == 0) {
        dvu_data->div_by_zero = true;
        div_by_zero_count_++;
        dvu_data->result = 0;
        dvu_data->iteration = 4;  // Mark as done
        dvu_data->computation_done = true;
        return std::static_pointer_cast<Architecture::DataPacket>(dvu_data);
      }

      // Check if signed operation
      bool is_signed =
          (dvu_data->op == DivOp::DIV || dvu_data->op == DivOp::REM);

      // Convert to unsigned for processing
      if (is_signed) {
        dvu_data->dividend =
            dvu_data->dividend_neg ? -dvu_data->dividend : dvu_data->dividend;
        dvu_data->divisor =
            dvu_data->divisor_neg ? -dvu_data->divisor : dvu_data->divisor;
      }

      dvu_data->quotient = 0;
      dvu_data->remainder = 0;
      dvu_data->iteration = 0;
      dvu_data->computation_done = false;

      return std::static_pointer_cast<Architecture::DataPacket>(dvu_data);
    });

    // Stage 1: Multi-cycle division computation
    setStageFunction(1, [this](std::shared_ptr<Architecture::DataPacket> data) {
      auto dvu_data = std::dynamic_pointer_cast<DvuData>(data);
      if (!dvu_data || dvu_data->div_by_zero) {
        dvu_data->computation_done = true;
        return std::static_pointer_cast<Architecture::DataPacket>(dvu_data);
      }

      if (dvu_data->computation_done) {
        return std::static_pointer_cast<Architecture::DataPacket>(dvu_data);
      }

      // Perform 8 bit-iterations per cycle
      const uint32_t BITS_PER_CYCLE = 8;
      uint32_t start_bit = dvu_data->iteration * BITS_PER_CYCLE;

      for (uint32_t i = 0; i < BITS_PER_CYCLE && start_bit + i < 32; i++) {
        uint32_t bit_index = 31 - (start_bit + i);
        uint32_t bit = (dvu_data->dividend >> bit_index) & 1;

        auto [new_q, new_r] = divideBitIteration(
            dvu_data->quotient, dvu_data->remainder, dvu_data->divisor, bit);
        dvu_data->quotient = new_q;
        dvu_data->remainder = new_r;
      }

      dvu_data->iteration++;

      // Mark as done after 4 iterations (32 bits total)
      if (dvu_data->iteration >= 4) {
        dvu_data->computation_done = true;
      }

      return std::static_pointer_cast<Architecture::DataPacket>(dvu_data);
    });

    // Set stage 1 stall predicate: stall if computation not done
    setStageStallPredicate(
        1, [](std::shared_ptr<Architecture::DataPacket> data) {
          auto dvu_data = std::dynamic_pointer_cast<DvuData>(data);
          if (!dvu_data) return false;
          // Stall (return true) if computation is not done
          return !dvu_data->computation_done;
        });

    // Stage 2: Result formatting
    setStageFunction(2, [this](std::shared_ptr<Architecture::DataPacket> data) {
      auto dvu_data = std::dynamic_pointer_cast<DvuData>(data);
      if (!dvu_data)
        return std::static_pointer_cast<Architecture::DataPacket>(data);

      if (dvu_data->div_by_zero) {
        dvu_data->result = 0xFFFFFFFFU;
        results_output_++;
        return std::static_pointer_cast<Architecture::DataPacket>(dvu_data);
      }

      // Division complete, format result
      bool is_signed =
          (dvu_data->op == DivOp::DIV || dvu_data->op == DivOp::REM);
      bool return_quotient =
          (dvu_data->op == DivOp::DIV || dvu_data->op == DivOp::DIVU);

      uint32_t raw_result =
          return_quotient ? dvu_data->quotient : dvu_data->remainder;

      // Apply sign correction if needed
      if (is_signed) {
        if (return_quotient) {
          // Quotient sign = dividend_sign XOR divisor_sign
          if (dvu_data->dividend_neg != dvu_data->divisor_neg &&
              raw_result != 0) {
            raw_result = (~raw_result) + 1;
          }
        } else {
          // Remainder sign = dividend sign
          if (dvu_data->dividend_neg && raw_result != 0) {
            raw_result = (~raw_result) + 1;
          }
        }
      }

      dvu_data->result = raw_result;
      results_output_++;

      return std::static_pointer_cast<Architecture::DataPacket>(dvu_data);
    });
  }
};

#endif  // DVU_H
