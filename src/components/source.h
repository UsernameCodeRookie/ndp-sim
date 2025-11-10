#ifndef SOURCE_H
#define SOURCE_H

#include <iostream>
#include <memory>
#include <string>

#include "../port.h"
#include "../tick_component.h"
#include "int_packet.h"

/**
 * @brief Example: Data source component (generates test data)
 */
class DataSourceComponent : public Architecture::TickingComponent {
 public:
  DataSourceComponent(const std::string& name,
                      EventDriven::EventScheduler& scheduler, uint64_t period,
                      int initial_value)
      : Architecture::TickingComponent(name, scheduler, period),
        current_value_(initial_value) {
    auto out = std::make_shared<Architecture::Port>(
        "out", Architecture::PortDirection::OUTPUT, this);
    addPort(out);
  }

  void tick() override {
    auto out = getPort("out");

    std::cout << "[" << scheduler_.getCurrentTime() << "] " << getName()
              << ": Generating value " << current_value_ << std::endl;

    auto data_packet = std::make_shared<IntDataPacket>(current_value_);
    data_packet->setTimestamp(scheduler_.getCurrentTime());
    out->write(std::static_pointer_cast<Architecture::DataPacket>(data_packet));

    current_value_++;

    // Stop after generating 5 values
    if (tick_count_ >= 4) {
      stop();
    }
  }

 private:
  int current_value_;
};

#endif  // SOURCE_H
