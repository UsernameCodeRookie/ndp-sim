#ifndef PE_H
#define PE_H

#include <array>
#include <cstring>
#include <memory>
#include <queue>
#include <string>
#include <vector>

#include "../packet.h"
#include "../port.h"
#include "../tick.h"
#include "../trace.h"
#include "alu.h"

/**
 * @brief Register File Packet
 *
 * Contains register address and value for read/write operations
 */
class RegisterPacket : public Architecture::DataPacket {
 public:
  RegisterPacket(int address, int value, bool is_write = false)
      : address_(address), value_(value), is_write_(is_write) {}

  int getAddress() const { return address_; }
  int getValue() const { return value_; }
  bool isWrite() const { return is_write_; }

  void setAddress(int addr) { address_ = addr; }
  void setValue(int val) { value_ = val; }
  void setWrite(bool w) { is_write_ = w; }

  std::shared_ptr<Architecture::DataPacket> clone() const override {
    auto cloned = std::make_shared<RegisterPacket>(address_, value_, is_write_);
    cloned->setTimestamp(timestamp_);
    cloned->setValid(valid_);
    return cloned;
  }

 private:
  int address_;
  int value_;
  bool is_write_;
};

/**
 * @brief PE Instruction Packet
 *
 * Contains instruction for PE: operation, source registers, destination
 * register, and whether to use FPU or INTU
 */
class PEInstructionPacket : public Architecture::DataPacket {
 public:
  PEInstructionPacket(ALUOp op, int src_a, int src_b, int dst,
                      bool use_fpu = false)
      : operation_(op),
        src_reg_a_(src_a),
        src_reg_b_(src_b),
        dst_reg_(dst),
        use_fpu_(use_fpu) {}

  ALUOp getOperation() const { return operation_; }
  int getSrcRegA() const { return src_reg_a_; }
  int getSrcRegB() const { return src_reg_b_; }
  int getDstReg() const { return dst_reg_; }
  bool useFPU() const { return use_fpu_; }

  void setUseFPU(bool use_fpu) { use_fpu_ = use_fpu; }

  std::shared_ptr<Architecture::DataPacket> clone() const override {
    auto cloned = std::make_shared<PEInstructionPacket>(
        operation_, src_reg_a_, src_reg_b_, dst_reg_, use_fpu_);
    cloned->setTimestamp(timestamp_);
    cloned->setValid(valid_);
    return cloned;
  }

 private:
  ALUOp operation_;
  int src_reg_a_;
  int src_reg_b_;
  int dst_reg_;
  bool use_fpu_;  // true for FPU, false for INTU
};

/**
 * @brief Processing Element (PE) Component with MAC support
 *
 * A complete processing element with:
 * - Register file (local storage)
 * - INTU (Integer Unit - INT32 ALU) for integer computations
 * - FPU (Floating Point Unit - FP32 ALU) for floating point computations
 * - Instruction queue with back pressure support
 * - Input/output ports with ready/valid handshake
 * - Dedicated accumulators for MAC operations (separate for INT and FP)
 */
class ProcessingElement : public Architecture::TickingComponent {
 public:
  ProcessingElement(const std::string& name,
                    EventDriven::EventScheduler& scheduler, uint64_t period,
                    size_t num_registers = 32, size_t queue_depth = 4)
      : Architecture::TickingComponent(name, scheduler, period),
        num_registers_(num_registers),
        queue_depth_(queue_depth),
        instructions_executed_(0),
        int_instructions_executed_(0),
        fp_instructions_executed_(0),
        cycles_stalled_(0),
        input_ready_(true),
        output_valid_(false),
        mac_accumulator_(0),
        fp_mac_accumulator_(0.0f) {
    // Initialize register file
    register_file_.resize(num_registers_, 0);
    fp_register_file_.resize(num_registers_, 0.0f);

    // Create INTU (Integer Unit) and FPU (Floating Point Unit)
    intu_ = std::make_shared<INTUComponent>(name + "_INTU", scheduler, period);
    fpu_ = std::make_shared<FPUComponent>(name + "_FPU", scheduler, period);

    // Create ports
    addPort("inst_in",
            Architecture::PortDirection::INPUT);  // Instruction input port
    addPort(
        "data_in",
        Architecture::PortDirection::INPUT);  // Data input port (for loading
                                              // registers from external)
    addPort("data_out",
            Architecture::PortDirection::OUTPUT);  // Data output port
    addPort(
        "ready",
        Architecture::PortDirection::OUTPUT);  // Ready signal output (indicates
                                               // PE can accept new instruction)
    addPort(
        "valid",
        Architecture::PortDirection::INPUT);  // Valid signal input (indicates
                                              // input has valid instruction)
  }

