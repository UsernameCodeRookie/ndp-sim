#ifndef REGFILE_H
#define REGFILE_H

#include <algorithm>
#include <cassert>
#include <cstring>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "../packet.h"
#include "../port.h"
#include "../tick.h"
#include "../trace.h"

/**
 * @brief Register File Parameters
 *
 * Parameterizes register file configuration similar to Coral NPU design
 */
struct RegisterFileParameters {
  uint32_t num_registers;    // Total number of registers (typically 32)
  uint32_t num_read_ports;   // Total read ports (typically 16)
  uint32_t num_write_ports;  // Total write ports (instruction lanes + extras)
  uint32_t num_lanes;        // Number of instruction lanes
  uint32_t register_width;   // Width of each register in bits (32 or 64)
  uint32_t address_width;    // Width of address bus
  bool use_scoreboard;       // Enable dependency tracking scoreboard
  bool use_forwarding;       // Enable write-through forwarding
  bool use_debug_module;     // Enable debug read/write port

  RegisterFileParameters(uint32_t num_regs = 32, uint32_t num_read = 16,
                         uint32_t num_write = 8, uint32_t num_lanes_val = 4,
                         uint32_t reg_width = 32, bool scoreboard = true,
                         bool forwarding = true, bool debug = false)
      : num_registers(num_regs),
        num_read_ports(num_read),
        num_write_ports(num_write),
        num_lanes(num_lanes_val),
        register_width(reg_width),
        address_width(5),  // log2(32) = 5 for 32 registers
        use_scoreboard(scoreboard),
        use_forwarding(forwarding),
        use_debug_module(debug) {
    // Validate configuration
    assert(num_registers > 0 && num_registers <= 256);
    assert(num_read_ports > 0);
    assert(num_write_ports > 0);
    assert(register_width == 32 || register_width == 64);
    assert(address_width == 5 || address_width == 8);
  }
};

/**
 * @brief Read port address packet
 *
 * Used for specifying which register to read
 */
class RegfileReadAddrPacket : public Architecture::DataPacket {
 public:
  RegfileReadAddrPacket(uint32_t addr, bool valid = true)
      : addr_(addr), valid_read_(valid) {}

  uint32_t getAddress() const { return addr_; }
  bool isValidRead() const { return valid_read_; }

  void setAddress(uint32_t addr) { addr_ = addr; }
  void setValidRead(bool valid) { valid_read_ = valid; }

  std::shared_ptr<Architecture::DataPacket> clone() const override {
    return cloneImpl<RegfileReadAddrPacket>(addr_, valid_read_);
  }

 private:
  uint32_t addr_;
  bool valid_read_;
};

/**
 * @brief Read port data response packet
 *
 * Contains data read from register file
 */
class RegfileReadDataPacket : public Architecture::DataPacket {
 public:
  RegfileReadDataPacket(uint32_t data, bool valid = true)
      : data_(data), valid_data_(valid) {}

  uint32_t getData() const { return data_; }
  bool isValidData() const { return valid_data_; }

  void setData(uint32_t data) { data_ = data; }
  void setValidData(bool valid) { valid_data_ = valid; }

  std::shared_ptr<Architecture::DataPacket> clone() const override {
    return cloneImpl<RegfileReadDataPacket>(data_, valid_data_);
  }

 private:
  uint32_t data_;
  bool valid_data_;
};

/**
 * @brief Write port command packet
 *
 * Contains address and data for write operation
 */
class RegfileWritePacket : public Architecture::DataPacket {
 public:
  RegfileWritePacket(uint32_t addr, uint32_t data, bool valid = true,
                     bool masked = false)
      : addr_(addr), data_(data), valid_write_(valid), masked_(masked) {}

  uint32_t getAddress() const { return addr_; }
  uint32_t getData() const { return data_; }
  bool isValidWrite() const { return valid_write_; }
  bool isMasked() const { return masked_; }

  void setAddress(uint32_t addr) { addr_ = addr; }
  void setData(uint32_t data) { data_ = data; }
  void setValidWrite(bool valid) { valid_write_ = valid; }
  void setMasked(bool masked) { masked_ = masked; }

  std::shared_ptr<Architecture::DataPacket> clone() const override {
    return cloneImpl<RegfileWritePacket>(addr_, data_, valid_write_, masked_);
  }

