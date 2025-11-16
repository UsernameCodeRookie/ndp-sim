#ifndef PIPELINE_H
#define PIPELINE_H

#include <deque>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "../packet.h"
#include "../port.h"
#include "../tick.h"
#include "../trace.h"

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
class Pipeline : public Architecture::TickingComponent {
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
  Pipeline(const std::string& name, EventDriven::EventScheduler& scheduler,
           uint64_t period, size_t num_stages = 3)
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

    // Initialize default stage stall predicates (never stall)
    stage_stall_predicates_.resize(num_stages_);
    for (size_t i = 0; i < num_stages_; ++i) {
      stage_stall_predicates_[i] =
          [](std::shared_ptr<Architecture::DataPacket>) { return false; };
    }

    // Initialize stage latencies (default: 1 cycle per stage)
    stage_latencies_.resize(num_stages_, 1);

    // Create ports
    addPort("in", Architecture::PortDirection::INPUT);
    addPort("out", Architecture::PortDirection::OUTPUT);
    addPort("stall", Architecture::PortDirection::INPUT);
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
   * @brief Set stall predicate for a specific stage
   * When this function returns true, data stays in the stage (doesn't advance)
   * @param stage_index Stage index (0-based)
   * @param predicate Stall condition function that takes stage data and returns
   * true if stalled
   */
  void setStageStallPredicate(
      size_t stage_index,
      std::function<bool(std::shared_ptr<Architecture::DataPacket>)>
          predicate) {
    if (stage_index < num_stages_) {
      stage_stall_predicates_[stage_index] = predicate;
    }
  }

  /**
   * @brief Set execution latency for a specific stage
   * Data will remain in the stage for the specified number of cycles
   * before advancing to the next stage
   * @param stage_index Stage index (0-based)
   * @param latency Number of cycles to hold data in this stage
   */
  void setStageLatency(size_t stage_index, uint64_t latency) {
    if (stage_index < num_stages_) {
      stage_latencies_[stage_index] = latency;
    }
  }

  /**
   * @brief Get execution latency for a specific stage
   * @param stage_index Stage index (0-based)
   * @return Latency in cycles (default: 1)
   */
  uint64_t getStageLatency(size_t stage_index) const {
    if (stage_index < num_stages_) {
      return stage_latencies_[stage_index];
    }
    return 1;  // Default: 1 cycle
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
          std::dynamic_pointer_cast<Architecture::IntDataPacket>(
              stall_ctrl->read());
      if (stall_packet) {
        stall_ = (stall_packet->value != 0);
      }
    }

    if (stall_) {
      total_stalls_++;
      return;
    }

    // Process pipeline stages from back to front
    // Stage N-1 (last stage) -> output
    if (stages_[num_stages_ - 1].valid) {
      auto processed_data = stages_[num_stages_ - 1].data;
      if (processed_data) {
        processed_data->timestamp = scheduler_.getCurrentTime();
        out->write(processed_data);
        total_processed_++;

        // Enhanced pipeline output trace
        auto int_data = std::dynamic_pointer_cast<Architecture::IntDataPacket>(
            processed_data);
        if (int_data) {
          TRACE_COMPUTE(scheduler_.getCurrentTime(), getName(), "PIPELINE_OUT",
                        "Stage[" << (num_stages_ - 1)
                                 << "]->OUT value=" << int_data->value
                                 << " cycle=" << scheduler_.getCurrentTime()
                                 << " latency="
                                 << (scheduler_.getCurrentTime() -
                                     stages_[num_stages_ - 1].stage_entry_time)
                                 << " total_processed=" << total_processed_);
        }
      }
      stages_[num_stages_ - 1].valid = false;
    }

    // Middle stages: propagate data backwards
    for (int i = num_stages_ - 1; i > 0; --i) {
      if (stages_[i - 1].valid) {
        // Check if stage latency requirement has been met
        uint64_t elapsed =
            scheduler_.getCurrentTime() - stages_[i - 1].stage_entry_time;
        uint64_t required_latency = stage_latencies_[i - 1];

        if (elapsed < required_latency) {
          // Not enough time has passed, stage must remain in this stage
          // But still apply the stage function to update data in place
          auto processed_data = stage_functions_[i](stages_[i - 1].data);
          stages_[i - 1].data = processed_data;
          TRACE_COMPUTE(
              scheduler_.getCurrentTime(), getName(), "PIPELINE_LATENCY_HOLD",
              "Stage[" << (i - 1) << "] latency hold: elapsed=" << elapsed
                       << " required=" << required_latency);
          total_stalls_++;
          continue;
        }

        // Check if target stage is already occupied
        if (stages_[i].valid) {
          // Target stage is full, stall this stage
          // But still apply the stage function to the data in place
          auto processed_data = stage_functions_[i](stages_[i - 1].data);
          stages_[i - 1].data =
              processed_data;  // Keep updated data in current stage
          TRACE_COMPUTE(
              scheduler_.getCurrentTime(), getName(), "PIPELINE_STALL",
              "Stage[" << (i - 1) << "] stalled, stage[" << i << "] full");
          total_stalls_++;
          continue;  // Don't advance, wait for target to empty
        }

        // Check if this stage is stalled by user-defined predicate
        if (stage_stall_predicates_[i](stages_[i - 1].data)) {
          // Stage is stalled, apply stage function but don't advance
          auto processed_data = stage_functions_[i](stages_[i - 1].data);
          stages_[i - 1].data = processed_data;  // Update data in place
          continue;                              // Skip advancing to next stage
        }

        // Latency requirement met, target stage empty, and no user stalls
        // Apply stage function
        auto processed_data = stage_functions_[i](stages_[i - 1].data);
        stages_[i] =
            PipelineStageData(processed_data, scheduler_.getCurrentTime());

        // Enhanced stage transition trace
        auto int_data = std::dynamic_pointer_cast<Architecture::IntDataPacket>(
            processed_data);
        if (int_data) {
          TRACE_COMPUTE(scheduler_.getCurrentTime(), getName(), "STAGE_PROP",
                        "Stage[" << (i - 1) << "]->Stage[" << i
                                 << "] value=" << int_data->value
                                 << " cycle=" << scheduler_.getCurrentTime()
                                 << " period=" << getPeriod());
        }

        stages_[i - 1].valid = false;
      }
    }

    // Stage 0: read from input OR generate data internally
    // If input has data, use it; otherwise call stage 0 function with null data
    // to allow components like SCore to generate their own data from buffers
    auto input_data = in->hasData() ? in->read() : nullptr;

    // Always try to process stage 0 (even without external input)
    // This allows stage functions to generate data from internal buffers
    if (!stages_[0].valid) {  // Only if stage 0 is not already occupied
      auto processed_data = stage_functions_[0](input_data);
      if (processed_data) {
        stages_[0] =
            PipelineStageData(processed_data, scheduler_.getCurrentTime());

        // Enhanced input trace
        auto int_data = std::dynamic_pointer_cast<Architecture::IntDataPacket>(
            processed_data);
        if (int_data) {
          TRACE_COMPUTE(scheduler_.getCurrentTime(), getName(), "PIPELINE_IN",
                        "IN->Stage[0] value=" << int_data->value << " cycle="
                                              << scheduler_.getCurrentTime()
                                              << " period=" << getPeriod()
                                              << " depth=" << num_stages_);
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
      stage_functions_;  // Processing function for each stage
  std::vector<std::function<bool(std::shared_ptr<Architecture::DataPacket>)>>
      stage_stall_predicates_;             // Stall predicates for each stage
  std::vector<uint64_t> stage_latencies_;  // Latency (in cycles) for each stage
  bool stall_;                             // Pipeline stall flag
  bool flush_;                             // Pipeline flush flag
  uint64_t total_processed_;               // Total items processed
  uint64_t total_stalls_;                  // Total stall cycles
};

/**
 * @brief Simple arithmetic pipeline example
 *
 * 3-stage pipeline that performs: multiply -> add -> shift
 */
class ArithmeticPipeline : public Pipeline {
 public:
  ArithmeticPipeline(const std::string& name,
                     EventDriven::EventScheduler& scheduler, uint64_t period,
                     int multiply_factor = 2, int add_value = 10,
                     int shift_amount = 1)
      : Pipeline(name, scheduler, period, 3) {
    // Stage 0: Multiply
    setStageFunction(
        0, [multiply_factor](std::shared_ptr<Architecture::DataPacket> data) {
          auto int_data =
              std::dynamic_pointer_cast<Architecture::IntDataPacket>(data);
          if (int_data) {
            int result = int_data->value * multiply_factor;
            return std::static_pointer_cast<Architecture::DataPacket>(
                std::make_shared<Architecture::IntDataPacket>(result));
          }
          return data;
        });

    // Stage 1: Add
    setStageFunction(
        1, [add_value](std::shared_ptr<Architecture::DataPacket> data) {
          auto int_data =
              std::dynamic_pointer_cast<Architecture::IntDataPacket>(data);
          if (int_data) {
            int result = int_data->value + add_value;
            return std::static_pointer_cast<Architecture::DataPacket>(
                std::make_shared<Architecture::IntDataPacket>(result));
          }
          return data;
        });

    // Stage 2: Right shift (divide by 2^shift_amount)
    setStageFunction(
        2, [shift_amount](std::shared_ptr<Architecture::DataPacket> data) {
          auto int_data =
              std::dynamic_pointer_cast<Architecture::IntDataPacket>(data);
          if (int_data) {
            int result = int_data->value >> shift_amount;
            return std::static_pointer_cast<Architecture::DataPacket>(
                std::make_shared<Architecture::IntDataPacket>(result));
          }
          return data;
        });
  }
};

#endif  // PIPELINE_H
