#ifndef PIPELINE_H
#define PIPELINE_H

#include <deque>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "packet.h"
#include "port.h"
#include "stage.h"
#include "tick.h"
#include "trace.h"

/**
 * @brief PipelineStageData
 *
 * Represents data in a pipeline stage
 */
struct PipelineStageData {
  std::shared_ptr<Architecture::DataPacket> data;
  uint64_t stage_entry_time;  // Wall clock time when data entered this stage
  uint32_t cycles_in_stage;   // Number of ticks data has been in this stage
  bool valid;                 // Whether this stage contains valid data

  PipelineStageData()
      : data(nullptr), stage_entry_time(0), cycles_in_stage(0), valid(false) {}

  PipelineStageData(std::shared_ptr<Architecture::DataPacket> d, uint64_t time)
      : data(d), stage_entry_time(time), cycles_in_stage(0), valid(true) {}
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
   * @param default_stage_latency Default latency for all stages (default: 0
   * cycles). Set to 0 when using direct tick() calls in tests, or to 1+ when
   * using event-driven scheduler.
   */
  Pipeline(const std::string& name, EventDriven::EventScheduler& scheduler,
           uint64_t period, size_t num_stages = 3,
           uint64_t default_stage_latency = 0)
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

    // Initialize stage latencies (default: 0 cycles per stage)
    // This prevents timing issues when using direct tick() calls in unit tests.
    // For event-driven operation, set this to 1+ when needed.
    stage_latencies_.resize(num_stages_, default_stage_latency);

    // Create default "in" and "out" ports for backward compatibility
    // Subclasses can call addPort() to create custom ports
    addPort("in", Architecture::PortDirection::INPUT);
    addPort("out", Architecture::PortDirection::OUTPUT);

    // Create stall control port only (other ports are created by subclasses)
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
   * @brief Register a Stage object at a specific pipeline stage
   *
   * Replaces the stage function at stage_index with one that calls
   * the Stage::process() method.
   *
   * @param stage_index Pipeline stage index (0-based)
   * @param stage Pointer to the Stage object
   * @return True if successful
   */
  bool setStage(size_t stage_index,
                std::shared_ptr<Architecture::Stage> stage) {
    if (stage_index >= num_stages_ || !stage) {
      return false;
    }

    // Store the stage object
    if (stage_index >= stage_objects_.size()) {
      stage_objects_.resize(stage_index + 1, nullptr);
    }
    stage_objects_[stage_index] = stage;

    // Create a stage function that calls the Stage's process method
    setStageFunction(stage_index,
                     [stage](std::shared_ptr<Architecture::DataPacket> data) {
                       return stage->process(data);
                     });

    // Set the stall predicate to use the Stage's shouldStall method
    setStageStallPredicate(
        stage_index, [stage](std::shared_ptr<Architecture::DataPacket> data) {
          return stage->shouldStall(data);
        });

    // Set the latency from the Stage
    setStageLatency(stage_index, stage->getLatency());

    return true;
  }

  /**
   * @brief Main tick function
   */
  void tick() override {
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

    // Increment cycle counters for all valid stages
    for (auto& stage : stages_) {
      if (stage.valid) {
        stage.cycles_in_stage++;
      }
    }

    // Process pipeline stages from back to front
    // Stage N-1 (last stage) -> output (to all OUTPUT ports)
    if (stages_[num_stages_ - 1].valid) {
      auto processed_data = stages_[num_stages_ - 1].data;
      if (processed_data) {
        processed_data->timestamp = scheduler_.getCurrentTime();

        // Write to all OUTPUT ports
        for (const auto& [port_name, port] : ports_) {
          if (port &&
              port->getDirection() == Architecture::PortDirection::OUTPUT) {
            port->write(processed_data);

            // Enhanced pipeline output trace
            auto int_data =
                std::dynamic_pointer_cast<Architecture::IntDataPacket>(
                    processed_data);
            if (int_data) {
              TRACE_COMPUTE(
                  scheduler_.getCurrentTime(), getName(), "PIPELINE_OUT",
                  "Stage[" << (num_stages_ - 1) << "]->" << port_name
                           << " value=" << int_data->value << " cycle="
                           << scheduler_.getCurrentTime() << " latency="
                           << (scheduler_.getCurrentTime() -
                               stages_[num_stages_ - 1].stage_entry_time)
                           << " total_processed=" << total_processed_);
            }
          }
        }
        total_processed_++;
      }
      stages_[num_stages_ - 1].valid = false;
    }

    // Middle stages: propagate data from stage i-1 to stage i (backwards)
    for (int i = num_stages_ - 1; i > 0; --i) {
      if (stages_[i - 1].valid && !stages_[i].valid) {
        // Stage i-1 has data and stage i is empty
        // Check if stage latency requirement has been met using tick cycles
        uint32_t required_latency = stage_latencies_[i - 1];
        uint32_t cycles_held = stages_[i - 1].cycles_in_stage;

        if (cycles_held < required_latency) {
          // Not enough cycles have passed, keep data in stage i-1
          TRACE_COMPUTE(
              scheduler_.getCurrentTime(), getName(), "PIPELINE_LATENCY_HOLD",
              "Stage[" << (i - 1) << "] latency hold: cycles_held="
                       << cycles_held << " required=" << required_latency);
          total_stalls_++;
          continue;
        }

        // Check if this stage is stalled by user-defined predicate
        if (stage_stall_predicates_[i](stages_[i - 1].data)) {
          // Stage is stalled, don't advance
          TRACE_COMPUTE(scheduler_.getCurrentTime(), getName(),
                        "PIPELINE_STALL",
                        "Stage[" << (i - 1) << "] stalled by predicate");
          total_stalls_++;
          continue;
        }

        // Latency requirement met, target stage empty, and no user stalls
        // Apply stage function to process the data
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

    // Stage 0: read from all INPUT ports OR generate data internally
    // Try to read from all INPUT ports; if any has data, use it
    // Otherwise call stage 0 function with null data to allow components
    // to generate their own data from internal buffers
    std::shared_ptr<Architecture::DataPacket> input_data = nullptr;
    std::string input_port_name;

    for (const auto& [port_name, port] : ports_) {
      if (port && port->getDirection() == Architecture::PortDirection::INPUT &&
          port_name != "stall") {  // Skip stall control port
        if (port->hasData()) {
          input_data = port->read();
          input_port_name = port_name;
          break;  // Take first available input
        }
      }
    }

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
          std::string trace_src =
              input_port_name.empty() ? "INTERNAL" : input_port_name;
          TRACE_COMPUTE(scheduler_.getCurrentTime(), getName(), "PIPELINE_IN",
                        trace_src << "->Stage[0] value=" << int_data->value
                                  << " cycle=" << scheduler_.getCurrentTime()
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
  std::vector<std::shared_ptr<Architecture::Stage>>
      stage_objects_;         // Stage objects registered at each stage
  bool stall_;                // Pipeline stall flag
  bool flush_;                // Pipeline flush flag
  uint64_t total_processed_;  // Total items processed
  uint64_t total_stalls_;     // Total stall cycles
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
