#ifndef CORE_H
#define CORE_H

#include <algorithm>
#include <cstring>
#include <memory>
#include <queue>
#include <string>
#include <vector>

#include "comp/alu.h"
#include "comp/bru.h"
#include "comp/core/decode.h"
#include "comp/core/dispatch.h"
#include "comp/dvu.h"
#include "comp/fpu.h"
#include "comp/lsu.h"
#include "comp/mlu.h"
#include "comp/pipeline.h"
#include "comp/regfile.h"
#include "conn/credit.h"
#include "conn/ready_valid.h"
#include "conn/wire.h"
#include "packet.h"
#include "port.h"
#include "scheduler.h"
#include "trace.h"

namespace Architecture {

/**
 * @brief Coral NPU Scalar Core (SCore)
 *
 * Enhanced scalar core implementation for Coral NPU with integrated Decode &
 * Dispatch. Now inherits from PipelineComponent to model the 3-stage
 * instruction pipeline:
 * - Stage 0: Fetch/Decode - Pull instruction from buffer, decode fields
 * - Stage 1: Dispatch - Check hazards, issue to execution units
 * - Stage 2: Execute/Writeback - Monitor execution units, writeback results
 *
 * Simplified scalar core implementation for Coral NPU based on event-driven
 * architecture. This version omits:
 * - Retirement buffer
 * - Debug module and fault manager
 * - Floating-point extension (RV32F)
 * - Vector extension (RVV)
 *
 * Architecture Overview:
 * - Fetch stage: Instruction fetching (simplified, pre-decoded instructions)
 * - Dispatch stage: Instruction decode and issue to functional units
 * - Execution units: ALU, BRU, MLU, DVU, LSU (with 3-stage pipelines)
 * - Register file: Multi-port register storage with scoreboard
 * - Interconnects: Wire and Ready-Valid connections between units
 *
 * Functional Units (3-stage pipelines):
 * - ALU: Arithmetic and Logic Unit (integer operations)
 * - BRU: Branch Resolution Unit (branch/jump operations)
 * - MLU: Multiply Unit (multiplication operations)
 * - DVU: Divide Unit (division operations)
 * - LSU: Load-Store Unit (memory operations)
 * - Register File: Dual-ported register storage
 *
 * Instruction Flow:
 * 1. Fetch stage pulls instructions from fetch buffer
 * 2. Decode stage decodes instruction words
 * 3. Dispatch stage issues commands with Coral NPU rules:
 *    - In-order: don't skip instructions
 *    - Scoreboarding: RAW/WAW hazard detection
 *    - Resource constraints: MLU/DVU/LSU = 1 per cycle
 *    - Control flow: don't dispatch past branches
 *    - Special instructions: CSR/FENCE in slot 0 only
 * 4. Execution units execute in parallel
 * 5. Results write back to register file
 * 6. Ready-Valid connections provide flow control
 */
class SCore : public Pipeline {
 public:
  /**
   * @brief SCore configuration parameters
   */
  struct Config {
    // Instruction dispatch parameters
    uint32_t num_instruction_lanes;  // Number of parallel decode/dispatch lanes
    // Register file parameters
    uint32_t num_registers;       // Number of general-purpose registers
    uint32_t num_read_ports;      // Total read ports
    uint32_t num_write_ports;     // Total write ports
    bool use_regfile_forwarding;  // Enable write-through forwarding
    // Functional unit parameters
    uint32_t alu_period;      // ALU tick period (cycles)
    uint32_t bru_period;      // BRU tick period
    uint32_t mlu_period;      // MLU tick period (3 cycles)
    uint32_t dvu_period;      // DVU tick period (8 cycles)
    uint32_t lsu_period;      // LSU tick period
    uint32_t regfile_period;  // Register file tick period
    // Connection parameters
    uint32_t connection_latency;  // Wire connection latency (cycles)
    uint32_t buffer_size;         // Ready-Valid buffer size
    uint64_t start_time;          // Start time (simulation cycles)

    Config()
        : num_instruction_lanes(2),
          num_registers(32),
          num_read_ports(16),
          num_write_ports(8),
          use_regfile_forwarding(true),
          alu_period(1),
          bru_period(1),
          mlu_period(3),
          dvu_period(8),
          lsu_period(1),
          regfile_period(1),
          connection_latency(0),
          buffer_size(2),
          start_time(0) {}
  };

