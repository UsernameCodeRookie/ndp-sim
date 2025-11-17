#ifndef RVV_RETIRE_H
#define RVV_RETIRE_H

#include <algorithm>
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "../../component.h"
#include "../../event.h"
#include "../../packet.h"
#include "../../port.h"
#include "../../scheduler.h"
#include "../../tick.h"
#include "../../trace.h"
#include "rvv_rob.h"

namespace Architecture {

/**
 * @brief Write-After-Write (WAW) Hazard Detector
 *
 * Detects and resolves conflicts when multiple instructions write to same
 * register in the same cycle. Uses byte-enable masking: later write wins.
 */
class WAWHazardResolver {
 public:
  /**
   * @brief Resolve byte-enable for two writes to same register
   *
   * If both write to same register, later write (higher index) wins
   * Result: earlier write disables bytes that later write enables
   *
   * @param be0 Earlier write byte-enable
   * @param be1 Later write byte-enable
   * @return Masked byte-enable for earlier write
   */
  static std::vector<bool> resolveTwo(const std::vector<bool>& be0,
                                      const std::vector<bool>& be1) {
    std::vector<bool> result = be0;
    for (size_t i = 0; i < result.size() && i < be1.size(); ++i) {
      if (be1[i]) {
        result[i] = false;  // Later write wins
      }
    }
    return result;
  }

  /**
   * @brief Resolve byte-enable for three writes to same register
   *
   * Applies two-stage resolution: first resolve 1&2, then resolve 0 with result
   *
   * @param be0 Earliest write byte-enable
   * @param be1 Middle write byte-enable
   * @param be2 Latest write byte-enable
   * @param be0_out Output byte-enable for write 0
   * @param be1_out Output byte-enable for write 1
   */
  static void resolveThree(const std::vector<bool>& be0,
                           const std::vector<bool>& be1,
                           const std::vector<bool>& be2,
                           std::vector<bool>& be0_out,
                           std::vector<bool>& be1_out) {
    // First resolve be1 and be2
    auto be1_masked = resolveTwo(be1, be2);

    // Then resolve be0 with masked be1 (accounting for be2)
    std::vector<bool> be1_combined = be1_masked;
    for (size_t i = 0; i < be1_combined.size() && i < be2.size(); ++i) {
      be1_combined[i] = be1_combined[i] || be2[i];  // Effective write
    }

    be0_out = resolveTwo(be0, be1_combined);
    be1_out = be1_masked;
  }

  /**
   * @brief Resolve byte-enable for four writes to same register
   *
   * @param be0 Earliest write byte-enable
   * @param be1 Second write byte-enable
   * @param be2 Third write byte-enable
   * @param be3 Latest write byte-enable
   * @param be0_out Output for write 0
   * @param be1_out Output for write 1
   * @param be2_out Output for write 2
   */
  static void resolveFour(const std::vector<bool>& be0,
                          const std::vector<bool>& be1,
                          const std::vector<bool>& be2,
                          const std::vector<bool>& be3,
                          std::vector<bool>& be0_out,
                          std::vector<bool>& be1_out,
                          std::vector<bool>& be2_out) {
    // Resolve be2 and be3 first
    auto be2_masked = resolveTwo(be2, be3);

    // Resolve be1 with be2_masked (accounting for be3)
    std::vector<bool> be2_combined = be2_masked;
    for (size_t i = 0; i < be2_combined.size() && i < be3.size(); ++i) {
      be2_combined[i] = be2_combined[i] || be3[i];
    }
    auto be1_masked = resolveTwo(be1, be2_combined);

    // Resolve be0 with result
    std::vector<bool> be1_combined = be1_masked;
    for (size_t i = 0; i < be1_combined.size() && i < be2_combined.size();
         ++i) {
      be1_combined[i] = be1_combined[i] || be2_combined[i];
    }

    be0_out = resolveTwo(be0, be1_combined);
    be1_out = be1_masked;
    be2_out = be2_masked;
  }
};

/**
 * @brief RVV Retire Stage
 *
 * Manages instruction writeback from ROB to register files
 * Features:
 * - Multi-port write arbitration (up to 4 simultaneous writes)
 * - WAW hazard resolution through byte-enable masking
 * - Support for VRF and XRF writes
 * - Trap/exception handling with pipeline flush logic
 * - Saturation flag (VXSAT) management
 *
 * All logic is combinational (no internal pipeline stages)
 */
class RVVRetireStage : public Component {
 public:
  /**
   * @brief Write request to register file
   */
  struct WriteRequest {
    uint64_t rob_index;
    uint32_t dest_reg;
    std::vector<uint8_t> data;
    std::vector<bool> byte_enable;
    std::vector<bool> vxsat_flags;
    uint8_t dest_type;  // 0=VRF, 1=XRF
    bool trap_flag;