 private:
  uint32_t addr_;
  uint32_t data_;
  bool valid_write_;
  bool masked_;  // Write masked (under branch shadow in speculative execution)
};

/**
 * @brief Scoreboard entry for dependency tracking
 */
struct ScoreboardEntry {
  bool valid;     // Whether register has pending write
  uint32_t mask;  // Bit mask for partial writes (future use)

  ScoreboardEntry() : valid(false), mask(0) {}
  ScoreboardEntry(bool v, uint32_t m = 0) : valid(v), mask(m) {}
};

/**
 * @brief Register File Component
 *
 * Multi-port register file with optional scoreboard and write forwarding
 * Based on Coral NPU design:
 * - Multiple read ports (e.g., 8 per lane × 2 operands = 16 ports)
 * - Multiple write ports (instruction lanes + extra for MLU/DVU/LSU)
 * - Global scoreboard tracking pending writes
 * - Write forwarding to bypass write-read dependency latency
 * - Support for masked writes (under speculation)
 *
 * Architecture:
 * - Register storage: uint32_t array
 * - Scoreboard: tracks which registers have pending writes
 * - Write priority encoder: handles multi-writer conflicts
 * - Read forwarding logic: provides latest data from pending writes
 */
class RegisterFile : public Architecture::Component {
 public:
  /**
   * @brief Constructor
   * @param name Component name
   * @param scheduler Event scheduler reference
   * @param params Register file parameters
   */
  RegisterFile(const std::string& name, EventDriven::EventScheduler& scheduler,
               const RegisterFileParameters& params)
      : Component(name, scheduler),
        params_(params),
        registers_(params.num_registers, 0),
        scoreboard_(params.num_registers),
        read_port_data_(params.num_read_ports),
        read_port_valid_(params.num_read_ports, false),
        total_reads_(0),
        total_writes_(0),
        total_forwards_(0),
        total_conflicts_(0),
        write_count_(0) {
    // Create read address ports (one per read port)
    for (uint32_t i = 0; i < params_.num_read_ports; ++i) {
      addPort("read_addr_" + std::to_string(i),
              Architecture::PortDirection::INPUT);
    }

    // Create read data ports (one per read port)
    for (uint32_t i = 0; i < params_.num_read_ports; ++i) {
      addPort("read_data_" + std::to_string(i),
              Architecture::PortDirection::OUTPUT);
    }

    // Create write ports
    for (uint32_t i = 0; i < params_.num_write_ports; ++i) {
      addPort("write_addr_" + std::to_string(i),
              Architecture::PortDirection::INPUT);
      addPort("write_data_" + std::to_string(i),
              Architecture::PortDirection::INPUT);
      addPort("write_mask_" + std::to_string(i),
              Architecture::PortDirection::INPUT);
    }

    // Scoreboard output
    addPort("scoreboard_regd", Architecture::PortDirection::OUTPUT);
    addPort("scoreboard_comb", Architecture::PortDirection::OUTPUT);

    // Debug port (optional)
    if (params_.use_debug_module) {
      addPort("debug_read_addr", Architecture::PortDirection::INPUT);
      addPort("debug_read_data", Architecture::PortDirection::OUTPUT);
      addPort("debug_write_valid", Architecture::PortDirection::INPUT);
    }

    // Statistics port
    addPort("write_count", Architecture::PortDirection::OUTPUT);
  }

  /**
   * @brief Read a register value
   * @param addr Register address
   * @return Register value at address
   */
  uint32_t readRegister(uint32_t addr) const {
    if (addr >= params_.num_registers) {
      return 0;
    }
    return registers_[addr];
  }

  /**
   * @brief Write to a register
   * @param addr Register address
   * @param data Data to write
   * @param masked Whether this write is masked (speculative)
   */
  void writeRegister(uint32_t addr, uint32_t data, bool masked = false) {
    if (addr == 0) {
      // Register 0 is always zero, ignore writes
      return;
    }
    if (addr >= params_.num_registers) {
      return;
    }

    registers_[addr] = data;
    if (!masked) {
      scoreboard_[addr].valid = false;  // Clear dependency
    }
    total_writes_++;
  }