  /**
   * @brief Constructor
   * @param name Component name (e.g., "SCore_0")
   * @param scheduler Event scheduler reference
   * @param config Core configuration
   */
  SCore(const std::string& name, EventDriven::EventScheduler& scheduler,
        const Config& config = Config())
      : Pipeline(name, scheduler, 1, 3),  // 3-stage pipeline with period 1
        config_(config),
        dispatch_ctrl_(name + "_DispatchCtrl", config.num_registers,
                       config.num_instruction_lanes),
        instructions_dispatched_(0),
        instructions_retired_(0),
        branch_taken_(false),
        pc_(0),
        dispatch_ready_(true),
        scoreboard_(config.num_registers, false),
        mlu_busy_(false),
        dvu_busy_(false),
        lsu_busy_(false) {
    // Initialize all functional units
    initializeFunctionalUnits();

    // Create register file
    RegisterFileParameters rf_params(
        config_.num_registers, config_.num_read_ports, config_.num_write_ports,
        config_.num_instruction_lanes, 32,
        true,  // use_scoreboard
        config_.use_regfile_forwarding,
        false  // use_debug_module
    );
    regfile_ =
        std::make_shared<RegisterFile>("RegisterFile", scheduler, rf_params);

    // Create connections
    createConnections();

    // Setup pipeline stage functions for the 3-stage instruction pipeline
    setupPipelineStages();

    // Initialize dispatch ready
    dispatch_ready_ = true;
  }

  virtual ~SCore() = default;

  /**
   * @brief Initialize all functional units
   *
   * Creates instances of ALU, BRU, MLU, DVU, and LSU with configured
   * parameters
   */
  void initializeFunctionalUnits() {
    // Create ALU (2 instances for dual-lane issue)
    for (uint32_t i = 0; i < config_.num_instruction_lanes; i++) {
      std::string alu_name = name_ + "_ALU_" + std::to_string(i);
      alusv_.push_back(std::make_shared<ArithmeticLogicUnit>(
          alu_name, scheduler_, config_.alu_period));
    }

    // Create BRU (1 instance, sequential branch execution)
    bru_ = std::make_shared<BranchUnit>(name_ + "_BRU", scheduler_,
                                        config_.bru_period);

    // Create MLU (1 instance)
    mlu_ = std::make_shared<MultiplyUnit>(name_ + "_MLU", scheduler_,
                                          config_.mlu_period);

    // Create DVU (1 instance)
    dvu_ = std::make_shared<DivideUnit>(name_ + "_DVU", scheduler_,
                                        config_.dvu_period);

    // Create LSU (1 instance)
    lsu_ = std::make_shared<LoadStoreUnit>(name_ + "_LSU", scheduler_,
                                           config_.lsu_period);
  }

  /**
   * @brief Create and connect all data flow connections
   *
   * Establishes wire and ready-valid connections between:
   * - Dispatch -> Functional units (command channels)
   * - Functional units -> Register file (result writeback)
   * - Register file -> Dispatch (read data feedback)
   * - Control signals (stall, flush)
   */
  void createConnections() {
    // Register file read ports (from dispatch)
    // Each instruction lane needs 2 read ports (rs1, rs2)
    // These use simple wire connections for low-latency access
    for (uint32_t i = 0; i < config_.num_instruction_lanes; i++) {
      std::string conn_name =
          name_ + "_RF_Read_Lane" + std::to_string(i) + "_RS1";
      auto conn =
          std::make_shared<Wire>(conn_name, scheduler_, config_.regfile_period);
      conn->setLatency(config_.connection_latency);
      regfile_connections_.push_back(conn);
    }

    // ALU result write ports -> Register file (wire connection)
    for (uint32_t i = 0; i < config_.num_instruction_lanes; i++) {
      std::string conn_name = name_ + "_ALU_WB_" + std::to_string(i);
      auto conn =
          std::make_shared<Wire>(conn_name, scheduler_, config_.regfile_period);
      conn->setLatency(config_.connection_latency);
      alu_wb_connections_.push_back(conn);
    }

    // BRU result write port -> Register file
    {
      std::string conn_name = name_ + "_BRU_WB";
      auto conn =
          std::make_shared<Wire>(conn_name, scheduler_, config_.regfile_period);
      conn->setLatency(config_.connection_latency);
      bru_wb_connection_ = conn;
    }

    // MLU result write port -> Register file
    {
      std::string conn_name = name_ + "_MLU_WB";
      auto conn =
          std::make_shared<Wire>(conn_name, scheduler_, config_.regfile_period);
      conn->setLatency(config_.connection_latency);
      mlu_wb_connection_ = conn;
    }

    // DVU result write port -> Register file
    {
      std::string conn_name = name_ + "_DVU_WB";
      auto conn =
          std::make_shared<Wire>(conn_name, scheduler_, config_.regfile_period);
      conn->setLatency(config_.connection_latency);
      dvu_wb_connection_ = conn;
    }

    // LSU result write port -> Register file
    {
      std::string conn_name = name_ + "_LSU_WB";
      auto conn =
          std::make_shared<Wire>(conn_name, scheduler_, config_.regfile_period);
      conn->setLatency(config_.connection_latency);
      lsu_wb_connection_ = conn;
    }
  }

