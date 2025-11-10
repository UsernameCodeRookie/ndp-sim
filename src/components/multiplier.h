#ifndef MULTIPLIER_H
#define MULTIPLIER_H

#include <iostream>
#include <memory>
#include <string>

#include "../port.h"
#include "../tick_component.h"
#include "int_packet.h"

/**
 * @brief Example: Simple multiplier component
 */
class MultiplierComponent : public Architecture::TickingComponent {
 public:
  MultiplierComponent(const std::string& name,
                      EventDriven::EventScheduler& scheduler, uint64_t period)
      : Architecture::TickingComponent(name, scheduler, period) {
    auto in = std::make_shared<Architecture::Port>(
        "in", Architecture::PortDirection::INPUT, this);
    auto out = std::make_shared<Architecture::Port>(
        "out", Architecture::PortDirection::OUTPUT, this);

    addPort(in);
    addPort(out);
  }

  void tick() override {
    auto in = getPort("in");
    auto out = getPort("out");

    if (in->hasData()) {
      auto data = std::dynamic_pointer_cast<IntDataPacket>(in->read());

      if (data) {
        int result = data->getValue() * 2;

        std::cout << "[" << scheduler_.getCurrentTime() << "] " << getName()
                  << ": " << data->getValue() << " * 2 = " << result
                  << std::endl;

        auto result_packet = std::make_shared<IntDataPacket>(result);
        result_packet->setTimestamp(scheduler_.getCurrentTime());
        out->write(
            std::static_pointer_cast<Architecture::DataPacket>(result_packet));
      }
    }
  }
};

#endif  // MULTIPLIER_H