  /**
   * @brief Main tick function with back pressure support
   */
  void tick() override {
    auto inst_in = getPort("inst_in");
    auto data_in = getPort("data_in");
    auto data_out = getPort("data_out");
    auto ready_out = getPort("ready");
    auto valid_in = getPort("valid");

    // Phase 1: Check if we can accept new instruction (back pressure)
    input_ready_ = (instruction_queue_.size() < queue_depth_);

    // Send ready signal
    auto ready_packet = std::make_shared<IntDataPacket>(input_ready_ ? 1 : 0);
    ready_out->write(
        std::static_pointer_cast<Architecture::DataPacket>(ready_packet));

    // Phase 2: Try to enqueue new instruction if ready and valid
    if (input_ready_ && inst_in->hasData()) {
      auto inst_packet =
          std::dynamic_pointer_cast<PEInstructionPacket>(inst_in->read());
      if (inst_packet) {
        instruction_queue_.push(inst_packet);

        TRACE_QUEUE_OP(scheduler_.getCurrentTime(), getName(), "Enqueued",
                       instruction_queue_.size(), queue_depth_);
        TRACE_EVENT(scheduler_.getCurrentTime(), getName(), "QUEUE_DETAIL",
                    "Enqueued "
                        << INTUComponent::getOpName(inst_packet->getOperation())
                        << " | size=" << instruction_queue_.size() << "/"
                        << queue_depth_
                        << " cycle=" << scheduler_.getCurrentTime() << " ready="
                        << (instruction_queue_.size() < queue_depth_));
      }
    }

    // Phase 3: Handle external data input (register write)
    if (data_in->hasData()) {
      auto reg_packet =
          std::dynamic_pointer_cast<RegisterPacket>(data_in->read());
      if (reg_packet && reg_packet->isWrite()) {
        writeRegister(reg_packet->getAddress(), reg_packet->getValue());

        // Trace register write
        EventDriven::Tracer::getInstance().traceRegisterAccess(
            scheduler_.getCurrentTime(), getName(), true,
            reg_packet->getAddress(), reg_packet->getValue());
      }
    }

    // Phase 4: Execute instruction from queue
    if (!instruction_queue_.empty()) {
      auto inst = instruction_queue_.front();
      instruction_queue_.pop();

      bool use_fpu = inst->useFPU();

      if (use_fpu) {
        // Use FPU for floating point operations
        float operand_a = readFPRegister(inst->getSrcRegA());
        float operand_b = readFPRegister(inst->getSrcRegB());

        float result;
        if (inst->getOperation() == ALUOp::MAC) {
          // MAC operation: accumulator = accumulator + (a * b)
          float prev_acc = fp_mac_accumulator_;
          fp_mac_accumulator_ = fp_mac_accumulator_ + (operand_a * operand_b);
          result = fp_mac_accumulator_;

          TRACE_MAC(scheduler_.getCurrentTime(), getName(), fp_mac_accumulator_,
                    operand_a, operand_b, " [FP32]");
          TRACE_COMPUTE(
              scheduler_.getCurrentTime(), getName(), "MAC_DETAIL",
              "acc=" << fp_mac_accumulator_ << " (prev=" << prev_acc << " + "
                     << operand_a << "*" << operand_b
                     << ") [FP32] | cycle=" << scheduler_.getCurrentTime()
                     << " period=" << getPeriod());
        } else {
          // Regular FPU operation
          result = FPUComponent::executeOperation(operand_a, operand_b,
                                                  inst->getOperation());

          TRACE_INSTRUCTION(
              scheduler_.getCurrentTime(), getName(),
              std::string(FPUComponent::getOpName(inst->getOperation())) +
                  "_FP32",
              FPUComponent::getOpName(inst->getOperation())
                  << " FR" << inst->getSrcRegA() << "(" << operand_a << "), FR"
                  << inst->getSrcRegB() << "(" << operand_b << ") -> FR"
                  << inst->getDstReg() << "(" << result << ")"
                  << " [FP32] | cycle=" << scheduler_.getCurrentTime()
                  << " period=" << getPeriod()
                  << " queue_depth=" << instruction_queue_.size());
        }

        // Write result back to FP register file
        writeFPRegister(inst->getDstReg(), result);
        fp_instructions_executed_++;

        // Output result (convert float to int representation)
        int float_as_int;
        std::memcpy(&float_as_int, &result, sizeof(float));
        auto result_packet = std::make_shared<IntDataPacket>(float_as_int);
        result_packet->setTimestamp(scheduler_.getCurrentTime());
        data_out->write(
            std::static_pointer_cast<Architecture::DataPacket>(result_packet));
      } else {
        // Use INTU for integer operations
        int operand_a = readRegister(inst->getSrcRegA());
        int operand_b = readRegister(inst->getSrcRegB());

        int result;
        if (inst->getOperation() == ALUOp::MAC) {
          // MAC operation: accumulator = accumulator + (a * b)
          int prev_acc = mac_accumulator_;
          mac_accumulator_ = mac_accumulator_ + (operand_a * operand_b);
          result = mac_accumulator_;

          TRACE_MAC(scheduler_.getCurrentTime(), getName(), mac_accumulator_,
                    operand_a, operand_b, " [INT32]");
          TRACE_COMPUTE(
              scheduler_.getCurrentTime(), getName(), "MAC_DETAIL",
              "acc=" << mac_accumulator_ << " (prev=" << prev_acc << " + "
                     << operand_a << "*" << operand_b
                     << ") [INT32] | cycle=" << scheduler_.getCurrentTime()
                     << " period=" << getPeriod());
        } else {
          // Regular INTU operation
          result = INTUComponent::executeOperation(operand_a, operand_b,
                                                   inst->getOperation());

          TRACE_INSTRUCTION(
              scheduler_.getCurrentTime(), getName(),
              std::string(INTUComponent::getOpName(inst->getOperation())) +
                  "_INT32",
              INTUComponent::getOpName(inst->getOperation())
                  << " R" << inst->getSrcRegA() << "(" << operand_a << "), R"
                  << inst->getSrcRegB() << "(" << operand_b << ") -> R"
                  << inst->getDstReg() << "(" << result << ")"
                  << " [INT32] | cycle=" << scheduler_.getCurrentTime()
                  << " period=" << getPeriod()
                  << " queue_depth=" << instruction_queue_.size());
        }

        // Write result back to register file
        writeRegister(inst->getDstReg(), result);
        int_instructions_executed_++;

        // Output result
        auto result_packet = std::make_shared<IntDataPacket>(result);
        result_packet->setTimestamp(scheduler_.getCurrentTime());
        data_out->write(
            std::static_pointer_cast<Architecture::DataPacket>(result_packet));
      }

      instructions_executed_++;
      output_valid_ = true;
    } else {
      // No instruction to execute - stall
      cycles_stalled_++;
      output_valid_ = false;
    }
  }

