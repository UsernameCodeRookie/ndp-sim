#ifndef RVV_REGFILE_H
#define RVV_REGFILE_H

#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include "../packet.h"
#include "../port.h"
#include "../tick.h"
#include "../trace.h"
#include "component.h"

namespace Architecture {

/**
 * @brief RVV Vector Register File - 32 vector registers
 *
 * Models the CoralNPU RVV backend Vector Register File with:
 * - 32 vector registers (v0-v31)
 * - 4 read ports for dispatch unit
 * - 4 write ports for retire/writeback
 * - Byte-enable granularity write control
 * - Full VLEN width support (128/256/512 bits)
 *
 * Characteristics:
 * - v0 is special (used for mask register)
 * - Single-cycle read/write operations
 * - No write forwarding (WAR/WAW handled by scoreboard)
 * - Byte-enable writes for partial updates
 */
class RVVVectorRegisterFile : public TickingComponent {
 public:
  /**
   * @brief Constructor
   * @param name Component name
   * @param scheduler Event scheduler
   * @param period Clock period
   * @param num_registers Number of vector registers (typically 32)
   * @param vlen Vector length in bits (128/256/512)
   * @param read_ports Number of read ports (typically 4)
   * @param write_ports Number of write ports (typically 4)
   */
  RVVVectorRegisterFile(const std::string& name,
                        EventDriven::EventScheduler& scheduler, uint64_t period,
                        size_t num_registers = 32, uint32_t vlen = 128,
                        size_t read_ports = 4, size_t write_ports = 4)
      : TickingComponent(name, scheduler, period),
        num_registers_(num_registers),
        vlen_(vlen),
        num_read_ports_(read_ports),
        num_write_ports_(write_ports),
        registers_(num_registers, std::vector<uint8_t>(vlen / 8, 0)),
        read_count_(0),
        write_count_(0) {
    // Create read and write ports
    for (size_t i = 0; i < num_read_ports_; ++i) {
      addPort("rd_" + std::to_string(i), PortDirection::OUTPUT);
    }
    for (size_t i = 0; i < num_write_ports_; ++i) {
      addPort("wr_" + std::to_string(i), PortDirection::INPUT);
    }
  }

  /**
   * @brief Write data to a register with byte-enable control
   * @param reg_index Register index (0-31)
   * @param data Data to write (size must match vlen)
   * @param byte_enable Byte enable mask (1 bit per byte)
   * @return true if write successful
   */
  bool write(uint32_t reg_index, const std::vector<uint8_t>& data,
             const std::vector<bool>& byte_enable = {}) {
    if (reg_index >= num_registers_) {
      return false;
    }

    if (data.size() != registers_[reg_index].size()) {
      return false;
    }

    // If byte_enable is not provided, enable all bytes
    if (byte_enable.empty()) {
      registers_[reg_index] = data;
    } else {
      if (byte_enable.size() != data.size()) {
        return false;
      }

      // Apply byte-enable mask
      for (size_t i = 0; i < data.size(); ++i) {
        if (byte_enable[i]) {
          registers_[reg_index][i] = data[i];
        }
      }
    }

    write_count_++;
    return true;
  }

  /**
   * @brief Read data from a register
   * @param reg_index Register index (0-31)
   * @return Register data (size = vlen/8)
   */
  std::vector<uint8_t> read(uint32_t reg_index) const {
    if (reg_index >= num_registers_) {
      return std::vector<uint8_t>(vlen_ / 8, 0);
    }

    read_count_++;
    return registers_[reg_index];
  }

  /**
   * @brief Get mask register (v0)
   * @return Mask register data
   */
  std::vector<uint8_t> getMaskRegister() const { return read(0); }

  /**
   * @brief Set mask register (v0)
   * @param mask_data Mask data
   * @return true if successful
   */
  bool setMaskRegister(const std::vector<uint8_t>& mask_data) {
    return write(0, mask_data);
  }

  /**
   * @brief Clear a register (set all bytes to 0)
   * @param reg_index Register index (0-31)
   * @return true if successful
   */
  bool clear(uint32_t reg_index) {
    if (reg_index >= num_registers_) {
      return false;
    }
    std::fill(registers_[reg_index].begin(), registers_[reg_index].end(), 0);
    return true;
  }

  /**
   * @brief Clear all registers
   */
  void clearAll() {
    for (auto& reg : registers_) {
      std::fill(reg.begin(), reg.end(), 0);
    }
  }

  /**
   * @brief Check if register is non-zero
   * @param reg_index Register index
   * @return true if register contains non-zero data
   */
  bool isNonZero(uint32_t reg_index) const {
    if (reg_index >= num_registers_) {
      return false;
    }

    for (const auto& byte : registers_[reg_index]) {
      if (byte != 0) {
        return true;
      }
    }
    return false;
  }

  /**
   * @brief Get number of registers
   */
  size_t getNumRegisters() const { return num_registers_; }

  /**
   * @brief Get vector length in bits
   */
  uint32_t getVectorLength() const { return vlen_; }

  /**
   * @brief Get number of read ports
   */
  size_t getNumReadPorts() const { return num_read_ports_; }

  /**
   * @brief Get number of write ports
   */
  size_t getNumWritePorts() const { return num_write_ports_; }

  /**
   * @brief Get total read count
   */
  uint64_t getReadCount() const { return read_count_; }

  /**
   * @brief Get total write count
   */
  uint64_t getWriteCount() const { return write_count_; }

  /**
   * @brief Reset statistics
   */
  void resetStatistics() {
    read_count_ = 0;
    write_count_ = 0;
  }

  /**
   * @brief Dump register contents for debugging
   * @return String representation of register file state
   */
  std::string dumpRegisters() const {
    std::string result;
    for (size_t i = 0; i < num_registers_; ++i) {
      result += "v" + std::to_string(i) + ": ";
      // Print first 4 bytes as hex
      for (size_t j = 0; j < std::min(size_t(4), registers_[i].size()); ++j) {
        char buf[3];
        snprintf(buf, sizeof(buf), "%02x", registers_[i][j]);
        result += buf;
      }
      if (registers_[i].size() > 4) {
        result += "...";
      }
      result += " ";
    }
    return result;
  }

 protected:
  /**
   * @brief Tick handler (empty for register file)
   */
  void tick() override {
    // Register file is combinational, no cycle-to-cycle updates needed
  }

 private:
  size_t num_registers_;    // Number of vector registers (32)
  uint32_t vlen_;           // Vector length in bits
  size_t num_read_ports_;   // Number of simultaneous read ports
  size_t num_write_ports_;  // Number of simultaneous write ports

  // Register storage: 32 registers x (vlen/8) bytes each
  std::vector<std::vector<uint8_t>> registers_;

  // Statistics
  mutable uint64_t read_count_;
  mutable uint64_t write_count_;
};

}  // namespace Architecture

#endif  // RVV_REGFILE_H
