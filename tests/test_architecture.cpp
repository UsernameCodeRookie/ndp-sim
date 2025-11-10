#include <gtest/gtest.h>

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
    return cloned;
  }

 private:
  int value_;
};

// Test component
class TestComponent : public Architecture::TickingComponent {
 public:
  TestComponent(const std::string& name, EventDriven::EventScheduler& scheduler,
                uint64_t period)
      : Architecture::TickingComponent(name, scheduler, period),
        tick_called_(0) {}

  void tick() override { tick_called_++; }

  int getTickCalled() const { return tick_called_; }

 private:
  int tick_called_;
};

// Test Port functionality
TEST(ArchitectureTest, PortBasics) {
  EventDriven::EventScheduler scheduler;
  TestComponent comp("TestComp", scheduler, 10);

  Architecture::Port port("test_port", Architecture::PortDirection::INPUT,
                          &comp);

  EXPECT_EQ(port.getName(), "test_port");
  EXPECT_EQ(port.getDirection(), Architecture::PortDirection::INPUT);
  EXPECT_EQ(port.getOwner(), &comp);
  EXPECT_FALSE(port.isConnected());
  EXPECT_FALSE(port.hasData());
}

// Test Port data operations
TEST(ArchitectureTest, PortDataOperations) {
  EventDriven::EventScheduler scheduler;
  TestComponent comp("TestComp", scheduler, 10);

  Architecture::Port port("test_port", Architecture::PortDirection::OUTPUT,
                          &comp);

  // Write data
  auto data = std::make_shared<TestDataPacket>(42);
  port.write(data);

  EXPECT_TRUE(port.hasData());
  EXPECT_EQ(
      std::dynamic_pointer_cast<TestDataPacket>(port.getData())->getValue(),
      42);

  // Read data (consumes it)
  auto read_data = port.read();
  EXPECT_FALSE(port.hasData());
  EXPECT_EQ(std::dynamic_pointer_cast<TestDataPacket>(read_data)->getValue(),
            42);
}

// Test Component basics
TEST(ArchitectureTest, ComponentBasics) {
  EventDriven::EventScheduler scheduler;
  TestComponent comp("TestComp", scheduler, 10);

  EXPECT_EQ(comp.getName(), "TestComp");
  EXPECT_TRUE(comp.isEnabled());

  auto port = std::make_shared<Architecture::Port>(
      "port1", Architecture::PortDirection::INPUT, &comp);
  comp.addPort(port);

  EXPECT_NE(comp.getPort("port1"), nullptr);
  EXPECT_EQ(comp.getPort("port1")->getName(), "port1");
  EXPECT_EQ(comp.getPort("nonexistent"), nullptr);
}

// Test Connection basics
TEST(ArchitectureTest, ConnectionBasics) {
  EventDriven::EventScheduler scheduler;
  Architecture::Connection conn("TestConn", scheduler);

  EXPECT_EQ(conn.getName(), "TestConn");
  EXPECT_EQ(conn.getLatency(), 0);

  conn.setLatency(5);
  EXPECT_EQ(conn.getLatency(), 5);
}

// Test Connection port management
TEST(ArchitectureTest, ConnectionPortManagement) {
  EventDriven::EventScheduler scheduler;
  TestComponent comp1("Comp1", scheduler, 10);
  TestComponent comp2("Comp2", scheduler, 10);

  auto src_port = std::make_shared<Architecture::Port>(
      "src", Architecture::PortDirection::OUTPUT, &comp1);
  auto dst_port = std::make_shared<Architecture::Port>(
      "dst", Architecture::PortDirection::INPUT, &comp2);

  Architecture::Connection conn("TestConn", scheduler);
  conn.addSourcePort(src_port);
  conn.addDestinationPort(dst_port);

  EXPECT_EQ(conn.getSourcePorts().size(), 1);
  EXPECT_EQ(conn.getDestinationPorts().size(), 1);
  EXPECT_TRUE(src_port->isConnected());
  EXPECT_TRUE(dst_port->isConnected());
}

// Test TickingComponent
TEST(ArchitectureTest, TickingComponentBasics) {
  EventDriven::EventScheduler scheduler;
  TestComponent comp("TestComp", scheduler, 10);

  auto port = std::make_shared<Architecture::Port>(
      "out", Architecture::PortDirection::OUTPUT, &comp);
  comp.addPort(port);

  comp.start(0);
  scheduler.run(50);

  // Should tick at times: 0, 10, 20, 30, 40, 50 = 6 times
  EXPECT_GE(comp.getTickCalled(), 5);
}

