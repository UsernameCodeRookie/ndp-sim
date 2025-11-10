#ifndef PIPELINE_H
#define PIPELINE_H

#include <deque>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "../port.h"
#include "../tick_component.h"
#include "int_packet.h"

/**
 * @brief Pipeline stage data structure
 *
 * Represents data in a pipeline stage
 */
struct PipelineStageData {
  std::shared_ptr<Architecture::DataPacket> data;
  uint64_t stage_entry_time;  // When data entered this stage
  bool valid;                 // Whether this stage contains valid data

  PipelineStageData() : data(nullptr), stage_entry_time(0), valid(false) {}

  PipelineStageData(std::shared_ptr<Architecture::DataPacket> d, uint64_t time)
      : data(d), stage_entry_time(time), valid(true) {}
};

/**
 * @brief Pipeline component
 *
 * Multi-stage pipeline that processes data through sequential stages
 * Each stage can perform transformations on the data
 */
class PipelineComponent : public Architecture::TickingComponent {
 public:
  using StageFunction = std::function<std::shared_ptr<Architecture::DataPacket>(
      std::shared_ptr<Architecture::DataPacket>)>;

  /**
   * @brief Constructor
   * @param name Component name
   * @param scheduler Event scheduler reference
   * @param period Tick period
   * @param num_stages Number of pipeline stages
   */
  PipelineComponent(const std::string& name,
                    EventDriven::EventScheduler& scheduler, uint64_t period,
                    size_t num_stages = 3)
      : Architecture::TickingComponent(name, scheduler, period),
        num_stages_(num_stages),
        stall_(false),
        flush_(false),
        total_processed_(0),
        total_stalls_(0) {
    // Initialize pipeline stages
    stages_.resize(num_stages_);

    // Initialize default stage functions (identity function)
    stage_functions_.resize(num_stages_);
    for (size_t i = 0; i < num_stages_; ++i) {
      stage_functions_[i] = [](std::shared_ptr<Architecture::DataPacket> data) {
        return data;  // Default: pass through
      };
    }

    // Create input port
    auto in = std::make_shared<Architecture::Port>(
        "in", Architecture::PortDirection::INPUT, this);
    addPort(in);

    // Create output port
    auto out = std::make_shared<Architecture::Port>(
        "out", Architecture::PortDirection::OUTPUT, this);
    addPort(out);

    // Create stall control port (optional)
    auto stall_ctrl = std::make_shared<Architecture::Port>(
        "stall", Architecture::PortDirection::INPUT, this);
    addPort(stall_ctrl);
  }

  /**
   * @brief Set processing function for a specific stage
   * @param stage_index Stage index (0-based)
   * @param func Processing function
   */
  void setStageFunction(size_t stage_index, StageFunction func) {
    if (stage_index < num_stages_) {
      stage_functions_[stage_index] = func;
    }
  }

  /**
   * @brief Main tick function
   */
  void tick() override {
    auto in = getPort("in");
    auto out = getPort("out");
    auto stall_ctrl = getPort("stall");

    // Check stall control
    if (stall_ctrl && stall_ctrl->hasData()) {
      auto stall_packet =
          std::dynamic_pointer_cast<IntDataPacket>(stall_ctrl->read());
      if (stall_packet) {
        stall_ = (stall_packet->getValue() != 0);
      }
    }

    if (stall_) {
      total_stalls_++;
      if (verbose_) {
        std::cout << "[" << scheduler_.getCurrentTime() << "] " << getName()
                  << ": Pipeline stalled" << std::endl;
      }
      return;
    }

    // Process pipeline stages from back to front
    // Stage N-1 (last stage) -> output
    if (stages_[num_stages_ - 1].valid) {
      auto processed_data = stages_[num_stages_ - 1].data;
      if (processed_data) {
        processed_data->setTimestamp(scheduler_.getCurrentTime());
        out->write(processed_data);
        total_processed_++;

        if (verbose_) {
          auto int_data =
              std::dynamic_pointer_cast<IntDataPacket>(processed_data);
          if (int_data) {
            std::cout << "[" << scheduler_.getCurrentTime() << "] " << getName()
                      << ": Stage " << (num_stages_ - 1)
                      << " -> Output: " << int_data->getValue() << std::endl;
          }
        }
      }
      stages_[num_stages_ - 1].valid = false;
    }

    // Middle stages: propagate data backwards
    for (int i = num_stages_ - 1; i > 0; --i) {
      if (stages_[i - 1].valid) {
        // Apply stage function
        auto processed_data = stage_functions_[i](stages_[i - 1].data);
        stages_[i] =
            PipelineStageData(processed_data, scheduler_.getCurrentTime());

        if (verbose_) {
          auto int_data =
              std::dynamic_pointer_cast<IntDataPacket>(processed_data);
          if (int_data) {
            std::cout << "[" << scheduler_.getCurrentTime() << "] " << getName()
                      << ": Stage " << (i - 1) << " -> Stage " << i << ": "
                      << int_data->getValue() << std::endl;
          }
        }

        stages_[i - 1].valid = false;
      }
    }

    // Stage 0: read from input
    if (in->hasData()) {
      auto input_data = in->read();
      if (input_data && input_data->isValid()) {
        // Apply stage 0 function
        auto processed_data = stage_functions_[0](input_data);
        stages_[0] =
            PipelineStageData(processed_data, scheduler_.getCurrentTime());

        if (verbose_) {
          auto int_data =
              std::dynamic_pointer_cast<IntDataPacket>(processed_data);
          if (int_data) {
            std::cout << "[" << scheduler_.getCurrentTime() << "] " << getName()
                      << ": Input -> Stage 0: " << int_data->getValue()
                      << std::endl;
          }
        }
      }
    }
  }