  /**
   * @brief Get ALU instance by index
   */
  std::shared_ptr<ArithmeticLogicUnit> getALU(uint32_t index = 0) {
    if (index < alusv_.size()) {
      return alusv_[index];
    }
    return nullptr;
  }

  /**
   * @brief Get BRU instance
   */
  std::shared_ptr<BranchUnit> getBRU() { return bru_; }

  /**
   * @brief Get MLU instance
   */
  std::shared_ptr<MultiplyUnit> getMLU() { return mlu_; }

  /**
   * @brief Get DVU instance
   */
  std::shared_ptr<DivideUnit> getDVU() { return dvu_; }

  /**
   * @brief Get LSU instance
   */
  std::shared_ptr<LoadStoreUnit> getLSU() { return lsu_; }

  /**
   * @brief Get Register File instance
   */
  std::shared_ptr<RegisterFile> getRegisterFile() { return regfile_; }

  /**
   * @brief Get all ALU instances
   */
  const std::vector<std::shared_ptr<ArithmeticLogicUnit>>& getALUs() const {
    return alusv_;
  }

  /**
   * @brief Get all connections
   */
  const std::vector<std::shared_ptr<Wire>>& getRegfileConnections() const {
    return regfile_connections_;
  }

  const std::vector<std::shared_ptr<Wire>>& getALUWBConnections() const {
    return alu_wb_connections_;
  }

  /**
   * @brief Initialize core by starting all components
   *
   * Brings all functional units, register file, and connections online
   */
  void initialize() override {
    // Call Pipeline's initialize to start the pipeline ticking
    Pipeline::initialize();

    TRACE_EVENT(scheduler_.getCurrentTime(), getName(), "INITIALIZE",
                "Starting SCore initialization");

    // Start all functional units
    for (auto& alu : alusv_) {
      alu->start(config_.start_time);
    }
    bru_->start(config_.start_time);
    mlu_->start(config_.start_time);
    dvu_->start(config_.start_time);
    lsu_->start(config_.start_time);

    // Start all connections
    for (auto& conn : regfile_connections_) {
      conn->start(config_.start_time);
    }
    for (auto& conn : alu_wb_connections_) {
      conn->start(config_.start_time);
    }
    if (bru_wb_connection_) bru_wb_connection_->start(config_.start_time);
    if (mlu_wb_connection_) mlu_wb_connection_->start(config_.start_time);
    if (dvu_wb_connection_) dvu_wb_connection_->start(config_.start_time);
    if (lsu_wb_connection_) lsu_wb_connection_->start(config_.start_time);

    TRACE_EVENT(scheduler_.getCurrentTime(), getName(), "INITIALIZE_COMPLETE",
                "FUs and connections started");
  }