    WriteRequest() = default;
    WriteRequest(uint64_t idx, uint32_t reg, const std::vector<uint8_t>& d,
                 const std::vector<bool>& be, uint8_t dtype)
        : rob_index(idx),
          dest_reg(reg),
          data(d),
          byte_enable(be),
          dest_type(dtype),
          trap_flag(false) {}
  };

  /**
   * @brief Constructor
   *
   * @param name Component name
   * @param scheduler Event scheduler
   * @param vlen Vector length in bits
   * @param num_write_ports Number of write ports (typically 4)
   */
  RVVRetireStage(const std::string& name,
                 EventDriven::EventScheduler& scheduler, uint32_t vlen = 128,
                 size_t num_write_ports = 4)
      : Component(name, scheduler),
        vlen_(vlen),
        num_write_ports_(num_write_ports),
        bytes_per_reg_(vlen / 8),
        writes_this_cycle_(0),
        vrf_writes_(0),
        xrf_writes_(0),
        waw_collisions_(0),
        traps_handled_(0) {
    addPort("rob_in", PortDirection::INPUT);
    addPort("vrf_write", PortDirection::OUTPUT);
    addPort("xrf_write", PortDirection::OUTPUT);
  }

  /**
   * @brief Process ROB entries for writeback
   *
   * Applies WAW hazard resolution and generates write requests
   *
   * @param rob_entries Entries from ROB ready to retire
   * @return Vector of processed write requests
   */
  std::vector<WriteRequest> processRetireEntries(
      const std::vector<ROBEntry>& rob_entries) {
    std::vector<WriteRequest> writes;
    writes_this_cycle_ = 0;

    // Group entries by destination register
    std::map<uint32_t, std::vector<const ROBEntry*>> writes_by_dest;

    for (const auto& entry : rob_entries) {
      if (entry.dest_valid) {
        writes_by_dest[entry.dest_reg].push_back(&entry);
      }
    }

    // Process each register with WAW detection
    for (auto& [dest_reg, entries] : writes_by_dest) {
      if (entries.size() == 1) {
        // Single write - no WAW
        const ROBEntry* entry = entries[0];
        WriteRequest wr(entry->rob_index, dest_reg, entry->result_data,
                        entry->byte_enable, entry->dest_type);
        wr.trap_flag = entry->trap_flag;

        // Check if any write is a trap - should stop subsequent writes
        if (entry->trap_flag) {
          writes.push_back(wr);
          break;  // Trap terminates retirement
        }

        writes.push_back(wr);

      } else if (entries.size() == 2) {
        // Two writes to same register - WAW hazard
        waw_collisions_++;

        const ROBEntry* entry0 = entries[0];
        const ROBEntry* entry1 = entries[1];

        // Later write wins
        auto be0 = WAWHazardResolver::resolveTwo(entry0->byte_enable,
                                                 entry1->byte_enable);

        WriteRequest wr0(entry0->rob_index, dest_reg, entry0->result_data, be0,
                         entry0->dest_type);
        WriteRequest wr1(entry1->rob_index, dest_reg, entry1->result_data,
                         entry1->byte_enable, entry1->dest_type);

        wr0.trap_flag = entry0->trap_flag;
        wr1.trap_flag = entry1->trap_flag;

        writes.push_back(wr0);
        if (!entry0->trap_flag) {
          writes.push_back(wr1);
        }

      } else if (entries.size() == 3) {
        // Three writes to same register - WAW hazard
        waw_collisions_++;

        const ROBEntry* entry0 = entries[0];
        const ROBEntry* entry1 = entries[1];
        const ROBEntry* entry2 = entries[2];

        std::vector<bool> be0, be1;
        WAWHazardResolver::resolveThree(entry0->byte_enable,
                                        entry1->byte_enable,
                                        entry2->byte_enable, be0, be1);

        WriteRequest wr0(entry0->rob_index, dest_reg, entry0->result_data, be0,
                         entry0->dest_type);
        WriteRequest wr1(entry1->rob_index, dest_reg, entry1->result_data, be1,
                         entry1->dest_type);
        WriteRequest wr2(entry2->rob_index, dest_reg, entry2->result_data,
                         entry2->byte_enable, entry2->dest_type);

        wr0.trap_flag = entry0->trap_flag;
        wr1.trap_flag = entry1->trap_flag;
        wr2.trap_flag = entry2->trap_flag;

        writes.push_back(wr0);
        if (!entry0->trap_flag) {
          writes.push_back(wr1);
        }
        if (!entry0->trap_flag && !entry1->trap_flag) {
          writes.push_back(wr2);
        }

      } else if (entries.size() == 4) {
        // Four writes to same register - WAW hazard
        waw_collisions_++;

        const ROBEntry* entry0 = entries[0];
        const ROBEntry* entry1 = entries[1];
        const ROBEntry* entry2 = entries[2];
        const ROBEntry* entry3 = entries[3];

        std::vector<bool> be0, be1, be2;
        WAWHazardResolver::resolveFour(entry0->byte_enable, entry1->byte_enable,
                                       entry2->byte_enable, entry3->byte_enable,
                                       be0, be1, be2);

        WriteRequest wr0(entry0->rob_index, dest_reg, entry0->result_data, be0,
                         entry0->dest_type);
        WriteRequest wr1(entry1->rob_index, dest_reg, entry1->result_data, be1,
                         entry1->dest_type);
        WriteRequest wr2(entry2->rob_index, dest_reg, entry2->result_data, be2,
                         entry2->dest_type);
        WriteRequest wr3(entry3->rob_index, dest_reg, entry3->result_data,
                         entry3->byte_enable, entry3->dest_type);

        wr0.trap_flag = entry0->trap_flag;
        wr1.trap_flag = entry1->trap_flag;
        wr2.trap_flag = entry2->trap_flag;
        wr3.trap_flag = entry3->trap_flag;

        writes.push_back(wr0);
        if (!entry0->trap_flag) {
          writes.push_back(wr1);
        }
        if (!entry0->trap_flag && !entry1->trap_flag) {
          writes.push_back(wr2);
        }
        if (!entry0->trap_flag && !entry1->trap_flag && !entry2->trap_flag) {
          writes.push_back(wr3);
        }
      }

      // Check for trap - stop processing further
      for (const auto& entry : entries) {
        if (entry->trap_flag) {
          traps_handled_++;
          break;
        }
      }
    }

    writes_this_cycle_ = writes.size();
    for (const auto& w : writes) {
      if (w.dest_type == 0) {
        vrf_writes_++;
      } else {
        xrf_writes_++;
      }
    }

    return writes;
  }

  /**
   * @brief Get statistics
   */
  uint64_t getWritesThisCycle() const { return writes_this_cycle_; }
  uint64_t getVRFWrites() const { return vrf_writes_; }
  uint64_t getXRFWrites() const { return xrf_writes_; }
  uint64_t getWAWCollisions() const { return waw_collisions_; }
  uint64_t getTrapsHandled() const { return traps_handled_; }

  /**
   * @brief Reset statistics
   */
  void resetStatistics() {
    writes_this_cycle_ = 0;
    vrf_writes_ = 0;
    xrf_writes_ = 0;
    waw_collisions_ = 0;
    traps_handled_ = 0;
  }

 private:
  uint32_t vlen_;
  size_t num_write_ports_;
  size_t bytes_per_reg_;

  // Per-cycle stats
  uint64_t writes_this_cycle_;
  uint64_t vrf_writes_;
  uint64_t xrf_writes_;
  uint64_t waw_collisions_;
  uint64_t traps_handled_;
};

}  // namespace Architecture

#endif  // RVV_RETIRE_H
