#ifndef REGFILE_H
#define REGFILE_H

#include <algorithm>
#include <cassert>
#include <cstring>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "../../packet.h"
#include "../../pipeline.h"
#include "../../port.h"
#include "../../tick.h"
#include "../../trace.h"

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
  RegfileReadAddrPacket(uint32_t addr = 0, bool valid_read = true)
      : addr(addr), valid_read(valid_read) {}

  std::shared_ptr<Architecture::DataPacket> clone() const override {
    return cloneImpl<RegfileReadAddrPacket>(addr, valid_read);
  }

  uint32_t addr;
  bool valid_read;
};

/**
 * @brief Read port data response packet
 *
 * Contains data read from register file
 */
class RegfileReadDataPacket : public Architecture::DataPacket {
 public:
  RegfileReadDataPacket(uint32_t data = 0, bool valid_data = true)
      : data(data), valid_data(valid_data) {}

  std::shared_ptr<Architecture::DataPacket> clone() const override {
    return cloneImpl<RegfileReadDataPacket>(data, valid_data);
  }

  uint32_t data;
  bool valid_data;
};

/**
 * @brief Write port command packet
 *
 * Contains address and data for write operation
 */
class RegfileWritePacket : public Architecture::DataPacket {
 public:
  RegfileWritePacket(uint32_t addr = 0, uint32_t data = 0,
                     bool valid_write = true, bool masked = false)
      : addr(addr), data(data), valid_write(valid_write), masked(masked) {}

  std::shared_ptr<Architecture::DataPacket> clone() const override {
    return cloneImpl<RegfileWritePacket>(addr, data, valid_write, masked);
  }

  uint32_t addr;
  uint32_t data;
  bool valid_write;
  bool masked;  // Write masked (under branch shadow in speculative execution)
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
 * @brief Register File Component (Event-Driven Pipeline Version)
 *
 * Event-driven register file that inherits from TickingComponent for automatic
 * tick-based port processing. Processes read/write ports every cycle.
 *
 * Based on Coral NPU design:
 * - Multiple read ports (e.g., 8 per lane × 2 operands = 16 ports)
 * - Multiple write ports (instruction lanes + extra for MLU/DVU/LSU)
 * - Global scoreboard tracking pending writes
 * - Write forwarding to bypass write-read dependency latency
 * - Support for masked writes (under speculation)
 *
 * Architecture:
 * - Single tick cycle that processes all ports
 * - Read operations: fetch data from registers based on address ports
 * - Write operations: update registers from write ports
 * - Scoreboard: tracks pending writes and prevents RAW hazards
 *
 * Port Protocol:
 * - Read: client sets read_addr_N port with register address
 *         RF outputs read_data_N with the register value
 * - Write: connection sets write_addr_N, write_data_N, write_mask_N ports
 *          RF updates registers and clears scoreboard if unmasked
 */
class RegisterFile : public Architecture::TickingComponent {
 public:
  /**
   * @brief Constructor
   * @param name Component name
   * @param scheduler Event scheduler reference
   * @param params Register file parameters
   */
  RegisterFile(const std::string& name, EventDriven::EventScheduler& scheduler,
               const RegisterFileParameters& params)
      : TickingComponent(name, scheduler, 1),  // period = 1 cycle
        params_(params),
        registers_(params.num_registers, 0),
        scoreboard_(params.num_registers),
        scoreboard_prev_(0),  // Initialize previous scoreboard to 0
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

    // Create write ports (for RegisterFileWire connections)
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
   * @brief Write to a register (updates register value only, NOT scoreboard)
   * @param addr Register address
   * @param data Data to write
   * @param masked Whether this write is masked (speculative)
   *
   * Following golden reference (Coral NPU):
   * - Register value is updated here
   * - Scoreboard clearing happens separately in processWrites() when data
   * arrives
   * - This separation ensures dispatch_ctrl_ sees correct availability timing
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
    // NOTE: Scoreboard is NOT cleared here. It will be cleared in
    // processWrites() when the write data is actually received on the write
    // port.
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
    scoreboard_prev_ = 0;
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
   * @brief Get the latency mask for registers that became available this cycle
   */
  uint32_t getLatencyMask() const {
    // Registers that are becoming available this cycle are those that were
    // scoreboarded last cycle but are not scoreboarded this cycle
    return scoreboard_prev_ & ~getScoreboardMask();
  }