  /**
   * @brief Setup pipeline stage functions
   *
   * Configures the 3-stage instruction pipeline:
   * - Stage 0: Fetch/Decode - Extract instruction from buffer, decode fields
   * - Stage 1: Dispatch - Issue instruction to execution unit
   * - Stage 2: Execute/Writeback - Monitor completion
   */
  void setupPipelineStages() {
    // Stage 0: Fetch/Decode
    // Read from fetch buffer and decode the instruction
    setStageFunction(0, [this](std::shared_ptr<Architecture::DataPacket> data) {
      uint64_t current_time = scheduler_.getCurrentTime();

      // If we have instructions in fetch buffer, create a packet to drive the
      // pipeline
      if (!fetch_buffer_.empty()) {
        const auto& inst_word = fetch_buffer_[0];
        DecodedInstruction inst =
            DecodeStage::decode(inst_word.first, inst_word.second);

        // Create a packet containing the decoded instruction information
        auto decoded_packet =
            std::make_shared<Architecture::IntDataPacket>(inst.word);
        decoded_packet->timestamp = current_time;

        TRACE_COMPUTE(current_time, getName(), "STAGE0_DECODE",
                      "PC=0x" << std::hex << inst_word.first << " word=0x"
                              << inst_word.second << std::dec);

        return std::static_pointer_cast<Architecture::DataPacket>(
            decoded_packet);
      }

      // No instruction in buffer, return original data to avoid stalling
      // pipeline
      return data;
    });

    // Stage 1: Dispatch
    // Take decoded instruction and dispatch to execution unit
    setStageFunction(1, [this](std::shared_ptr<Architecture::DataPacket> data) {
      uint64_t current_time = scheduler_.getCurrentTime();

      // Dispatch cycle processes instructions from fetch buffer
      if (!fetch_buffer_.empty()) {
        uint32_t dispatched = dispatch();
        if (dispatched > 0) {
          TRACE_COMPUTE(current_time, getName(), "STAGE1_DISPATCH",
                        "Dispatched " << dispatched << " instructions, total="
                                      << instructions_dispatched_);
        } else if (!fetch_buffer_.empty()) {
          TRACE_COMPUTE(
              current_time, getName(), "STAGE1_STALL",
              "Cannot dispatch, fetch_buffer size=" << fetch_buffer_.size());
        }
      }
      return data;
    });

    // Stage 2: Execute/Writeback
    // Monitor execution unit completions and update statistics
    setStageFunction(2, [this](std::shared_ptr<Architecture::DataPacket> data) {
      uint64_t current_time = scheduler_.getCurrentTime();

      // Only count retirement if data is valid
      if (data) {
        instructions_retired_++;

        TRACE_COMPUTE(
            current_time, getName(), "STAGE2_WRITEBACK",
            "Retired " << 1 << " instruction, total=" << instructions_retired_);
      }

      return data;
    });
  }

  /**
   * @brief Reset core state
   *
   * Resets all components and clears pipeline
   */
  void reset() override {
    for (auto& alu : alusv_) {
      alu->reset();
    }
    bru_->reset();
    mlu_->reset();
    dvu_->reset();
    lsu_->reset();
    regfile_->reset();

    instructions_dispatched_ = 0;
    instructions_retired_ = 0;
  }

  /**
   * @brief Dispatch instruction command
   *
   * @param op Operation type (ALU, BRU, MLU, DVU, LSU)
   * @param lane Dispatch lane index (0-based)
   * @param operand1 First operand (or address)
   * @param operand2 Second operand
   * @param opcode Operation code (ALUOp, BruOp, etc.)
   * @param rd Destination register index
   *
   * Example:
   *   core->dispatchInstruction(OpType::ALU, 0, 5, 3, ALUOp::ADD, 8);
   */
  enum class OpType { ALU, BRU, MLU, DVU, LSU };

