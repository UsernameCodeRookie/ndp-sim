#include <iostream>
#include <memory>

#include "architecture.h"
#include "event.h"

// Test data packet
class TestDataPacket : public Architecture::DataPacket {
 public:
  TestDataPacket(int value) : value_(value) {}
  int getValue() const { return value_; }

  std::shared_ptr<Architecture::DataPacket> clone() const override {
    auto cloned = std::make_shared<TestDataPacket>(value_);
    cloned->setTimestamp(timestamp_);
    cloned->setValid(valid_);
    return std::static_pointer_cast<Architecture::DataPacket>(cloned);
  }

 private:
  int value_;
};

// Test component
class TestComponent : public Architecture::TickingComponent {
 public:
  TestComponent(const std::string& name, EventDriven::EventScheduler& scheduler,
                uint64_t period)
      : Architecture::TickingComponent(name, scheduler, period) {}

  void tick() override {}
};

int main() {
  std::cout << "=== Testing TickingConnection with Latency ===" << std::endl;

  EventDriven::EventScheduler scheduler;
  scheduler.setVerbose(true);

  TestComponent comp1("Comp1", scheduler, 10);
  TestComponent comp2("Comp2", scheduler, 10);

  auto src_port = std::make_shared<Architecture::Port>(
      "src", Architecture::PortDirection::OUTPUT, &comp1);
  auto dst_port = std::make_shared<Architecture::Port>(
      "dst", Architecture::PortDirection::INPUT, &comp2);

  comp1.addPort(src_port);
  comp2.addPort(dst_port);

  Architecture::TickingConnection conn("TestConn", scheduler, 10);
  conn.addSourcePort(src_port);
  conn.addDestinationPort(dst_port);
  conn.setLatency(5);  // 5 cycle latency

  // Write data to source port
  auto data = std::make_shared<TestDataPacket>(456);
  src_port->write(std::static_pointer_cast<Architecture::DataPacket>(data));
  std::cout << "\n[Setup] Data written to source port: 456" << std::endl;
  std::cout << "[Setup] Source port has data: " << src_port->hasData()
            << std::endl;

  conn.start(0);
  std::cout << "\n[Setup] Connection started at time 0" << std::endl;
  std::cout << "[Setup] Connection latency: " << conn.getLatency() << std::endl;

  std::cout << "\n--- Running to time 3 ---" << std::endl;
  scheduler.run(3);
  std::cout << "\n[Check @time 3] Destination port has data: "
            << dst_port->hasData() << std::endl;
  std::cout << "[Check @time 3] Source port has data: " << src_port->hasData()
            << std::endl;

  std::cout << "\n--- Running to time 10 ---" << std::endl;
  scheduler.run(10);
  std::cout << "\n[Check @time 10] Destination port has data: "
            << dst_port->hasData() << std::endl;

  if (dst_port->hasData()) {
    auto received =
        std::dynamic_pointer_cast<TestDataPacket>(dst_port->getData());
    std::cout << "[Check @time 10] Received value: " << received->getValue()
              << std::endl;
    std::cout << "\n✓ TEST PASSED" << std::endl;
  } else {
    std::cout << "\n✗ TEST FAILED: No data at destination port" << std::endl;
  }

  return 0;
}
