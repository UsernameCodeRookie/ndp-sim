#ifndef PE_H
#define PE_H

#include <array>
#include <iostream>
#include <memory>
#include <queue>
#include <string>
#include <vector>

#include "../port.h"
#include "../tick.h"
#include "alu.h"
#include "int_packet.h"

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
 * register
 */
class PEInstructionPacket : public Architecture::DataPacket {
 public:
  PEInstructionPacket(ALUOp op, int src_a, int src_b, int dst)
      : operation_(op), src_reg_a_(src_a), src_reg_b_(src_b), dst_reg_(dst) {}

  ALUOp getOperation() const { return operation_; }
  int getSrcRegA() const { return src_reg_a_; }
  int getSrcRegB() const { return src_reg_b_; }
  int getDstReg() const { return dst_reg_; }

  std::shared_ptr<Architecture::DataPacket> clone() const override {
    auto cloned = std::make_shared<PEInstructionPacket>(operation_, src_reg_a_,
                                                        src_reg_b_, dst_reg_);
    cloned->setTimestamp(timestamp_);
    cloned->setValid(valid_);
    return cloned;
  }

 private:
  ALUOp operation_;
  int src_reg_a_;
  int src_reg_b_;
  int dst_reg_;
};

/**
 * @brief Processing Element (PE) Component
 *
 * A complete processing element with:
 * - Register file (local storage)
 * - ALU for computations
 * - Instruction queue with back pressure support
 * - Input/output ports with ready/valid handshake
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
        cycles_stalled_(0),
        input_ready_(true),
        output_valid_(false) {
    // Initialize register file
    register_file_.resize(num_registers_, 0);

    // Create ALU
    alu_ = std::make_shared<ALUComponent>(name + "_ALU", scheduler, period);

    // Create ports
    // Instruction input port
    auto inst_in = std::make_shared<Architecture::Port>(
        "inst_in", Architecture::PortDirection::INPUT, this);
    addPort(inst_in);

    // Data input port (for loading registers from external)
    auto data_in = std::make_shared<Architecture::Port>(
        "data_in", Architecture::PortDirection::INPUT, this);
    addPort(data_in);

    // Data output port
    auto data_out = std::make_shared<Architecture::Port>(
        "data_out", Architecture::PortDirection::OUTPUT, this);
    addPort(data_out);

    // Ready signal output (indicates PE can accept new instruction)
    auto ready_out = std::make_shared<Architecture::Port>(
        "ready", Architecture::PortDirection::OUTPUT, this);
    addPort(ready_out);

    // Valid signal input (indicates input has valid instruction)
    auto valid_in = std::make_shared<Architecture::Port>(
        "valid", Architecture::PortDirection::INPUT, this);
    addPort(valid_in);
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
        if (verbose_) {
          std::cout << "[" << scheduler_.getCurrentTime() << "] " << getName()
                    << ": Enqueued instruction, queue="
                    << instruction_queue_.size() << "/" << queue_depth_
                    << std::endl;
        }
      }
    }

    // Phase 3: Handle external data input (register write)
    if (data_in->hasData()) {
      auto reg_packet =
          std::dynamic_pointer_cast<RegisterPacket>(data_in->read());
      if (reg_packet && reg_packet->isWrite()) {
        writeRegister(reg_packet->getAddress(), reg_packet->getValue());
        if (verbose_) {
          std::cout << "[" << scheduler_.getCurrentTime() << "] " << getName()
                    << ": Write R" << reg_packet->getAddress() << " = "
                    << reg_packet->getValue() << std::endl;
        }
      }
    }

    // Phase 4: Execute instruction from queue
    if (!instruction_queue_.empty()) {
      auto inst = instruction_queue_.front();
      instruction_queue_.pop();

      // Read operands from register file
      int operand_a = readRegister(inst->getSrcRegA());
      int operand_b = readRegister(inst->getSrcRegB());

      // Execute operation using ALU
      int result = ALUComponent::executeOperation(operand_a, operand_b,
                                                  inst->getOperation());

      // Write result back to register file
      writeRegister(inst->getDstReg(), result);

      instructions_executed_++;

      if (verbose_) {
        std::cout << "[" << scheduler_.getCurrentTime() << "] " << getName()
                  << ": Execute "
                  << ALUComponent::getOpName(inst->getOperation()) << " R"
                  << inst->getSrcRegA() << "(" << operand_a << "), R"
                  << inst->getSrcRegB() << "(" << operand_b << ") -> R"
                  << inst->getDstReg() << "(" << result << ")" << std::endl;
      }

      // Output result
      auto result_packet = std::make_shared<IntDataPacket>(result);
      result_packet->setTimestamp(scheduler_.getCurrentTime());
      data_out->write(
          std::static_pointer_cast<Architecture::DataPacket>(result_packet));
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
   * @brief Initialize register with value
   */
  void initRegister(int address, int value) { writeRegister(address, value); }

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
   * @brief Print register file contents
   */
  void printRegisters() const {
    std::cout << "\n=== Register File: " << getName() << " ===" << std::endl;
    for (size_t i = 0; i < num_registers_; ++i) {
      if (i % 8 == 0) std::cout << std::endl;
      std::cout << "R" << i << "=" << register_file_[i] << "\t";
    }
    std::cout << std::endl;
  }

  /**
   * @brief Print statistics
   */
  void printStatistics() const {
    std::cout << "\n=== PE Statistics: " << getName() << " ===" << std::endl;
    std::cout << "Instructions executed: " << instructions_executed_
              << std::endl;
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
  }

  // Enable/disable verbose output
  void setVerbose(bool verbose) { verbose_ = verbose; }

 private:
  size_t num_registers_;            // Number of registers
  size_t queue_depth_;              // Instruction queue depth
  std::vector<int> register_file_;  // Register file storage
  std::queue<std::shared_ptr<PEInstructionPacket>>
      instruction_queue_;              // Instruction queue
  std::shared_ptr<ALUComponent> alu_;  // ALU unit
  uint64_t instructions_executed_;     // Instructions executed count
  uint64_t cycles_stalled_;            // Stall cycles count
  bool input_ready_;                   // Ready to accept instruction
  bool output_valid_;                  // Has valid output
  bool verbose_ = false;               // Verbose output flag
};

#endif  // PE_H