  void issue(OpType op_type, uint32_t lane, uint32_t operand1,
             uint32_t operand2, uint32_t opcode, uint32_t rd) {
    uint64_t current_time = scheduler_.getCurrentTime();

    if (!dispatch_ready_) {
      // Dispatch backpressure - implement flow control logic
      TRACE_COMPUTE(current_time, getName(), "DISPATCH_BACKPRESSURE",
                    "Dispatch blocked");
      return;
    }

    instructions_dispatched_++;

    switch (op_type) {
      case OpType::ALU:
        // Create ALU command and inject into ALU input
        if (lane < alusv_.size()) {
          auto alu = alusv_[lane];
          auto in_port = alu->getPort("in");
          if (in_port && !in_port->hasData()) {
            auto cmd = std::make_shared<ALUDataPacket>(
                static_cast<int32_t>(operand1), static_cast<int32_t>(operand2),
                static_cast<ALUOp>(opcode));
            cmd->timestamp = current_time;
            in_port->write(cmd);
            TRACE_COMPUTE(current_time, getName(), "DISPATCH_ALU",
                          "lane=" << lane << " rd=" << rd << " op1=" << operand1
                                  << " op2=" << operand2);
          }
        }
        break;

      case OpType::BRU:
        // Create BRU command and inject
        if (bru_) {
          auto in_port = bru_->getPort("in");
          if (in_port && !in_port->hasData()) {
            auto cmd = std::make_shared<BruCommandPacket>(
                operand1, operand2, static_cast<BruOp>(opcode), 0, 0, rd);
            cmd->timestamp = current_time;
            in_port->write(cmd);
            TRACE_COMPUTE(current_time, getName(), "DISPATCH_BRU",
                          "rd=" << rd << " operand=" << operand1);
          }
        }
        break;

      case OpType::MLU:
        // Create MLU command and inject
        if (mlu_) {
          auto in_port = mlu_->getPort("in");
          if (in_port && !in_port->hasData()) {
            auto cmd = std::make_shared<MultiplyUnit::MluData>(
                rd, static_cast<MultiplyUnit::MulOp>(opcode),
                static_cast<int64_t>(operand1) *
                    static_cast<int64_t>(operand2));
            cmd->timestamp = current_time;
            in_port->write(cmd);
            TRACE_COMPUTE(
                current_time, getName(), "DISPATCH_MLU",
                "rd=" << rd << " op1=" << operand1 << " op2=" << operand2);
          }
        }
        break;

      case OpType::DVU:
        // Create DVU command and inject
        if (dvu_) {
          auto in_port = dvu_->getPort("in");
          if (in_port && !in_port->hasData()) {
            auto cmd = std::make_shared<DivideUnit::DvuData>(
                rd, static_cast<DivideUnit::DivOp>(opcode),
                static_cast<int32_t>(operand1), static_cast<int32_t>(operand2));
            cmd->timestamp = current_time;
            in_port->write(cmd);
            TRACE_COMPUTE(
                current_time, getName(), "DISPATCH_DVU",
                "rd=" << rd << " op1=" << operand1 << " op2=" << operand2);
          }
        }
        break;

      case OpType::LSU:
        // Create LSU command and inject
        if (lsu_) {
          auto in_port = lsu_->getPort("in");
          if (in_port && !in_port->hasData()) {
            auto cmd = std::make_shared<MemoryRequestPacket>(
                static_cast<LSUOp>(opcode), operand1,
                static_cast<int32_t>(operand2), 1, 1, false);
            cmd->timestamp = current_time;
            in_port->write(cmd);
            TRACE_COMPUTE(current_time, getName(), "DISPATCH_LSU",
                          "addr=" << operand1 << " data=" << operand2);
          }
        }
        break;
    }
  }

  /**
   * @brief Inject instruction word into fetch buffer (for testing/simulation)
   *
   * Allows test code to pre-populate the instruction stream
   */
  void inject(uint32_t pc, uint32_t word) {
    fetch_buffer_.push_back({pc, word});
    TRACE_COMPUTE(scheduler_.getCurrentTime(), getName(), "INJECT_INSTRUCTION",
                  "PC=0x" << std::hex << pc << " word=0x" << word
                          << " buffer_size=" << std::dec
                          << fetch_buffer_.size());
  }