  /**
   * @brief Read from register file
   */
  int readRegister(int address) const {
    if (address >= 0 && address < static_cast<int>(num_registers_)) {
      return register_file_[address];
    }
    return 0;  // Return 0 for invalid address
  }

  /**
   * @brief Write to register file
   */
  void writeRegister(int address, int value) {
    if (address >= 0 && address < static_cast<int>(num_registers_)) {
      register_file_[address] = value;
    }
  }

  /**
   * @brief Read from FP register file
   */
  float readFPRegister(int address) const {
    if (address >= 0 && address < static_cast<int>(num_registers_)) {
      return fp_register_file_[address];
    }
    return 0.0f;  // Return 0 for invalid address
  }

  /**
   * @brief Write to FP register file
   */
  void writeFPRegister(int address, float value) {
    if (address >= 0 && address < static_cast<int>(num_registers_)) {
      fp_register_file_[address] = value;
    }
  }

  /**
   * @brief Initialize register with value
   */
  void initRegister(int address, int value) { writeRegister(address, value); }

  /**
   * @brief Initialize FP register with value
   */
  void initFPRegister(int address, float value) {
    writeFPRegister(address, value);
  }

  /**
   * @brief Check if PE is ready to accept instruction
   */
  bool isReady() const { return input_ready_; }

