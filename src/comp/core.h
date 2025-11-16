#ifndef CORE_H
#define CORE_H

#include <algorithm>
#include <cstring>
#include <map>
#include <memory>
#include <queue>
#include <string>
#include <vector>

#include "comp/alu.h"
#include "comp/bru.h"
#include "comp/buffer.h"
#include "comp/core/decode.h"
#include "comp/core/dispatch.h"
#include "comp/core/fetch.h"
#include "comp/dvu.h"
#include "comp/fpu.h"
#include "comp/lsu.h"
#include "comp/mlu.h"
#include "comp/pipeline.h"
#include "comp/regfile.h"
#include "comp/rvv_interface.h"
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
        fetch_stage_(name + "_FetchStage", 8),
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

    // Create instruction buffer (i-buffer) based on Buffer class
    BufferParameters ibuf_params(
        4096,                       // depth: 4096 instructions
        32,                         // data_width: 32 bits per instruction
        1,                          // num_read_ports: 1 for fetch
        1,                          // num_write_ports: 1 for load
        BufferMode::RANDOM_ACCESS,  // mode: random access for instructions
        false,                      // enable_bypass
        true,                       // enable_overflow_check
        true,                       // enable_underflow_check
        0,                          // read_latency
        0                           // write_latency
    );
    ibuffer_ =
        std::make_shared<Buffer>(name + "_IBuffer", scheduler, ibuf_params);

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

    // Connect functional unit outputs to wire connections
    // This enables data buffering through wire instead of direct port access
    for (uint32_t i = 0; i < alusv_.size() && i < alu_wb_connections_.size();
         i++) {
      auto alu = alusv_[i];
      auto out_port = alu->getPort("out");
      if (out_port && alu_wb_connections_[i]) {
        alu_wb_connections_[i]->addSourcePort(out_port);
      }
    }

    if (bru_) {
      auto out_port = bru_->getPort("out");
      if (out_port && bru_wb_connection_) {
        bru_wb_connection_->addSourcePort(out_port);
      }
    }

    if (mlu_) {
      auto out_port = mlu_->getPort("out");
      if (out_port && mlu_wb_connection_) {
        mlu_wb_connection_->addSourcePort(out_port);
      }
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
    // Connect fetch stage to instruction buffer
    fetch_stage_.setInstructionBuffer(ibuffer_);

    // Call Pipeline's initialize to start the pipeline ticking
    Pipeline::initialize();

    // Start the pipeline - this schedules the tick events
    Pipeline::start(config_.start_time);

    TRACE_EVENT(scheduler_.getCurrentTime(), getName(), "INITIALIZE",
                "Starting SCore initialization");

    // Inject initial data packet to start the pipeline
    // This primes the fetch stage to begin fetching instructions
    auto initial_packet = std::make_shared<Architecture::IntDataPacket>(0);
    initial_packet->timestamp = scheduler_.getCurrentTime();
    auto in_port = getPort("in");
    if (in_port) {
      in_port->write(initial_packet);
    }

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
    // Fetch instruction from instruction buffer and decode it
    setStageFunction(0, [this](std::shared_ptr<Architecture::DataPacket> data) {
      uint64_t current_time = scheduler_.getCurrentTime();

      // Try to fetch a new instruction from instruction buffer
      // Allow fetch_buffer to accumulate instructions (max size 8) to handle
      // stalls
      const size_t MAX_FETCH_BUFFER = 8;
      if (fetch_buffer_.size() < MAX_FETCH_BUFFER) {
        // Load from buffer - always attempt to fetch
        uint32_t inst_word = ibuffer_->load(pc_);

        // Continue fetching as long as we're in valid address space
        // Don't use inst_word value as stopping condition (0 is valid NOP)
        // Only stop if we've explicitly detected end of program
        fetch_buffer_.push_back({pc_, inst_word});

        TRACE_COMPUTE(current_time, getName(), "STAGE0_FETCH",
                      "Fetch from PC=0x" << std::hex << pc_ << " word=0x"
                                         << inst_word << std::dec);

        // Increment PC for next instruction
        pc_ += 4;
      }

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
                      "Decode PC=0x" << std::hex << inst_word.first
                                     << " word=0x" << inst_word.second
                                     << std::dec);

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

      // Always return a valid data packet to keep Pipeline flowing
      // This ensures Stage 2 is continuously called for writeback polling
      // even when dispatch is stalled
      if (!data) {
        data = std::make_shared<Architecture::IntDataPacket>(0);
      }
      return data;
    });

    // Stage 2: Execute/Writeback
    // Monitor execution unit completions and update statistics
    // Poll results from wire connections (which buffer FU outputs)
    setStageFunction(2, [this](std::shared_ptr<Architecture::DataPacket> data) {
      uint64_t current_time = scheduler_.getCurrentTime();

      // Poll ALL ALU writeback wires every cycle
      // NOTE: Read from Wire's buffer instead of source port directly
      // This uses the Wire's internal buffering to prevent data loss
      std::shared_ptr<Architecture::DataPacket> wb_result = nullptr;
      for (uint32_t i = 0; i < alu_wb_connections_.size(); i++) {
        auto conn = alu_wb_connections_[i];
        if (conn) {
          // Read from Wire's current buffer instead of source port
          if (conn->hasCurrentData()) {
            auto result = conn->getCurrentData();

            auto alu_result =
                std::dynamic_pointer_cast<Architecture::ALUResultPacket>(
                    result);
            if (alu_result) {
              // Check if we've already processed this rd value
              uint64_t result_cycle = result->timestamp;
              if (last_alu_rd_cycles_[i] != result_cycle) {
                // New result, process it
                if (alu_result->rd != 0) {
                  regfile_->writeRegister(alu_result->rd, alu_result->value);
                  TRACE_COMPUTE(current_time, getName(), "ALU_OUTPUT",
                                "ALU" << i << " rd=" << alu_result->rd
                                      << " value=" << alu_result->value);
                  TRACE_COMPUTE(current_time, getName(), "REGFILE_WRITE",
                                "ALU wrote x" << alu_result->rd << " = "
                                              << alu_result->value);
                  retireInstruction(alu_result->rd);

                  TRACE_COMPUTE(current_time, getName(), "STAGE2_WRITEBACK",
                                "Retired ALU result="
                                    << alu_result->value << " to x"
                                    << alu_result->rd << " from ALU" << i);
                  last_alu_rd_cycles_[i] = result_cycle;

                  // Clear the wire buffer after successful read
                  conn->clearCurrentData();
                }
                instructions_retired_++;
                wb_result = result;  // Keep last result
              }
            }
          }
        }
      }

      // Poll BRU output from wire
      if (bru_wb_connection_ && !bru_wb_connection_->getSourcePorts().empty()) {
        auto src_port = bru_wb_connection_->getSourcePorts()[0];
        if (src_port && src_port->hasData()) {
          auto result = src_port->read();
          auto bru_result =
              std::dynamic_pointer_cast<Architecture::BruResultPacket>(result);
          if (bru_result && bru_result->link_valid && bru_result->rd != 0) {
            // For JAL/JALR, write back the link address
            regfile_->writeRegister(bru_result->rd, bru_result->link_data);
            TRACE_COMPUTE(current_time, getName(), "REGFILE_WRITE",
                          "BRU wrote x" << bru_result->rd << " = "
                                        << bru_result->link_data);
            retireInstruction(bru_result->rd);
            instructions_retired_++;
            wb_result = result;
          }
        }
      }

      // Poll MLU output from wire
      if (mlu_wb_connection_ && !mlu_wb_connection_->getSourcePorts().empty()) {
        auto src_port = mlu_wb_connection_->getSourcePorts()[0];
        if (src_port && src_port->hasData()) {
          auto result = src_port->read();
          auto mlu_result =
              std::dynamic_pointer_cast<Architecture::MLUResultPacket>(result);
          if (mlu_result && mlu_result->rd != 0) {
            regfile_->writeRegister(mlu_result->rd, mlu_result->value);
            TRACE_COMPUTE(
                current_time, getName(), "REGFILE_WRITE",
                "MLU wrote x" << mlu_result->rd << " = " << mlu_result->value);
            retireInstruction(mlu_result->rd);
            TRACE_COMPUTE(current_time, getName(), "STAGE2_WRITEBACK",
                          "Retired MLU result=" << mlu_result->value << " to x"
                                                << mlu_result->rd);
            instructions_retired_++;
            wb_result = result;
          }
        }
      }

      // If we found an ALU result, return it (keeps Stage 2 active)
      // Otherwise return the data from Stage 1 (if any)
      // This ensures Stage 2 is always called even when dispatch is stalled
      return wb_result ? wb_result : data;
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

    // Delegate to helper methods to reduce duplication
    switch (op_type) {
      case OpType::ALU:
        issueALU(lane, operand1, operand2, opcode, rd);
        TRACE_COMPUTE(current_time, getName(), "DISPATCH_ALU",
                      "lane=" << lane << " rd=" << rd << " op1=" << operand1
                              << " op2=" << operand2);
        break;

      case OpType::BRU:
        issueBRU(operand1, opcode, 0, 0, rd);
        TRACE_COMPUTE(current_time, getName(), "DISPATCH_BRU",
                      "rd=" << rd << " operand=" << operand1);
        break;

      case OpType::MLU:
        issueMLU(
            rd, opcode,
            static_cast<int64_t>(operand1) * static_cast<int64_t>(operand2));
        TRACE_COMPUTE(
            current_time, getName(), "DISPATCH_MLU",
            "rd=" << rd << " op1=" << operand1 << " op2=" << operand2);
        break;

      case OpType::DVU:
        issueDVU(rd, opcode, static_cast<int32_t>(operand1),
                 static_cast<int32_t>(operand2));
        TRACE_COMPUTE(
            current_time, getName(), "DISPATCH_DVU",
            "rd=" << rd << " op1=" << operand1 << " op2=" << operand2);
        break;

      case OpType::LSU:
        issueLSU(opcode, operand1, static_cast<int32_t>(operand2));
        TRACE_COMPUTE(current_time, getName(), "DISPATCH_LSU",
                      "addr=" << operand1 << " data=" << operand2);
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
      bool dispatch_ok = dispatchToUnit(inst, lane);

      if (!dispatch_ok) {
        // Dispatch failed (unit busy), mark as blocked and stop trying
        TRACE_COMPUTE(current_time, getName(), "DISPATCH_UNIT_BUSY",
                      "Unit busy at lane=" << lane << ", rd=" << inst.rd);
        break;
      }

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
    bool result = dispatch_ctrl_.canDispatch(inst, lane);
    uint64_t current_time = scheduler_.getCurrentTime();

    // Debug: Print reason if dispatch is blocked
    if (!result && current_time >= 7) {
      bool is_special =
          (lane > 0) && (inst.op_type == DecodedInstruction::OpType::CSR ||
                         inst.op_type == DecodedInstruction::OpType::FENCE);
      bool raw_rs1 =
          (inst.rs1 != 0 && dispatch_ctrl_.getScoreboard()[inst.rs1]);
      bool raw_rs2 =
          (inst.rs2 != 0 && dispatch_ctrl_.getScoreboard()[inst.rs2]);
      bool resource_conflict = false;

      TRACE_COMPUTE(
          current_time, getName(), "DISPATCH_FAIL_REASON",
          "rd=" << inst.rd << " lane=" << lane << " special=" << is_special
                << " raw_rs1[" << inst.rs1 << "]=" << raw_rs1 << " raw_rs2["
                << inst.rs2 << "]=" << raw_rs2
                << " rs1_val=" << dispatch_ctrl_.getScoreboard()[inst.rs1]
                << " rs2_val=" << dispatch_ctrl_.getScoreboard()[inst.rs2]);
    }

    return result;
  }

  /**
   * @brief Dispatch instruction to appropriate execution unit
   * @return true if dispatch succeeded, false if unit's port is busy
   */
  bool dispatchToUnit(const DecodedInstruction& inst, uint32_t lane) {
    switch (inst.op_type) {
      case DecodedInstruction::OpType::ALU:
        return dispatchToALU(inst, lane);
      case DecodedInstruction::OpType::BRU:
        return dispatchToBRU(inst);
      case DecodedInstruction::OpType::MLU:
        return dispatchToMLU(inst);
      case DecodedInstruction::OpType::DVU:
        return dispatchToDVU(inst);
      case DecodedInstruction::OpType::LSU:
        return dispatchToLSU(inst);
      case DecodedInstruction::OpType::VECTOR:
        return dispatchToVector(inst, lane);
      case DecodedInstruction::OpType::CSR:
      case DecodedInstruction::OpType::FENCE:
        // For simplified implementation, these always succeed
        return true;
      default:
        return false;
    }
  }

  /**
   * @brief Dispatch to ALU
   */
  bool dispatchToALU(const DecodedInstruction& inst, uint32_t lane) {
    // Read operands and dispatch using helper
    uint32_t rs1_val = regfile_->readRegister(inst.rs1);
    TRACE_COMPUTE(scheduler_.getCurrentTime(), getName(), "REGFILE_READ",
                  "Dispatch: x" << inst.rs1 << " = " << rs1_val);

    // For I-type instructions (like ADDI), use immediate as second operand
    // For R-type instructions (like ADD), use rs2 register
    uint32_t rs2_val = inst.imm;  // Default to immediate (I-type)

    // If this is an R-type instruction, read rs2
    // Check if inst.opcode (bits 25-31) indicates R-type
    // For now, assume ADDI uses imm, ADD uses rs2
    // A better check would examine the funct3 field
    if ((inst.word & 0x7F) == 0x33) {  // R-type opcode (ADD, SUB, etc)
      rs2_val = regfile_->readRegister(inst.rs2);
      TRACE_COMPUTE(scheduler_.getCurrentTime(), getName(), "REGFILE_READ",
                    "Dispatch: x" << inst.rs2 << " = " << rs2_val);
    }

    return issueALU(lane, rs1_val, rs2_val, inst.opcode, inst.rd);
  }

  /**
   * @brief Dispatch to BRU
   */
  bool dispatchToBRU(const DecodedInstruction& inst) {
    // Read operands and dispatch using helper
    uint32_t rs1_val = regfile_->readRegister(inst.rs1);
    TRACE_COMPUTE(scheduler_.getCurrentTime(), getName(), "REGFILE_READ",
                  "Dispatch: x" << inst.rs1 << " = " << rs1_val);
    uint32_t rs2_val = regfile_->readRegister(inst.rs2);
    TRACE_COMPUTE(scheduler_.getCurrentTime(), getName(), "REGFILE_READ",
                  "Dispatch: x" << inst.rs2 << " = " << rs2_val);
    return issueBRU(pc_ + 4, inst.opcode, rs1_val, rs2_val, inst.rd);
  }

  /**
   * @brief Dispatch to MLU
   */
  bool dispatchToMLU(const DecodedInstruction& inst) {
    // Read operands and dispatch using helper
    uint32_t rs1_val = regfile_->readRegister(inst.rs1);
    TRACE_COMPUTE(scheduler_.getCurrentTime(), getName(), "REGFILE_READ",
                  "Dispatch: x" << inst.rs1 << " = " << rs1_val);
    uint32_t rs2_val = regfile_->readRegister(inst.rs2);
    TRACE_COMPUTE(scheduler_.getCurrentTime(), getName(), "REGFILE_READ",
                  "Dispatch: x" << inst.rs2 << " = " << rs2_val);
    return issueMLU(
        inst.rd, inst.opcode,
        static_cast<int64_t>(rs1_val) * static_cast<int64_t>(rs2_val));
  }

  /**
   * @brief Dispatch to DVU
   */
  bool dispatchToDVU(const DecodedInstruction& inst) {
    // Read operands and dispatch using helper
    uint32_t rs1_val = regfile_->readRegister(inst.rs1);
    TRACE_COMPUTE(scheduler_.getCurrentTime(), getName(), "REGFILE_READ",
                  "Dispatch: x" << inst.rs1 << " = " << rs1_val);
    uint32_t rs2_val = regfile_->readRegister(inst.rs2);
    TRACE_COMPUTE(scheduler_.getCurrentTime(), getName(), "REGFILE_READ",
                  "Dispatch: x" << inst.rs2 << " = " << rs2_val);
    return issueDVU(inst.rd, inst.opcode, static_cast<int32_t>(rs1_val),
                    static_cast<int32_t>(rs2_val));
  }

  /**
   * @brief Dispatch to LSU
   */
  bool dispatchToLSU(const DecodedInstruction& inst) {
    // Read operands and dispatch using helper
    uint32_t rs1_val = regfile_->readRegister(inst.rs1);
    TRACE_COMPUTE(scheduler_.getCurrentTime(), getName(), "REGFILE_READ",
                  "Dispatch: x" << inst.rs1 << " = " << rs1_val);
    uint32_t rs2_val = regfile_->readRegister(inst.rs2);
    TRACE_COMPUTE(scheduler_.getCurrentTime(), getName(), "REGFILE_READ",
                  "Dispatch: x" << inst.rs2 << " = " << rs2_val);
    return issueLSU(inst.opcode, rs1_val, static_cast<int32_t>(rs2_val));
  }

  /**
   * @brief Dispatch vector instruction to RVV backend
   * @return true if RVV interface accepted the instruction
   */
  bool dispatchToVector(const DecodedInstruction& inst, uint32_t /* lane */) {
    // Check if RVV interface is connected
    if (!rvv_interface_) {
      TRACE_COMPUTE(scheduler_.getCurrentTime(), getName(),
                    "DISPATCH_VECTOR_FAIL", "RVV interface not connected");
      return false;
    }

    uint64_t current_time = scheduler_.getCurrentTime();

    // Create instruction request for RVV backend
    // Extract vector register indices from instruction word
    uint32_t vd = inst.rd;    // Destination vector register (bits 7-11)
    uint32_t vs1 = inst.rs1;  // Source vector register 1 (bits 15-19)
    uint32_t vs2 = inst.rs2;  // Source vector register 2 (bits 20-24)

    RVVCoreInterface::InstructionRequest vec_req;
    vec_req.opcode = inst.opcode;
    vec_req.vd_idx = vd;
    vec_req.vs1_idx = vs1;
    vec_req.vs2_idx = vs2;
    vec_req.bits = inst.word & 0x1FFFFFF;  // 25-bit payload
    vec_req.vm = 1;                        // Assume mask enabled
    vec_req.sew = 0;                       // 8-bit
    vec_req.lmul = 0;                      // LMUL=1
    vec_req.vl = 8;                        // Vector length
    vec_req.pc = inst.addr;

    // Issue the vector instruction to RVV backend
    bool accepted = rvv_interface_->issueInstruction(vec_req);

    if (accepted) {
      TRACE_COMPUTE(current_time, getName(), "DISPATCH_VECTOR_SUCCESS",
                    "Issued vector instruction to RVV backend, "
                    "opcode=0x"
                        << std::hex << inst.opcode << std::dec << " vd=" << vd
                        << " vs1=" << vs1 << " vs2=" << vs2);
    } else {
      TRACE_COMPUTE(
          current_time, getName(), "DISPATCH_VECTOR_BUSY",
          "RVV backend busy, cannot issue vector instruction opcode=0x"
              << std::hex << inst.opcode << std::dec);
    }

    return accepted;
  }

 private:
  // ============ Private Helper Method Implementations ============

  /**
   * @brief Helper: Write ALU command to functional unit
   * @private
   */
  bool issueALU(uint32_t lane, uint32_t op1, uint32_t op2, uint32_t opcode,
                uint32_t rd) {
    if (lane < alusv_.size()) {
      auto alu = alusv_[lane];
      auto in_port = alu->getPort("in");
      if (in_port && !in_port->hasData()) {
        auto cmd = std::make_shared<ALUDataPacket>(
            static_cast<int32_t>(op1), static_cast<int32_t>(op2),
            static_cast<ALUOp>(opcode), rd);
        cmd->timestamp = scheduler_.getCurrentTime();
        in_port->write(cmd);
        TRACE_COMPUTE(scheduler_.getCurrentTime(), getName(), "ALU_DISPATCH",
                      "Dispatched to " << alu->getName() << " rd=" << rd
                                       << " op1=" << op1 << " op2=" << op2);
        return true;
      }
    }
    return false;
  }

  /**
   * @brief Helper: Write BRU command to functional unit
   * @private
   */
  bool issueBRU(uint32_t pc_next, uint32_t opcode, uint32_t op1, uint32_t op2,
                uint32_t rd) {
    if (bru_) {
      auto in_port = bru_->getPort("in");
      if (in_port && !in_port->hasData()) {
        auto cmd = std::make_shared<BruCommandPacket>(
            pc_, pc_next, static_cast<BruOp>(opcode), op1, op2,
            static_cast<int>(rd));
        cmd->timestamp = scheduler_.getCurrentTime();
        in_port->write(cmd);
        return true;
      }
    }
    return false;
  }

  /**
   * @brief Helper: Write MLU command to functional unit
   * @private
   */
  bool issueMLU(uint32_t rd, uint32_t opcode, int64_t product) {
    if (mlu_) {
      auto in_port = mlu_->getPort("in");
      if (in_port && !in_port->hasData()) {
        auto cmd = std::make_shared<MultiplyUnit::MluData>(
            rd, static_cast<MultiplyUnit::MulOp>(opcode), product);
        cmd->timestamp = scheduler_.getCurrentTime();
        in_port->write(cmd);
        return true;
      }
    }
    return false;
  }

  /**
   * @brief Helper: Write DVU command to functional unit
   * @private
   */
  bool issueDVU(uint32_t rd, uint32_t opcode, int32_t op1, int32_t op2) {
    if (dvu_) {
      auto in_port = dvu_->getPort("in");
      if (in_port && !in_port->hasData()) {
        auto cmd = std::make_shared<DivideUnit::DvuData>(
            rd, static_cast<DivideUnit::DivOp>(opcode), op1, op2);
        cmd->timestamp = scheduler_.getCurrentTime();
        in_port->write(cmd);
        return true;
      }
    }
    return false;
  }

  /**
   * @brief Helper: Write LSU command to functional unit
   * @private
   */
  bool issueLSU(uint32_t opcode, uint32_t addr, int32_t data) {
    if (lsu_) {
      auto in_port = lsu_->getPort("in");
      if (in_port && !in_port->hasData()) {
        auto cmd = std::make_shared<MemoryRequestPacket>(
            static_cast<LSUOp>(opcode), addr, data, 1, 1, false);
        cmd->timestamp = scheduler_.getCurrentTime();
        in_port->write(cmd);
        return true;
      }
    }
    return false;
  }

 public:
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
      uint32_t val = regfile_->readRegister(addr);
      TRACE_COMPUTE(scheduler_.getCurrentTime(), getName(), "REGFILE_READ",
                    "Debug read: x" << addr << " = " << val);
      return val;
    }
    return 0;
  }

  /**
   * @brief Write register value directly
   */
  void writeRegister(uint32_t addr, uint32_t data) {
    if (regfile_) {
      regfile_->writeRegister(addr, data);
      TRACE_COMPUTE(scheduler_.getCurrentTime(), getName(), "REGFILE_WRITE",
                    "Debug write: x" << addr << " = " << data);
    }
  }

  /**
   * @brief Get program counter
   */
  uint32_t getProgramCounter() const { return pc_; }

  /**
   * @brief Set program counter
   */
  void setProgramCounter(uint32_t pc) {
    pc_ = pc;
    fetch_stage_.setPC(pc);
  }

  /**
   * @brief Get fetch stage PC
   */
  uint32_t getFetchPC() const { return fetch_stage_.getPC(); }

  /**
   * @brief Set fetch stage PC (used for branch redirects)
   */
  void setFetchPC(uint32_t pc) { fetch_stage_.setPC(pc); }

  /**
   * @brief Handle branch redirect in fetch stage
   */
  void fetchBranchRedirect(uint32_t target_pc) {
    fetch_stage_.branchRedirect(target_pc);
  }

  /**
   * @brief Handle flush in fetch stage
   */
  void fetchFlush(uint32_t flush_pc) { fetch_stage_.flushAndReset(flush_pc); }

 public:
  /**
   * @brief Load instruction into instruction buffer
   * @param pc Program counter / address
   * @param instruction Encoded instruction word
   */
  void loadInstruction(uint32_t pc, uint32_t instruction) {
    ibuffer_->store(pc, instruction);
  }

  /**
   * @brief Load data into data memory
   * @param addr Memory address
   * @param data Data value to write
   */
  void loadData(uint32_t addr, uint32_t data) {
    // Expand data memory if needed
    if (addr / 4 >= data_memory_.size()) {
      data_memory_.resize(addr / 4 + 1, 0);
    }
    data_memory_[addr / 4] = data;
  }

  /**
   * @brief Read data from data memory
   * @param addr Memory address
   * @return Data value at address
   */
  uint32_t readData(uint32_t addr) const {
    if (addr / 4 < data_memory_.size()) {
      return data_memory_[addr / 4];
    }
    return 0;
  }

  /**
   * @brief Get instruction from instruction buffer
   * @param pc Program counter
   * @return Instruction word
   */
  uint32_t getInstruction(uint32_t pc) const { return ibuffer_->load(pc); }

  /**
   * @brief Set RVV core interface for vector extension support
   * @param rvv_interface Pointer to RVVCoreInterface implementation
   */
  void setRVVInterface(std::shared_ptr<RVVCoreInterface> rvv_interface) {
    rvv_interface_ = rvv_interface;
  }

  /**
   * @brief Get RVV core interface
   * @return RVVCoreInterface pointer or nullptr if not set
   */
  std::shared_ptr<RVVCoreInterface> getRVVInterface() const {
    return rvv_interface_;
  }

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

  // Pipeline stages
  FetchStage fetch_stage_;

  // Dispatch controller
  DispatchStage dispatch_ctrl_;

  // Functional units
  std::vector<std::shared_ptr<ArithmeticLogicUnit>> alusv_;  // ALU instances
  std::shared_ptr<BranchUnit> bru_;                          // Branch unit
  std::shared_ptr<MultiplyUnit> mlu_;                        // Multiply unit
  std::shared_ptr<DivideUnit> dvu_;                          // Divide unit
  std::shared_ptr<LoadStoreUnit> lsu_;                       // Load-Store unit
  std::shared_ptr<RegisterFile> regfile_;                    // Register file

  // Vector extension (RVV)
  std::shared_ptr<RVVCoreInterface> rvv_interface_;  // RVV backend interface

  // Memory components
  std::shared_ptr<Buffer> ibuffer_;  // Instruction buffer (i-cache predecessor)

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

  // Data memory - for LSU operations (separate address space)
  std::vector<uint32_t> data_memory_;

  // Control state
  uint32_t pc_;        // Program counter
  bool branch_taken_;  // Branch taken in this cycle

  // Statistics
  uint64_t instructions_dispatched_;
  uint64_t instructions_retired_;

  // Track last read ALU results to avoid duplicates
  std::map<uint32_t, uint64_t>
      last_alu_rd_cycles_;  // alu_index -> last rd cycle
};

}  // namespace Architecture

#endif  // CORE_H