  /**
   * @brief Flush the pipeline (clear all stages)
   */
  void flush() {
    for (auto& stage : stages_) {
      stage.valid = false;
      stage.data = nullptr;
    }
    flush_ = false;
    if (verbose_) {
      std::cout << "[" << scheduler_.getCurrentTime() << "] " << getName()
                << ": Pipeline flushed" << std::endl;
    }
  }

  /**
   * @brief Check if pipeline is empty
   */
  bool isEmpty() const {
    for (const auto& stage : stages_) {
      if (stage.valid) return false;
    }
    return true;
  }

  /**
   * @brief Check if pipeline is full
   */
  bool isFull() const {
    for (const auto& stage : stages_) {
      if (!stage.valid) return false;
    }
    return true;
  }

  /**
   * @brief Get pipeline occupancy (number of valid stages)
   */
  size_t getOccupancy() const {
    size_t count = 0;
    for (const auto& stage : stages_) {
      if (stage.valid) count++;
    }
    return count;
  }

  // Getters for statistics
  size_t getNumStages() const { return num_stages_; }
  uint64_t getTotalProcessed() const { return total_processed_; }
  uint64_t getTotalStalls() const { return total_stalls_; }
  bool isStalled() const { return stall_; }

  // Enable/disable verbose output
  void setVerbose(bool verbose) { verbose_ = verbose; }

  /**
   * @brief Print pipeline statistics
   */
  void printStatistics() const {
    std::cout << "\n=== Pipeline Statistics: " << getName()
              << " ===" << std::endl;
    std::cout << "Number of stages: " << num_stages_ << std::endl;
    std::cout << "Total processed: " << total_processed_ << std::endl;
    std::cout << "Total stalls: " << total_stalls_ << std::endl;
    std::cout << "Current occupancy: " << getOccupancy() << "/" << num_stages_
              << std::endl;
    std::cout << "Pipeline full: " << (isFull() ? "Yes" : "No") << std::endl;
    std::cout << "Pipeline empty: " << (isEmpty() ? "Yes" : "No") << std::endl;
  }

 protected:
  size_t num_stages_;                      // Number of pipeline stages
  std::vector<PipelineStageData> stages_;  // Pipeline stage data
  std::vector<StageFunction>
      stage_functions_;       // Processing function for each stage
  bool stall_;                // Pipeline stall flag
  bool flush_;                // Pipeline flush flag
  uint64_t total_processed_;  // Total items processed
  uint64_t total_stalls_;     // Total stall cycles
  bool verbose_ = false;      // Verbose output flag
};

/**
 * @brief Simple arithmetic pipeline example
 *
 * 3-stage pipeline that performs: multiply -> add -> shift
 */
class ArithmeticPipeline : public PipelineComponent {
 public:
  ArithmeticPipeline(const std::string& name,
                     EventDriven::EventScheduler& scheduler, uint64_t period,
                     int multiply_factor = 2, int add_value = 10,
                     int shift_amount = 1)
      : PipelineComponent(name, scheduler, period, 3) {
    // Stage 0: Multiply
    setStageFunction(
        0, [multiply_factor](std::shared_ptr<Architecture::DataPacket> data) {
          auto int_data = std::dynamic_pointer_cast<IntDataPacket>(data);
          if (int_data) {
            int result = int_data->getValue() * multiply_factor;
            return std::static_pointer_cast<Architecture::DataPacket>(
                std::make_shared<IntDataPacket>(result));
          }
          return data;
        });

    // Stage 1: Add
    setStageFunction(
        1, [add_value](std::shared_ptr<Architecture::DataPacket> data) {
          auto int_data = std::dynamic_pointer_cast<IntDataPacket>(data);
          if (int_data) {
            int result = int_data->getValue() + add_value;
            return std::static_pointer_cast<Architecture::DataPacket>(
                std::make_shared<IntDataPacket>(result));
          }
          return data;
        });

    // Stage 2: Right shift (divide by 2^shift_amount)
    setStageFunction(
        2, [shift_amount](std::shared_ptr<Architecture::DataPacket> data) {
          auto int_data = std::dynamic_pointer_cast<IntDataPacket>(data);
          if (int_data) {
            int result = int_data->getValue() >> shift_amount;
            return std::static_pointer_cast<Architecture::DataPacket>(
                std::make_shared<IntDataPacket>(result));
          }
          return data;
        });
  }
};

#endif  // PIPELINE_H