  /**
   * @brief Check if PE has valid output
   */
  bool hasValidOutput() const { return output_valid_; }

  /**
   * @brief Get queue occupancy
   */
  size_t getQueueOccupancy() const { return instruction_queue_.size(); }

  /**
   * @brief Get MAC accumulator value (integer)
   */
  int getMACAccumulator() const { return mac_accumulator_; }

  /**
   * @brief Reset MAC accumulator (integer)
   */
  void resetMACAccumulator() { mac_accumulator_ = 0; }

  /**
   * @brief Set MAC accumulator value (integer)
   */
  void setMACAccumulator(int value) { mac_accumulator_ = value; }

  /**
   * @brief Get FP MAC accumulator value
   */
  float getFPMACAccumulator() const { return fp_mac_accumulator_; }

  /**
   * @brief Reset FP MAC accumulator
   */
  void resetFPMACAccumulator() { fp_mac_accumulator_ = 0.0f; }

  /**
   * @brief Set FP MAC accumulator value
   */
  void setFPMACAccumulator(float value) { fp_mac_accumulator_ = value; }

  /**
   * @brief Print register file contents
   */
  void printRegisters() const {
    std::cout << "\n=== Integer Register File: " << getName()
              << " ===" << std::endl;
    for (size_t i = 0; i < num_registers_; ++i) {
      if (i % 8 == 0) std::cout << std::endl;
      std::cout << "R" << i << "=" << register_file_[i] << "\t";
    }
    std::cout << std::endl;

    std::cout << "\n=== Float Register File: " << getName()
              << " ===" << std::endl;
    for (size_t i = 0; i < num_registers_; ++i) {
      if (i % 8 == 0) std::cout << std::endl;
      std::cout << "FR" << i << "=" << fp_register_file_[i] << "\t";
    }
    std::cout << std::endl;
  }

  /**
   * @brief Print statistics
   */
  void printStatistics() const {
    std::cout << "\n=== PE Statistics: " << getName() << " ===" << std::endl;
    std::cout << "Instructions executed (total): " << instructions_executed_
              << std::endl;
    std::cout << "  - Integer instructions: " << int_instructions_executed_
              << std::endl;
    std::cout << "  - Floating point instructions: "
              << fp_instructions_executed_ << std::endl;
    std::cout << "Cycles stalled: " << cycles_stalled_ << std::endl;
    std::cout << "Queue occupancy: " << instruction_queue_.size() << "/"
              << queue_depth_ << std::endl;
    std::cout << "Ready: " << (input_ready_ ? "Yes" : "No") << std::endl;
    std::cout << "Output valid: " << (output_valid_ ? "Yes" : "No")
              << std::endl;
    if (instructions_executed_ + cycles_stalled_ > 0) {
      std::cout << "Utilization: "
                << (100.0 * instructions_executed_ /
                    (instructions_executed_ + cycles_stalled_))
                << "%" << std::endl;
    }
    std::cout << "\n--- INTU Statistics ---" << std::endl;
    if (intu_) intu_->printStatistics();
    std::cout << "\n--- FPU Statistics ---" << std::endl;
    if (fpu_) fpu_->printStatistics();
  }

 private:
  size_t num_registers_;                 // Number of registers
  size_t queue_depth_;                   // Instruction queue depth
  std::vector<int> register_file_;       // Integer register file storage
  std::vector<float> fp_register_file_;  // Floating point register file storage
  std::queue<std::shared_ptr<PEInstructionPacket>>
      instruction_queue_;                // Instruction queue
  std::shared_ptr<INTUComponent> intu_;  // Integer Unit (INT32 ALU)
  std::shared_ptr<FPUComponent> fpu_;    // Floating Point Unit (FP32 ALU)
  uint64_t instructions_executed_;       // Total instructions executed count
  uint64_t int_instructions_executed_;   // Integer instructions executed count
  uint64_t fp_instructions_executed_;    // FP instructions executed count
  uint64_t cycles_stalled_;              // Stall cycles count
  bool input_ready_;                     // Ready to accept instruction
  bool output_valid_;                    // Has valid output
  int mac_accumulator_;                  // Integer MAC accumulator
  float fp_mac_accumulator_;             // Floating point MAC accumulator
};

#endif  // PE_H
