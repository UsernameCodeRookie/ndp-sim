#ifndef STAGE_H
#define STAGE_H

#include <memory>
#include <string>

#include "component.h"
#include "packet.h"

namespace Architecture {

/**
 * @brief Base class for pipeline stages
 *
 * A Stage is a combinational or sequential component that can be inserted
 * into a Pipeline. Unlike TickingComponent, Stage doesn't have its own
 * clock - it's invoked by the Pipeline as data flows through.
 *
 * Stages process data synchronously with the Pipeline, making it easier
 * to compose complex pipeline stages from reusable components.
 */
class Stage : public Component {
 public:
  /**
   * @brief Constructor
   *
   * @param name Component name
   * @param scheduler Event scheduler
   */
  Stage(const std::string& name, EventDriven::EventScheduler& scheduler)
      : Component(name, scheduler) {}

  virtual ~Stage() = default;

  /**
   * @brief Process data through this stage
   *
   * This is the main processing function. It's called by the Pipeline
   * when data flows through this stage.
   *
   * @param data Input data packet (can be nullptr)
   * @return Processed data packet (can be nullptr to stall)
   */
  virtual std::shared_ptr<DataPacket> process(
      std::shared_ptr<DataPacket> data) = 0;

  /**
   * @brief Check if this stage should stall
   *
   * Returns true if the stage should stall (not advance data).
   * Default implementation returns false (never stall).
   *
   * @param data Current data in the stage
   * @return True if stall condition is met
   */
  virtual bool shouldStall(std::shared_ptr<DataPacket> data) const {
    return false;
  }

  /**
   * @brief Get latency of this stage in cycles
   *
   * If > 1, data will be held in this stage for multiple cycles.
   * Default implementation returns 1 (single cycle).
   *
   * @return Latency in cycles
   */
  virtual uint64_t getLatency() const { return 1; }

  /**
   * @brief Reset stage state
   *
   * Called when pipeline is flushed or reset.
   * Default implementation does nothing.
   */
  virtual void reset() {}
};

}  // namespace Architecture

#endif  // STAGE_H
