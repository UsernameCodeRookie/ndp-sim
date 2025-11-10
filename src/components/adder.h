#ifndef ADDER_H
#define ADDER_H

#include <iostream>
#include <memory>
#include <string>

#include "../port.h"
#include "../tick_component.h"
#include "int_packet.h"

/**
 * @brief Example: Simple adder component
 *
 * Reads two inputs, adds them, and produces output
 */
class AdderComponent : public Architecture::TickingComponent {
 public:
  AdderComponent(const std::string& name,
                 EventDriven::EventScheduler& scheduler, uint64_t period)
      : Architecture::TickingComponent(name, scheduler, period) {
    // Create input ports
    auto in_a = std::make_shared<Architecture::Port>(
        "in_a", Architecture::PortDirection::INPUT, this);
    auto in_b = std::make_shared<Architecture::Port>(
        "in_b", Architecture::PortDirection::INPUT, this);

    // Create output port
    auto out = std::make_shared<Architecture::Port>(
        "out", Architecture::PortDirection::OUTPUT, this);

    addPort(in_a);
    addPort(in_b);
    addPort(out);
  }

  void tick() override {
    auto in_a = getPort("in_a");
    auto in_b = getPort("in_b");
    auto out = getPort("out");

    // Check if both inputs have data
    if (in_a->hasData() && in_b->hasData()) {
      auto data_a = std::dynamic_pointer_cast<IntDataPacket>(in_a->read());
      auto data_b = std::dynamic_pointer_cast<IntDataPacket>(in_b->read());

      if (data_a && data_b) {
        int result = data_a->getValue() + data_b->getValue();

        std::cout << "[" << scheduler_.getCurrentTime() << "] " << getName()
                  << ": " << data_a->getValue() << " + " << data_b->getValue()
                  << " = " << result << std::endl;

        // Write result to output port
        auto result_packet = std::make_shared<IntDataPacket>(result);
        result_packet->setTimestamp(scheduler_.getCurrentTime());
        out->write(
            std::static_pointer_cast<Architecture::DataPacket>(result_packet));
      }
    }
  }
};

#endif  // ADDER_H