  /**
   * @brief Mark register as having pending write (set scoreboard)
   * @param addr Register address
   */
  void setScoreboard(uint32_t addr) {
    if (addr == 0) {
      // Register 0 scoreboard never set
      return;
    }
    if (addr < params_.num_registers) {
      scoreboard_[addr].valid = true;
    }
  }

  /**
   * @brief Check if register has pending write
   * @param addr Register address
   * @return true if register is busy
   */
  bool isScoreboardSet(uint32_t addr) const {
    if (addr >= params_.num_registers) {
      return false;
    }
    return scoreboard_[addr].valid;
  }

  /**
   * @brief Get scoreboard value as bitmask
   * @return 32-bit mask of pending registers
   */
  uint32_t getScoreboardMask() const {
    uint32_t mask = 0;
    for (uint32_t i = 1; i < params_.num_registers; ++i) {
      if (scoreboard_[i].valid) {
        mask |= (1u << i);
      }
    }
    return mask;
  }

  /**
   * @brief Reset all registers to zero
   */
  void reset() override {
    std::fill(registers_.begin(), registers_.end(), 0);
    for (auto& entry : scoreboard_) {
      entry.valid = false;
    }
    total_reads_ = 0;
    total_writes_ = 0;
    total_forwards_ = 0;
    total_conflicts_ = 0;
    write_count_ = 0;
  }

  /**
   * @brief Get total read operations
   */
  uint64_t getTotalReads() const { return total_reads_; }

  /**
   * @brief Get total write operations
   */
  uint64_t getTotalWrites() const { return total_writes_; }

  /**
   * @brief Get total forwarded reads
   */
  uint64_t getTotalForwards() const { return total_forwards_; }

  /**
   * @brief Get total write conflicts detected
   */
  uint64_t getTotalConflicts() const { return total_conflicts_; }

  /**
   * @brief Get write count for this cycle
   */
  uint32_t getWriteCount() const { return write_count_; }

  /**
   * @brief Process ports for synchronous update
   *
   * Call this at the end of each cycle to:
   * 1. Read register values from read address ports
   * 2. Process write operations from write ports
   * 3. Update scoreboard
   * 4. Update read data ports
   */
  void updatePorts() {
    // Process write operations and update scoreboard
    processWrites();

    // Process read operations
    processReads();

    // Output current statistics
    auto write_count_port = getPort("write_count");
    if (write_count_port) {
      auto packet = std::make_shared<Architecture::IntDataPacket>(write_count_);
      write_count_port->write(packet);
    }

    // Output scoreboard values
    outputScoreboard();
  }

  /**
   * @brief Print register file statistics
   */
  void printStatistics() const {
    std::cout << "\n=== Register File Statistics: " << getName()
              << " ===" << std::endl;
    std::cout << "Total registers: " << params_.num_registers << std::endl;
    std::cout << "Total read ports: " << params_.num_read_ports << std::endl;
    std::cout << "Total write ports: " << params_.num_write_ports << std::endl;
    std::cout << "Register width: " << params_.register_width << " bits"
              << std::endl;
    std::cout << "Total read operations: " << total_reads_ << std::endl;
    std::cout << "Total write operations: " << total_writes_ << std::endl;
    std::cout << "Total forwarded reads: " << total_forwards_ << std::endl;
    std::cout << "Total write conflicts: " << total_conflicts_ << std::endl;
    std::cout << "Pending writes in scoreboard: " << getScoreboardMask()
              << std::endl;
  }

  /**
   * @brief Dump register values
   */
  void dumpRegisters() const {
    std::cout << "\n=== Register Values ===" << std::endl;
    for (uint32_t i = 0; i < params_.num_registers; ++i) {
      std::cout << "x" << i << " = 0x" << std::hex << registers_[i] << std::dec
                << std::endl;
    }
  }

  /**
   * @brief Get parameters
   */
  const RegisterFileParameters& getParameters() const { return params_; }

 private:
  RegisterFileParameters params_;

  // Register storage
  std::vector<uint32_t> registers_;

  // Scoreboard for dependency tracking
  std::vector<ScoreboardEntry> scoreboard_;

  // Read port state
  std::vector<uint32_t> read_port_data_;
  std::vector<bool> read_port_valid_;

  // Statistics
  uint64_t total_reads_;
  uint64_t total_writes_;
  uint64_t total_forwards_;
  uint64_t total_conflicts_;
  uint32_t write_count_;  // Number of writes this cycle