  /**
   * @brief Dispatch cycle - main decode/dispatch logic
   *
   * Implements Coral NPU dispatch rules:
   * 1. In-order dispatch (don't skip instructions)
   * 2. Scoreboard hazard detection (RAW, WAW)
   * 3. Resource constraints (MLU/DVU/LSU = 1 per cycle)
   * 4. Control flow (don't dispatch past branches)
   * 5. Special instruction constraints (CSR/FENCE in slot 0 only)
   *
   * Returns number of instructions dispatched this cycle
   */
  uint32_t dispatch() {
    uint64_t current_time = scheduler_.getCurrentTime();

    // Clear resource usage trackers for this cycle
    dispatch_ctrl_.clearResourceTrackers();
    mlu_busy_ = false;
    dvu_busy_ = false;
    lsu_busy_ = false;

    size_t fetch_index = 0;
    uint32_t dispatched_count = 0;

    TRACE_COMPUTE(current_time, getName(), "DISPATCH_CYCLE_START",
                  "fetch_buffer size=" << fetch_buffer_.size());

    // Try to dispatch up to num_instruction_lanes instructions
    for (uint32_t lane = 0; lane < config_.num_instruction_lanes &&
                            fetch_index < fetch_buffer_.size();
         lane++) {
      const auto& inst_word = fetch_buffer_[fetch_index];
      DecodedInstruction inst =
          DecodeStage::decode(inst_word.first, inst_word.second);

      // Check if instruction is valid
      if (inst.op_type == DecodedInstruction::OpType::INVALID) {
        TRACE_COMPUTE(current_time, getName(), "DISPATCH_INVALID",
                      "Invalid instruction at index=" << fetch_index);
        break;
      }

      // In-order rule: if this instruction couldn't dispatch, stop
      if (!canDispatch(inst, lane)) {
        TRACE_COMPUTE(current_time, getName(), "DISPATCH_BLOCKED",
                      "Cannot dispatch at lane=" << lane << ", rd=" << inst.rd);
        break;
      }

      // Dispatch the instruction to appropriate unit
      dispatchToUnit(inst, lane);

      TRACE_COMPUTE(current_time, getName(), "DISPATCH_SUCCESS",
                    "Dispatched to lane=" << lane << ", rd=" << inst.rd);

      // Update scoreboard with destination register
      dispatch_ctrl_.updateScoreboard(inst.rd);
      if (inst.rd != 0) {  // Register 0 is hardwired to 0
        scoreboard_[inst.rd] = true;
      }

      // Mark resource busy if needed
      dispatch_ctrl_.setResourceBusy(inst);
      switch (inst.op_type) {
        case DecodedInstruction::OpType::MLU:
          mlu_busy_ = true;
          break;
        case DecodedInstruction::OpType::DVU:
          dvu_busy_ = true;
          break;
        case DecodedInstruction::OpType::LSU:
          lsu_busy_ = true;
          break;
        default:
          break;
      }

      // Check if this is a control flow instruction
      if (isControlFlowInstruction(inst)) {
        branch_taken_ = true;
        fetch_index++;
        dispatched_count++;
        TRACE_COMPUTE(current_time, getName(), "DISPATCH_CONTROL_FLOW",
                      "Control flow instruction, stopping dispatch");
        break;  // Don't dispatch past branches
      }

      fetch_index++;
      dispatched_count++;
      instructions_dispatched_++;
    }

    // Remove dispatched instructions from fetch buffer
    if (fetch_index > 0) {
      fetch_buffer_.erase(fetch_buffer_.begin(),
                          fetch_buffer_.begin() + fetch_index);
      TRACE_COMPUTE(current_time, getName(), "DISPATCH_CYCLE_END",
                    "Dispatched " << dispatched_count
                                  << " instructions, fetch_buffer remaining="
                                  << fetch_buffer_.size());
    }

    return dispatched_count;
  }

  /**
   * @brief Check if instruction can be dispatched
   *
   * Implements hazard detection and resource constraints
   */
  bool canDispatch(const DecodedInstruction& inst, uint32_t lane) {
    return dispatch_ctrl_.canDispatch(inst, lane);
  }

  /**
   * @brief Dispatch instruction to appropriate execution unit
   */
  void dispatchToUnit(const DecodedInstruction& inst, uint32_t lane) {
    switch (inst.op_type) {
      case DecodedInstruction::OpType::ALU:
        dispatchToALU(inst, lane);
        break;
      case DecodedInstruction::OpType::BRU:
        dispatchToBRU(inst);
        break;
      case DecodedInstruction::OpType::MLU:
        dispatchToMLU(inst);
        mlu_busy_ = true;
        break;
      case DecodedInstruction::OpType::DVU:
        dispatchToDVU(inst);
        dvu_busy_ = true;
        break;
      case DecodedInstruction::OpType::LSU:
        dispatchToLSU(inst);
        lsu_busy_ = true;
        break;
      case DecodedInstruction::OpType::CSR:
      case DecodedInstruction::OpType::FENCE:
        // For simplified implementation, just track as executed
        break;
      default:
        break;
    }
  }

