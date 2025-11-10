#include <iostream>

#include "../event_periodic.h"
#include "../events/compute.h"
#include "../events/memory.h"
#include "../scheduler.h"

// Example 1: Basic event scheduling
void example1_basic_events() {
  std::cout << "\n=== Example 1: Basic Event Scheduling ===" << std::endl;

  EventDriven::EventScheduler scheduler;
  scheduler.setVerbose(true);

  // Using Lambda events
  scheduler.scheduleAt(
      10,
      [](EventDriven::EventScheduler&) {
        std::cout << "  Hello from event at time 10!" << std::endl;
      },
      0, "HelloEvent");

  scheduler.scheduleAt(
      5,
      [](EventDriven::EventScheduler&) {
        std::cout << "  This event happens first (time 5)" << std::endl;
      },
      0, "FirstEvent");

  scheduler.scheduleAt(
      15,
      [](EventDriven::EventScheduler&) {
        std::cout << "  Goodbye from event at time 15!" << std::endl;
      },
      0, "GoodbyeEvent");

  scheduler.run();
}

// Example 2: Custom event classes
void example2_custom_events() {
  std::cout << "\n=== Example 2: Custom Event Classes ===" << std::endl;

  EventDriven::EventScheduler scheduler;
  scheduler.setVerbose(true);

  // Schedule compute event
  auto compute_event = std::make_shared<ComputeEvent>(0, 5, "InitialCompute");
  scheduler.schedule(compute_event);

  // Schedule memory access events
  auto read_event = std::make_shared<MemoryAccessEvent>(25, 0x1000, true);
  scheduler.schedule(read_event);

  auto write_event = std::make_shared<MemoryAccessEvent>(30, 0x2000, false);
  scheduler.schedule(write_event);

  scheduler.run();
}

// Example 3: Event priority
void example3_event_priority() {
  std::cout << "\n=== Example 3: Event Priority ===" << std::endl;

  EventDriven::EventScheduler scheduler;
  scheduler.setVerbose(true);

  // These events all happen at time 10, but with different priorities
  scheduler.scheduleAt(
      10,
      [](EventDriven::EventScheduler&) {
        std::cout << "  Low priority event (priority=1)" << std::endl;
      },
      1, "LowPriority");

  scheduler.scheduleAt(
      10,
      [](EventDriven::EventScheduler&) {
        std::cout << "  High priority event (priority=10)" << std::endl;
      },
      10, "HighPriority");

  scheduler.scheduleAt(
      10,
      [](EventDriven::EventScheduler&) {
        std::cout << "  Medium priority event (priority=5)" << std::endl;
      },
      5, "MediumPriority");

  scheduler.run();
}

// Example 4: Periodic events
void example4_periodic_events() {
  std::cout << "\n=== Example 4: Periodic Events ===" << std::endl;

  EventDriven::EventScheduler scheduler;
  scheduler.setVerbose(true);

  // Create a periodic event, starting at time 0, repeating every 10 time units,
  // total 5 executions
  auto periodic = std::make_shared<EventDriven::PeriodicEvent>(
      0,   // start time
      10,  // period
      [](EventDriven::EventScheduler&, uint64_t count) {
        std::cout << "  Periodic event execution #" << count << std::endl;
      },
      5,  // repeat 5 times
      "PeriodicTask");

  scheduler.schedule(periodic);
  scheduler.run();
}

// Example 5: Chained events
void example5_chained_events() {
  std::cout << "\n=== Example 5: Chained Events ===" << std::endl;

  EventDriven::EventScheduler scheduler;
  scheduler.setVerbose(true);

  // First event schedules second, second schedules third...
  scheduler.scheduleAt(
      0,
      [](EventDriven::EventScheduler& sched) {
        std::cout << "  Stage 1: Initialize" << std::endl;
        sched.scheduleAt(
            sched.getCurrentTime() + 5,
            [](EventDriven::EventScheduler& sched) {
              std::cout << "  Stage 2: Process" << std::endl;
              sched.scheduleAt(
                  sched.getCurrentTime() + 5,
                  [](EventDriven::EventScheduler&) {
                    std::cout << "  Stage 3: Finalize" << std::endl;
                  },
                  0, "Stage3");
            },
            0, "Stage2");
      },
      0, "Stage1");

  scheduler.run();
}

// Example 6: Event cancellation
void example6_event_cancellation() {
  std::cout << "\n=== Example 6: Event Cancellation ===" << std::endl;

  EventDriven::EventScheduler scheduler;
  scheduler.setVerbose(true);

  // Create an event but cancel it later
  auto event1 = std::make_shared<EventDriven::LambdaEvent>(
      10,
      [](EventDriven::EventScheduler&) {
        std::cout << "  This should not print!" << std::endl;
      },
      0, "CancelledEvent");

  auto event2 = std::make_shared<EventDriven::LambdaEvent>(
      15,
      [](EventDriven::EventScheduler&) {
        std::cout << "  This event will execute" << std::endl;
      },
      0, "NormalEvent");

  scheduler.schedule(event1);
  scheduler.schedule(event2);

  // Cancel the first event
  event1->cancel();
  std::cout << "Event '" << event1->getName() << "' has been cancelled."
            << std::endl;

  scheduler.run();
}

// Example 7: Time-limited run
void example7_time_limited_run() {
  std::cout << "\n=== Example 7: Time-Limited Simulation ===" << std::endl;

  EventDriven::EventScheduler scheduler;
  scheduler.setVerbose(true);

  // Schedule multiple events, but only run until time 50
  for (int i = 0; i < 10; i++) {
    uint64_t time = i * 10;
    scheduler.scheduleAt(
        time,
        [i](EventDriven::EventScheduler&) {
          std::cout << "  Event #" << i << std::endl;
        },
        0, "Event" + std::to_string(i));
  }

  std::cout << "\nRunning simulation until time 50..." << std::endl;
  scheduler.runUntil(50);

  std::cout << "\nRemaining events: " << scheduler.getPendingEventCount()
            << std::endl;
}

int main() {
  std::cout << "Event-Driven Framework Examples\n" << std::endl;
  std::cout << "================================" << std::endl;

  // Run all examples
  example1_basic_events();
  example2_custom_events();
  example3_event_priority();
  example4_periodic_events();
  example5_chained_events();
  example6_event_cancellation();
  example7_time_limited_run();

  std::cout << "\n================================" << std::endl;
  std::cout << "All examples completed!" << std::endl;

  return 0;
}