  /**
   * @brief Process read operations
   */
  void processReads() {
    read_port_valid_.assign(params_.num_read_ports, false);

    // Process each read port
    for (uint32_t i = 0; i < params_.num_read_ports; ++i) {
      auto addr_port = getPort("read_addr_" + std::to_string(i));
      if (!addr_port || !addr_port->hasData()) {
        continue;
      }

      auto addr_packet =
          std::dynamic_pointer_cast<RegfileReadAddrPacket>(addr_port->read());
      if (!addr_packet || !addr_packet->isValidRead()) {
        continue;
      }

      uint32_t addr = addr_packet->getAddress();
      uint32_t data = readRegister(addr);

      read_port_data_[i] = data;
      read_port_valid_[i] = true;
      total_reads_++;

      // Check for forwarding (read from pending write)
      if (params_.use_forwarding && isScoreboardSet(addr)) {
        total_forwards_++;
      }

      // Output read data
      auto data_port = getPort("read_data_" + std::to_string(i));
      if (data_port) {
        auto data_packet =
            std::make_shared<RegfileReadDataPacket>(data, read_port_valid_[i]);
        data_port->write(
            std::static_pointer_cast<Architecture::DataPacket>(data_packet));
      }
    }
  }

  /**
   * @brief Process write operations and detect conflicts
   */
  void processWrites() {
    // Collect all write requests
    std::vector<std::pair<uint32_t, uint32_t>> writes;  // (addr, data)
    std::vector<bool> write_valid(params_.num_write_ports, false);
    std::vector<bool> write_masked(params_.num_write_ports, false);

    // Read write port inputs
    for (uint32_t i = 0; i < params_.num_write_ports; ++i) {
      auto addr_port = getPort("write_addr_" + std::to_string(i));
      auto data_port = getPort("write_data_" + std::to_string(i));
      auto mask_port = getPort("write_mask_" + std::to_string(i));

      if (!addr_port || !data_port || !addr_port->hasData() ||
          !data_port->hasData()) {
        continue;
      }

      auto addr_packet =
          std::dynamic_pointer_cast<RegfileWritePacket>(addr_port->read());
      auto data_packet =
          std::dynamic_pointer_cast<RegfileWritePacket>(data_port->read());

      if (!addr_packet || !addr_packet->isValidWrite()) {
        continue;
      }

      write_valid[i] = true;
      write_masked[i] =
          (mask_port && mask_port->hasData())
              ? std::dynamic_pointer_cast<Architecture::BoolDataPacket>(
                    mask_port->read())
                    ->getValue()
              : false;

      writes.push_back({addr_packet->getAddress(), data_packet->getData()});
    }

    // Detect conflicts (multiple writes to same register)
    for (uint32_t i = 0; i < writes.size(); ++i) {
      for (uint32_t j = i + 1; j < writes.size(); ++j) {
        if (writes[i].first == writes[j].first && writes[i].first != 0) {
          total_conflicts_++;
        }
      }
    }

    // Priority encode writes (first writer wins)
    write_count_ = 0;
    std::vector<bool> write_applied(params_.num_registers, false);

    for (uint32_t i = 0; i < writes.size(); ++i) {
      uint32_t addr = writes[i].first;
      uint32_t data = writes[i].second;

      if (addr == 0 || write_applied[addr]) {
        // x0 is always zero or this address already written
        continue;
      }

      // Apply write
      writeRegister(addr, data, write_masked[i]);
      write_applied[addr] = true;
      write_count_++;

      // Update scoreboard if not masked
      if (!write_masked[i]) {
        setScoreboard(addr);
      }
    }
  }

  /**
   * @brief Output current scoreboard state
   */
  void outputScoreboard() {
    uint32_t scoreboard_mask = getScoreboardMask();

    auto regd_port = getPort("scoreboard_regd");
    if (regd_port) {
      auto packet =
          std::make_shared<Architecture::IntDataPacket>(scoreboard_mask);
      regd_port->write(packet);
    }

    auto comb_port = getPort("scoreboard_comb");
    if (comb_port) {
      auto packet =
          std::make_shared<Architecture::IntDataPacket>(scoreboard_mask);
      comb_port->write(packet);
    }
  }
};

#endif  // REGFILE_H