// Test TickingComponent stop
TEST(ArchitectureTest, TickingComponentStop) {
  EventDriven::EventScheduler scheduler;
  TestComponent comp("TestComp", scheduler, 10);

  comp.start(0);
  scheduler.run(25);

  int ticks_before_stop = comp.getTickCalled();
  comp.stop();

  scheduler.run(100);

  // Should not tick after stop
  EXPECT_EQ(comp.getTickCalled(), ticks_before_stop);
}

// Test TickingConnection propagation
TEST(ArchitectureTest, TickingConnectionPropagation) {
  EventDriven::EventScheduler scheduler;
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
  conn.setLatency(0);

  // Write data to source port
  auto data = std::make_shared<TestDataPacket>(123);
  src_port->write(data);

  conn.start(0);
  scheduler.run(5);

  // Data should be propagated to destination
  EXPECT_TRUE(dst_port->hasData());
  EXPECT_EQ(std::dynamic_pointer_cast<TestDataPacket>(dst_port->getData())
                ->getValue(),
            123);
}

// Test TickingConnection with latency
TEST(ArchitectureTest, TickingConnectionWithLatency) {
  EventDriven::EventScheduler scheduler;
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
  src_port->write(data);

  conn.start(0);
  scheduler.run(3);

  // Data should NOT be at destination yet (latency)
  EXPECT_FALSE(dst_port->hasData());

  scheduler.run(10);

  // Now data should be at destination
  EXPECT_TRUE(dst_port->hasData());
  EXPECT_EQ(std::dynamic_pointer_cast<TestDataPacket>(dst_port->getData())
                ->getValue(),
            456);
}

// Test DataPacket cloning
TEST(ArchitectureTest, DataPacketCloning) {
  auto original = std::make_shared<TestDataPacket>(999);
  original->setTimestamp(100);
  original->setValid(true);

  auto cloned = std::dynamic_pointer_cast<TestDataPacket>(original->clone());

  EXPECT_EQ(cloned->getValue(), 999);
  EXPECT_EQ(cloned->getTimestamp(), 100);
  EXPECT_TRUE(cloned->isValid());
  EXPECT_NE(original.get(), cloned.get());  // Different objects
}

// Test multiple source ports
TEST(ArchitectureTest, MultipleSourcePorts) {
  EventDriven::EventScheduler scheduler;
  TestComponent comp1("Comp1", scheduler, 10);
  TestComponent comp2("Comp2", scheduler, 10);
  TestComponent comp3("Comp3", scheduler, 10);

  auto src_port1 = std::make_shared<Architecture::Port>(
      "src1", Architecture::PortDirection::OUTPUT, &comp1);
  auto src_port2 = std::make_shared<Architecture::Port>(
      "src2", Architecture::PortDirection::OUTPUT, &comp2);
  auto dst_port = std::make_shared<Architecture::Port>(
      "dst", Architecture::PortDirection::INPUT, &comp3);

  Architecture::TickingConnection conn("TestConn", scheduler, 10);
  conn.addSourcePort(src_port1);
  conn.addSourcePort(src_port2);
  conn.addDestinationPort(dst_port);
  conn.setLatency(0);

  // Write data to first source port
  auto data1 = std::make_shared<TestDataPacket>(111);
  src_port1->write(data1);

  conn.start(0);
  scheduler.run(5);

  // Data should be propagated
  EXPECT_TRUE(dst_port->hasData());
}

// Test component enable/disable
TEST(ArchitectureTest, ComponentEnableDisable) {
  EventDriven::EventScheduler scheduler;
  TestComponent comp("TestComp", scheduler, 10);

  comp.start(0);
  scheduler.run(15);

  int ticks_enabled = comp.getTickCalled();

  comp.setEnabled(false);
  scheduler.run(50);

  // Should not tick when disabled
  EXPECT_EQ(comp.getTickCalled(), ticks_enabled);

  comp.setEnabled(true);
  comp.start(scheduler.getCurrentTime());
  scheduler.run(100);

  // Should tick again after re-enabling
  EXPECT_GT(comp.getTickCalled(), ticks_enabled);
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