  /**
   * @brief Dispatch to ALU
   */
  void dispatchToALU(const DecodedInstruction& inst, uint32_t lane) {
    if (lane >= alusv_.size()) return;

    auto alu = alusv_[lane];
    auto in_port = alu->getPort("in");
    if (in_port && !in_port->hasData()) {
      // Get operands from register file
      uint32_t rs1_val = regfile_->readRegister(inst.rs1);
      uint32_t rs2_val = regfile_->readRegister(inst.rs2);

      auto cmd = std::make_shared<ALUDataPacket>(
          static_cast<int32_t>(rs1_val), static_cast<int32_t>(rs2_val),
          static_cast<ALUOp>(inst.opcode));
      cmd->timestamp = scheduler_.getCurrentTime();
      in_port->write(cmd);
    }
  }

  /**
   * @brief Dispatch to BRU
   */
  void dispatchToBRU(const DecodedInstruction& inst) {
    if (bru_) {
      auto in_port = bru_->getPort("in");
      if (in_port && !in_port->hasData()) {
        // Get operands from register file
        uint32_t rs1_val = regfile_->readRegister(inst.rs1);
        uint32_t rs2_val = regfile_->readRegister(inst.rs2);

        auto cmd = std::make_shared<BruCommandPacket>(
            pc_, pc_ + 4,  // target address (simplified)
            static_cast<BruOp>(inst.opcode), rs1_val, rs2_val,
            static_cast<int>(inst.rd));
        cmd->timestamp = scheduler_.getCurrentTime();
        in_port->write(cmd);
      }
    }
  }

  /**
   * @brief Dispatch to MLU
   */
  void dispatchToMLU(const DecodedInstruction& inst) {
    if (mlu_) {
      auto in_port = mlu_->getPort("in");
      if (in_port && !in_port->hasData()) {
        uint32_t rs1_val = regfile_->readRegister(inst.rs1);
        uint32_t rs2_val = regfile_->readRegister(inst.rs2);

        auto cmd = std::make_shared<MultiplyUnit::MluData>(
            inst.rd, static_cast<MultiplyUnit::MulOp>(inst.opcode),
            static_cast<int64_t>(rs1_val) * static_cast<int64_t>(rs2_val));
        cmd->timestamp = scheduler_.getCurrentTime();
        in_port->write(cmd);
      }
    }
  }

  /**
   * @brief Dispatch to DVU
   */
  void dispatchToDVU(const DecodedInstruction& inst) {
    if (dvu_) {
      auto in_port = dvu_->getPort("in");
      if (in_port && !in_port->hasData()) {
        uint32_t rs1_val = regfile_->readRegister(inst.rs1);
        uint32_t rs2_val = regfile_->readRegister(inst.rs2);

        auto cmd = std::make_shared<DivideUnit::DvuData>(
            inst.rd, static_cast<DivideUnit::DivOp>(inst.opcode),
            static_cast<int32_t>(rs1_val), static_cast<int32_t>(rs2_val));
        cmd->timestamp = scheduler_.getCurrentTime();
        in_port->write(cmd);
      }
    }
  }

  /**
   * @brief Dispatch to LSU
   */
  void dispatchToLSU(const DecodedInstruction& inst) {
    if (lsu_) {
      auto in_port = lsu_->getPort("in");
      if (in_port && !in_port->hasData()) {
        uint32_t rs1_val = regfile_->readRegister(inst.rs1);
        uint32_t rs2_val = regfile_->readRegister(inst.rs2);

        auto cmd = std::make_shared<MemoryRequestPacket>(
            static_cast<LSUOp>(inst.opcode), rs1_val,
            static_cast<int32_t>(rs2_val), 1, 1, false);
        cmd->timestamp = scheduler_.getCurrentTime();
        in_port->write(cmd);
      }
    }
  }

  /**
   * @brief Check if instruction is a control flow instruction
   */
  bool isControlFlowInstruction(const DecodedInstruction& inst) const {
    return DispatchStage::isControlFlowInstruction(inst);
  }

  /**
   * @brief Check if instruction is special (CSR, FENCE, etc)
   */
  bool isSpecialInstruction(const DecodedInstruction& inst) const {
    return DispatchStage::isSpecialInstruction(inst);
  }

  /**
   * @brief Clear scoreboard after instruction retirement
   */
  void retireInstruction(uint32_t rd) {
    dispatch_ctrl_.retireRegister(rd);
    if (rd != 0 && rd < scoreboard_.size()) {
      scoreboard_[rd] = false;
    }
  }

