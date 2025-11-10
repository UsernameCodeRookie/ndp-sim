#include <iostream>
#include <memory>

#include "../components/adder.h"
#include "../components/multiplier.h"
#include "../components/sink.h"
#include "../components/source.h"
#include "../scheduler.h"
#include "../tick_connection.h"

/**
 * @brief Example 1: Simple pipeline with adder
 *
 * Source1 ─┐
 *          ├─> Adder ─> Connection ─> Sink
 * Source2 ─┘
 */
void example1_simple_pipeline() {
  std::cout << "\n=== Example 1: Simple Pipeline with Adder ===" << std::endl;

  EventDriven::EventScheduler scheduler;

  // Create components (all with period 10)
  auto source1 =
      std::make_shared<DataSourceComponent>("Source1", scheduler, 10, 1);
  auto source2 =
      std::make_shared<DataSourceComponent>("Source2", scheduler, 10, 10);
  auto adder = std::make_shared<AdderComponent>("Adder", scheduler, 10);
  auto sink = std::make_shared<DataSinkComponent>("Sink", scheduler, 10);

  // Create connections (with period 10)
  auto conn1 = std::make_shared<Architecture::TickingConnection>(
      "Conn_S1_to_Adder", scheduler, 10);
  auto conn2 = std::make_shared<Architecture::TickingConnection>(
      "Conn_S2_to_Adder", scheduler, 10);
  auto conn3 = std::make_shared<Architecture::TickingConnection>(
      "Conn_Adder_to_Sink", scheduler, 10);

  // Connect ports
  conn1->addSourcePort(source1->getPort("out"));
  conn1->addDestinationPort(adder->getPort("in_a"));

  conn2->addSourcePort(source2->getPort("out"));
  conn2->addDestinationPort(adder->getPort("in_b"));

  conn3->addSourcePort(adder->getPort("out"));
  conn3->addDestinationPort(sink->getPort("in"));

  // Set connection latency
  conn1->setLatency(1);  // 1 cycle delay
  conn2->setLatency(1);
  conn3->setLatency(2);  // 2 cycle delay

  // Start components and connections
  // Offset connections slightly to ensure proper data flow
  source1->start(0);
  source2->start(0);
  adder->start(0);
  sink->start(0);

  conn1->start(5);  // Start propagating after components start
  conn2->start(5);
  conn3->start(5);

  // Run simulation
  scheduler.run(100);

  std::cout << "\nSimulation completed at time " << scheduler.getCurrentTime()
            << std::endl;
}

/**
 * @brief Example 2: Multi-stage pipeline
 *
 * Source ─> Adder ─> Multiplier ─> Sink
 *   ↓        ↑
 *   └────────┘
 */
void example2_feedback_pipeline() {
  std::cout << "\n=== Example 2: Multi-Stage Pipeline ===" << std::endl;

  EventDriven::EventScheduler scheduler;

  // Create components
  auto source =
      std::make_shared<DataSourceComponent>("Source", scheduler, 10, 5);
  auto adder = std::make_shared<AdderComponent>("Adder", scheduler, 10);
  auto multiplier =
      std::make_shared<MultiplierComponent>("Multiplier", scheduler, 10);
  auto sink = std::make_shared<DataSinkComponent>("Sink", scheduler, 10);

  // Create a constant source for the second input of adder
  auto const_source =
      std::make_shared<DataSourceComponent>("ConstSource", scheduler, 10, 100);

  // Create connections
  auto conn1 = std::make_shared<Architecture::TickingConnection>(
      "Conn_Source_to_Adder", scheduler, 10);
  auto conn2 = std::make_shared<Architecture::TickingConnection>(
      "Conn_Const_to_Adder", scheduler, 10);
  auto conn3 = std::make_shared<Architecture::TickingConnection>(
      "Conn_Adder_to_Mult", scheduler, 10);
  auto conn4 = std::make_shared<Architecture::TickingConnection>(
      "Conn_Mult_to_Sink", scheduler, 10);

  // Connect ports
  conn1->addSourcePort(source->getPort("out"));
  conn1->addDestinationPort(adder->getPort("in_a"));

  conn2->addSourcePort(const_source->getPort("out"));
  conn2->addDestinationPort(adder->getPort("in_b"));

  conn3->addSourcePort(adder->getPort("out"));
  conn3->addDestinationPort(multiplier->getPort("in"));

  conn4->addSourcePort(multiplier->getPort("out"));
  conn4->addDestinationPort(sink->getPort("in"));

  // Set latencies
  conn1->setLatency(1);
  conn2->setLatency(1);
  conn3->setLatency(1);
  conn4->setLatency(1);

  // Start all components and connections
  source->start(0);
  const_source->start(0);
  adder->start(0);
  multiplier->start(0);
  sink->start(0);

  conn1->start(5);
  conn2->start(5);
  conn3->start(5);
  conn4->start(5);

  // Run simulation
  scheduler.run(100);

  std::cout << "\nSimulation completed at time " << scheduler.getCurrentTime()
            << std::endl;
}

/**
 * @brief Example 3: Demonstrating different tick periods
 */
void example3_different_periods() {
  std::cout << "\n=== Example 3: Different Tick Periods ===" << std::endl;

  EventDriven::EventScheduler scheduler;

  // Fast source: period 5
  auto fast_source =
      std::make_shared<DataSourceComponent>("FastSource", scheduler, 5, 1);

  // Slow processor: period 15
  auto slow_processor =
      std::make_shared<MultiplierComponent>("SlowProcessor", scheduler, 15);

  // Fast sink: period 5
  auto fast_sink =
      std::make_shared<DataSinkComponent>("FastSink", scheduler, 5);

  // Connection with period 5 (matches the faster component)
  auto conn1 =
      std::make_shared<Architecture::TickingConnection>("Conn1", scheduler, 5);
  auto conn2 =
      std::make_shared<Architecture::TickingConnection>("Conn2", scheduler, 5);

  // Connect
  conn1->addSourcePort(fast_source->getPort("out"));
  conn1->addDestinationPort(slow_processor->getPort("in"));

  conn2->addSourcePort(slow_processor->getPort("out"));
  conn2->addDestinationPort(fast_sink->getPort("in"));

  conn1->setLatency(0);
  conn2->setLatency(0);

  // Start
  fast_source->start(0);
  slow_processor->start(0);
  fast_sink->start(0);
  conn1->start(3);
  conn2->start(3);

  // Run
  scheduler.run(80);

  std::cout << "\nSimulation completed at time " << scheduler.getCurrentTime()
            << std::endl;
}

int main() {
  // Run examples
  example1_simple_pipeline();
  example2_feedback_pipeline();
  example3_different_periods();

  return 0;
}
