#ifndef SINK_H
#define SINK_H

#include <iostream>
#include <memory>
#include <string>

#include "../port.h"
#include "../tick_component.h"
#include "int_packet.h"

/**
 * @brief Example: Data sink component (receives and displays data)
 */
class DataSinkComponent : public Architecture::TickingComponent {
 public:
  DataSinkComponent(const std::string& name,
                    EventDriven::EventScheduler& scheduler, uint64_t period)
      : Architecture::TickingComponent(name, scheduler, period) {
    auto in = std::make_shared<Architecture::Port>(
        "in", Architecture::PortDirection::INPUT, this);
    addPort(in);
  }

  void tick() override {
    auto in = getPort("in");

    if (in->hasData()) {
      auto data = std::dynamic_pointer_cast<IntDataPacket>(in->read());

      if (data) {
        std::cout << "[" << scheduler_.getCurrentTime() << "] " << getName()
                  << ": Received value " << data->getValue() << std::endl;
      }
    }
  }
};

#endif  // SINK_H