  /**
   * @brief Get register value
   */
  uint32_t readRegister(uint32_t addr) {
    if (regfile_) {
      return regfile_->readRegister(addr);
    }
    return 0;
  }

  /**
   * @brief Write register value directly
   */
  void writeRegister(uint32_t addr, uint32_t data) {
    if (regfile_) {
      regfile_->writeRegister(addr, data);
    }
  }

  /**
   * @brief Get program counter
   */
  uint32_t getProgramCounter() const { return pc_; }

  /**
   * @brief Set program counter
   */
  void setProgramCounter(uint32_t pc) { pc_ = pc; }

  /**
   * @brief Get statistics
   */
  uint64_t getInstructionsDispatched() const {
    return instructions_dispatched_;
  }

  uint64_t getInstructionsRetired() const { return instructions_retired_; }

  /**
   * @brief Print core statistics and status
   */
  void printStatistics() const {
    std::cout << "\n========== " << name_
              << " Statistics ==========" << std::endl;

    std::cout << "\nDispatch Statistics:" << std::endl;
    std::cout << "  Instructions dispatched: " << instructions_dispatched_
              << std::endl;
    std::cout << "  Instructions retired: " << instructions_retired_
              << std::endl;

    std::cout << "\nFunctional Unit Statistics:" << std::endl;
    for (uint32_t i = 0; i < alusv_.size(); i++) {
      std::cout << "\n  ALU[" << i << "]:" << std::endl;
      std::cout << "    Operations: " << alusv_[i]->getOperationsExecuted()
                << std::endl;
    }

    if (bru_) {
      std::cout << "\n  BRU:" << std::endl;
      std::cout << "    Branches resolved: " << bru_->getBranchesResolved()
                << std::endl;
      std::cout << "    Branches taken: " << bru_->getBranchesTaken()
                << std::endl;
    }

    if (mlu_) {
      std::cout << "\n  MLU:" << std::endl;
      std::cout << "    Results output: " << mlu_->getResultsOutput()
                << std::endl;
    }

    if (dvu_) {
      std::cout << "\n  DVU:" << std::endl;
      std::cout << "    Results output: " << dvu_->getResultsOutput()
                << std::endl;
    }

    if (lsu_) {
      std::cout << "\n  LSU:" << std::endl;
      std::cout << "    Operations completed: "
                << lsu_->getOperationsCompleted() << std::endl;
    }

    if (regfile_) {
      std::cout << "\n  Register File:" << std::endl;
      std::cout << "    Reads: " << regfile_->getTotalReads() << std::endl;
      std::cout << "    Writes: " << regfile_->getTotalWrites() << std::endl;
    }

    std::cout << "\n==========================================\n" << std::endl;
  }

 private:
  // Configuration
  Config config_;

  // Dispatch controller
  DispatchStage dispatch_ctrl_;

  // Functional units
  std::vector<std::shared_ptr<ArithmeticLogicUnit>> alusv_;  // ALU instances
  std::shared_ptr<BranchUnit> bru_;                          // Branch unit
  std::shared_ptr<MultiplyUnit> mlu_;                        // Multiply unit
  std::shared_ptr<DivideUnit> dvu_;                          // Divide unit
  std::shared_ptr<LoadStoreUnit> lsu_;                       // Load-Store unit
  std::shared_ptr<RegisterFile> regfile_;                    // Register file

  // Connections
  std::vector<std::shared_ptr<Wire>> regfile_connections_;
  std::vector<std::shared_ptr<Wire>> alu_wb_connections_;
  std::shared_ptr<Wire> bru_wb_connection_;
  std::shared_ptr<Wire> mlu_wb_connection_;
  std::shared_ptr<Wire> dvu_wb_connection_;
  std::shared_ptr<Wire> lsu_wb_connection_;

  // Dispatch control
  bool dispatch_ready_;

  // Dispatch state
  std::vector<std::pair<uint32_t, uint32_t>> fetch_buffer_;  // (PC, word)
  std::vector<bool> scoreboard_;  // Register dependency tracking
  bool mlu_busy_;
  bool dvu_busy_;
  bool lsu_busy_;

  // Control state
  uint32_t pc_;        // Program counter
  bool branch_taken_;  // Branch taken in this cycle

  // Statistics
  uint64_t instructions_dispatched_;
  uint64_t instructions_retired_;
};

}  // namespace Architecture

#endif  // CORE_H
