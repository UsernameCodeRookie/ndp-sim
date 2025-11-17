#ifndef MLU_H
#define MLU_H

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "../../packet.h"
#include "../../pipeline.h"

namespace Architecture {

/**
 * @brief MLU (Multiplier) result packet
 */
class MLUResultPacket : public DataPacket {
 public:
  MLUResultPacket(uint32_t val = 0, uint32_t rd = 0) : value(val), rd(rd) {}

  std::shared_ptr<DataPacket> clone() const override {
    return cloneImpl<MLUResultPacket>(value, rd);
  }

  uint32_t value;
  uint32_t rd;  // Destination register
};

}  // namespace Architecture

/**
 * @brief MLU (Multiplication Logic Unit) Component
 *
 * A 3-stage pipelined multiplier unit that performs signed and unsigned
 * multiplication operations. Inspired by Coral NPU's MLU design.
 *
 * Stages:
 * - Stage 0: Arbitration and decode (select instruction from multiple lanes)
 * - Stage 1: Multiplication computation (full 32-bit × 32-bit → 66-bit)
 * - Stage 2: Result formatting and output (select low or high bits based on op)
 *
 * Operations:
 * - MUL:    32-bit × 32-bit unsigned → lower 32 bits
 * - MULH:   32-bit × 32-bit signed → upper 32 bits
 * - MULHSU: 32-bit signed × 32-bit unsigned → upper 32 bits
 * - MULHU:  32-bit × 32-bit unsigned → upper 32 bits
 *
 * Inherits from PipelineComponent to reuse pipeline infrastructure
 */
class MultiplyUnit : public Pipeline {
 public:
  enum class MulOp : uint8_t {
    MUL = 0,
    MULH = 1,
    MULHSU = 2,
    MULHU = 3,
  };

  /**
   * @brief MLU command packet containing operation and destination address
   */
  struct MluCmd {
    uint32_t rd_addr;
    MulOp op;
    int32_t rs1;
    int32_t rs2;

    MluCmd() : rd_addr(0), op(MulOp::MUL), rs1(0), rs2(0) {}
    MluCmd(uint32_t addr, MulOp operation, int32_t val1, int32_t val2)
        : rd_addr(addr), op(operation), rs1(val1), rs2(val2) {}
  };

  /**
   * @brief MLU stage data packet
   */
  struct MluData : public Architecture::DataPacket {
    uint32_t rd_addr;
    MulOp op;
    int64_t product;  // 66-bit product
    uint32_t result;  // Final 32-bit result

    MluData(uint32_t addr = 0, MulOp op_code = MulOp::MUL, int64_t prod = 0)
        : rd_addr(addr), op(op_code), product(prod), result(0) {}

    std::shared_ptr<Architecture::DataPacket> clone() const override {
      return cloneImpl<MluData>(rd_addr, op, product);
    }
  };

  MultiplyUnit(const std::string& name, EventDriven::EventScheduler& scheduler,
               uint64_t period, uint32_t num_lanes = 4)
      : Pipeline(name, scheduler, period, 3),  // Default latency=0
        num_lanes_(num_lanes),
        requests_processed_(0),
        results_output_(0) {
    createPorts();
    setupPipelineStages();
  }

  virtual ~MultiplyUnit() = default;

  /**
   * @brief Process a multiplication request
   */
  void processRequest(uint32_t rd_addr, MulOp op, int32_t rs1, int32_t rs2) {
    auto cmd_packet = std::make_shared<MluData>(rd_addr, op, 0);
    cmd_packet->rd_addr = rd_addr;
    cmd_packet->op = op;

    // Perform multiplication immediately for pipeline input
    int64_t rs1_ext, rs2_ext;
    bool rs1_signed = (op == MulOp::MULH || op == MulOp::MULHSU);
    bool rs2_signed = (op == MulOp::MULH);

    rs1_ext = rs1_signed ? static_cast<int64_t>(rs1)
                         : static_cast<int64_t>(rs1 & 0xFFFFFFFFU);
    rs2_ext = rs2_signed ? static_cast<int64_t>(rs2)
                         : static_cast<int64_t>(rs2 & 0xFFFFFFFFU);

    cmd_packet->product = rs1_ext * rs2_ext;

    requests_processed_++;

    auto in_port = getPort("in");
    in_port->write(
        std::static_pointer_cast<Architecture::DataPacket>(cmd_packet));
  }

  uint32_t getRequestsProcessed() const { return requests_processed_; }
  uint32_t getResultsOutput() const { return results_output_; }

  void printStatistics() const {
    Pipeline::printStatistics();
    std::cout << "MLU Requests processed: " << requests_processed_ << std::endl;
    std::cout << "MLU Results output: " << results_output_ << std::endl;
  }

 private:
  void createPorts() {
    // Create multi-lane request ports
    for (uint32_t i = 0; i < num_lanes_; i++) {
      addPort("req_" + std::to_string(i), Architecture::PortDirection::INPUT);
    }

    // Create operand ports
    for (uint32_t i = 0; i < num_lanes_; i++) {
      addPort("rs1_" + std::to_string(i), Architecture::PortDirection::INPUT);
      addPort("rs2_" + std::to_string(i), Architecture::PortDirection::INPUT);
    }

    // Result output port is inherited from PipelineComponent as "out"
  }

  void setupPipelineStages() {
    // Stage 0: Read input port and feed into pipeline
    setStageFunction(0, [this](std::shared_ptr<Architecture::DataPacket> data) {
      // First, try to read from input port (from dispatch)
      auto in_port = getPort("in");
      if (in_port && in_port->hasData()) {
        auto input_data = in_port->read();
        if (input_data) {
          requests_processed_++;
          return input_data;  // Feed into pipeline
        }
      }
      return data;  // Pass through if no new input
    });

    // Stage 1: Multiplication is done in processRequest
    setStageFunction(1, [](std::shared_ptr<Architecture::DataPacket> data) {
      return data;  // Multiplication already computed
    });

    // Stage 2: Result formatting
    // Compute result based on operation, but DO NOT write to port
    // Pipeline::tick() will write the stage data to out port automatically
    setStageFunction(2, [this](std::shared_ptr<Architecture::DataPacket> data) {
      auto mlu_data = std::dynamic_pointer_cast<MluData>(data);
      if (mlu_data) {
        // Compute result based on operation
        uint32_t result = 0;
        switch (mlu_data->op) {
          case MulOp::MUL:
            result = static_cast<uint32_t>(mlu_data->product & 0xFFFFFFFFUL);
            break;
          case MulOp::MULH:
          case MulOp::MULHSU:
          case MulOp::MULHU:
            result =
                static_cast<uint32_t>((mlu_data->product >> 32) & 0xFFFFFFFFUL);
            break;
        }

        // Update result in place
        mlu_data->result = result;
        results_output_++;

        TRACE_EVENT(scheduler_.getCurrentTime(), name_, "MLU_OUTPUT",
                    "rd_addr=0x" << std::hex << mlu_data->rd_addr << std::dec
                                 << " op=" << static_cast<int>(mlu_data->op)
                                 << " result=0x" << std::hex << result
                                 << std::dec);
      }
      return data;
      // NOTE: Pipeline::tick() will automatically write this data to out port
      // via stages_[num_stages_ - 1].valid && out->write(processed_data)
    });
  }

  uint32_t num_lanes_;
  uint32_t requests_processed_;
  uint32_t results_output_;
};

#endif  // MLU_H