  /**
   * @brief Get current scoreboard mask and previous cycle's mask
   */
  void getScoreboardState(uint32_t& comb, uint32_t& regd) const {
    comb = getScoreboardMask();
    regd = scoreboard_prev_;
  }

  /**
   * @brief Event-driven tick: automatically process all ports each cycle
   *
   * Called by Pipeline framework on each tick. This ensures all read and write
   * ports are processed automatically without manual calling from Core.
   */
  void tick() override {
    // Process all read and write ports
    TRACE_COMPUTE(scheduler_.getCurrentTime(), getName(), "REGFILE_TICK",
                  "Starting tick");
    updatePorts();
  } /**
     * @brief Process ports for synchronous update
     *
     * Call this at the end of each cycle to:
     * 1. Read register values from read address ports
     * 2. Process write operations from write ports
     * 3. Update scoreboard
     * 4. Update read data ports
     */
  void updatePorts() {
    write_count_ = 0;  // Reset write count for this cycle

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
  uint32_t
      scoreboard_prev_;  // Previous cycle's scoreboard (registered version)

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
      if (!addr_packet || !addr_packet->valid_read) {
        continue;
      }

      uint32_t addr = addr_packet->addr;
      uint32_t data = readRegister(addr);

      read_port_data_[i] = data;
      read_port_valid_[i] = true;
      total_reads_++;

      // Trace the read operation
      std::stringstream ss;
      ss << "Port " << i << ": x" << addr << " = " << data;
      if (isScoreboardSet(addr)) {
        ss << " (pending)";
      }
      TRACE_COMPUTE(scheduler_.getCurrentTime(), getName(), "REGFILE_READ",
                    ss.str());

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
    TRACE_COMPUTE(scheduler_.getCurrentTime(), getName(),
                  "REGFILE_PROCESS_WRITES_CALLED", "Starting");
    // Collect all write requests
    std::vector<std::pair<uint32_t, uint32_t>> writes;  // (addr, data)
    std::vector<bool> write_valid(params_.num_write_ports, false);
    std::vector<bool> write_masked(params_.num_write_ports, false);

    // Read write port inputs
    for (uint32_t i = 0; i < params_.num_write_ports; ++i) {
      auto addr_port = getPort("write_addr_" + std::to_string(i));
      auto data_port = getPort("write_data_" + std::to_string(i));
      auto mask_port = getPort("write_mask_" + std::to_string(i));

      if (!addr_port || !data_port) {
        continue;
      }

      if (!addr_port->hasData() || !data_port->hasData()) {
        TRACE_COMPUTE(
            scheduler_.getCurrentTime(), getName(), "REGFILE_WRITE_CHECK",
            "Port " << i << ": no data (addr="
                    << (addr_port->hasData() ? "yes" : "no") << " data="
                    << (data_port->hasData() ? "yes" : "no") << ")");
        continue;
      }

      TRACE_COMPUTE(scheduler_.getCurrentTime(), getName(),
                    "REGFILE_WRITE_CHECK", "Port " << i << ": has data");

      uint32_t addr = 0;
      uint32_t data = 0;
      bool found_data = false;

      // Get the data without consuming it first
      auto addr_packet = addr_port->getData();
      auto data_packet = data_port->getData();

      // Try to read as IntDataPacket (from RegisterFileWire)
      auto addr_data =
          std::dynamic_pointer_cast<Architecture::IntDataPacket>(addr_packet);
      auto data_data =
          std::dynamic_pointer_cast<Architecture::IntDataPacket>(data_packet);

      if (addr_data && data_data) {
        addr = static_cast<uint32_t>(addr_data->value);
        data = static_cast<uint32_t>(data_data->value);
        found_data = true;
        // Consume the data after reading
        addr_port->read();
        data_port->read();
      } else {
        // Try legacy RegfileWritePacket format
        auto addr_legacy =
            std::dynamic_pointer_cast<RegfileWritePacket>(addr_packet);
        auto data_legacy =
            std::dynamic_pointer_cast<RegfileWritePacket>(data_packet);

        if (addr_legacy && data_legacy && addr_legacy->valid_write &&
            data_legacy->valid_write) {
          addr = addr_legacy->addr;
          data = data_legacy->data;
          found_data = true;
          // Consume the data after reading
          addr_port->read();
          data_port->read();
        }
      }

      if (!found_data) {
        continue;
      }

      write_valid[i] = true;
      write_masked[i] =
          (mask_port && mask_port->hasData())
              ? std::dynamic_pointer_cast<Architecture::BoolDataPacket>(
                    mask_port->read())
                    ->value
              : false;

      writes.push_back({addr, data});
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
    // write_count_ will be reset by updatePorts()
    std::vector<bool> write_applied(params_.num_registers, false);

    for (uint32_t i = 0; i < writes.size(); ++i) {
      uint32_t addr = writes[i].first;
      uint32_t data = writes[i].second;
      bool is_masked = write_masked[i];

      if (addr == 0 || write_applied[addr]) {
        // x0 is always zero or this address already written
        continue;
      }

      // Apply write
      writeRegister(addr, data, is_masked);
      write_applied[addr] = true;
      write_count_++;

      // Clear scoreboard when write data arrives (unmasked only)
      // Following golden reference (Coral NPU): scoreboard clears when
      // writeData port has data
      if (!is_masked) {
        scoreboard_[addr].valid = false;
        std::cerr << "[processWrites] Cleared scoreboard for x" << addr
                  << " on data arrival (cycle " << scheduler_.getCurrentTime()
                  << ")" << std::endl;
      } else {
        std::cerr << "[processWrites] MASKED write to x" << addr
                  << " - NOT clearing scoreboard" << std::endl;
      }

      // Trace the write operation
      std::stringstream ss;
      ss << "Port " << i << ": x" << addr << " = " << data;
      if (is_masked) {
        ss << " (masked)";
      }
      TRACE_COMPUTE(scheduler_.getCurrentTime(), getName(), "REGFILE_WRITE",
                    ss.str());
    }
  }

  /**
   * @brief Output current scoreboard state
   *
   * Following golden reference (Coral NPU):
   * - regd: Previous cycle's scoreboard (registered version)
   * - comb: Current cycle's scoreboard with this cycle's clears applied
   *   = (current scoreboard) - (registers cleared this cycle)
   *
   * This ensures dispatch_ctrl_ sees when data actually arrives, not just
   * when write operations are dispatched.
   */
  void outputScoreboard() {
    uint32_t scoreboard_mask = getScoreboardMask();

    // Compute which registers were cleared this cycle
    // These are registers that were in scoreboard_prev_ but not in
    // scoreboard_mask
    uint32_t cleared_this_cycle = scoreboard_prev_ & ~scoreboard_mask;

    std::cerr << "[outputScoreboard] Time " << scheduler_.getCurrentTime()
              << ": scoreboard_mask=0x" << std::hex << scoreboard_mask
              << " prev=0x" << scoreboard_prev_ << " cleared=0x"
              << cleared_this_cycle << std::dec << std::endl;

    // Output previous cycle's scoreboard (registered)
    // This is what was in scoreboard at end of previous cycle
    auto regd_port = getPort("scoreboard_regd");
    if (regd_port) {
      auto packet =
          std::make_shared<Architecture::IntDataPacket>(scoreboard_prev_);
      regd_port->write(packet);
    }

    // Output current combinational scoreboard
    // = registered + (clears happening this cycle applied)
    // But since we already updated scoreboard_mask with clears, this is just
    // scoreboard_mask However, to be explicit: comb = scoreboard_prev -
    // cleared_this_cycle
    uint32_t comb_scoreboard = scoreboard_prev_ & ~cleared_this_cycle;
    auto comb_port = getPort("scoreboard_comb");
    if (comb_port) {
      auto packet =
          std::make_shared<Architecture::IntDataPacket>(comb_scoreboard);
      comb_port->write(packet);
    }

    // Update previous scoreboard for next cycle
    scoreboard_prev_ = scoreboard_mask;
  }
};

#endif  // REGFILE_H
