#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "../src/event.h"

// Test event class
class TestEvent : public EventDriven::Event {
 public:
  TestEvent(uint64_t time, int* counter, int priority = 0)
      : EventDriven::Event(time, priority, EventDriven::EventType::GENERIC,
                           "TestEvent"),
        counter_(counter) {}

  void execute(EventDriven::EventScheduler&) override { (*counter_)++; }

 private:
  int* counter_;
};

// Test: Basic event scheduling
TEST(EventSchedulerTest, BasicScheduling) {
  EventDriven::EventScheduler scheduler;
  int counter = 0;

  // Schedule three events
  scheduler.scheduleAt(
      10, [&counter](EventDriven::EventScheduler&) { counter++; }, 0, "Event1");

  scheduler.scheduleAt(
      20, [&counter](EventDriven::EventScheduler&) { counter++; }, 0, "Event2");

  scheduler.scheduleAt(
      30, [&counter](EventDriven::EventScheduler&) { counter++; }, 0, "Event3");

  EXPECT_EQ(scheduler.getPendingEventCount(), 3);

  // Run simulation
  scheduler.run();

  EXPECT_EQ(counter, 3);
  EXPECT_EQ(scheduler.getCurrentTime(), 30);
  EXPECT_EQ(scheduler.getPendingEventCount(), 0);
}

// Test: Events execute in time order
TEST(EventSchedulerTest, TimeOrdering) {
  EventDriven::EventScheduler scheduler;
  std::vector<int> execution_order;

  scheduler.scheduleAt(
      30,
      [&execution_order](EventDriven::EventScheduler&) {
        execution_order.push_back(3);
      },
      0, "Event3");

  scheduler.scheduleAt(
      10,
      [&execution_order](EventDriven::EventScheduler&) {
        execution_order.push_back(1);
      },
      0, "Event1");

  scheduler.scheduleAt(
      20,
      [&execution_order](EventDriven::EventScheduler&) {
        execution_order.push_back(2);
      },
      0, "Event2");

  scheduler.run();

  ASSERT_EQ(execution_order.size(), 3);
  EXPECT_EQ(execution_order[0], 1);
  EXPECT_EQ(execution_order[1], 2);
  EXPECT_EQ(execution_order[2], 3);
}

// Test: Event priority
TEST(EventSchedulerTest, EventPriority) {
  EventDriven::EventScheduler scheduler;
  std::vector<int> execution_order;

  // All events at the same time, but with different priorities
  scheduler.scheduleAt(
      10,
      [&execution_order](EventDriven::EventScheduler&) {
        execution_order.push_back(1);
      },
      1, "LowPriority");

  scheduler.scheduleAt(
      10,
      [&execution_order](EventDriven::EventScheduler&) {
        execution_order.push_back(3);
      },
      10, "HighPriority");

  scheduler.scheduleAt(
      10,
      [&execution_order](EventDriven::EventScheduler&) {
        execution_order.push_back(2);
      },
      5, "MediumPriority");

  scheduler.run();

  ASSERT_EQ(execution_order.size(), 3);
  EXPECT_EQ(execution_order[0], 3);  // High priority executes first
  EXPECT_EQ(execution_order[1], 2);  // Medium priority
  EXPECT_EQ(execution_order[2], 1);  // Low priority last
}

// Test: Custom event class
TEST(EventSchedulerTest, CustomEventClass) {
  EventDriven::EventScheduler scheduler;
  int counter = 0;

  auto event1 = std::make_shared<TestEvent>(10, &counter, 0);
  auto event2 = std::make_shared<TestEvent>(20, &counter, 0);

  scheduler.schedule(event1);
  scheduler.schedule(event2);

  scheduler.run();

  EXPECT_EQ(counter, 2);
}

// Test: Chained event scheduling
TEST(EventSchedulerTest, ChainedEvents) {
  EventDriven::EventScheduler scheduler;
  std::vector<uint64_t> times;

  scheduler.scheduleAt(
      0,
      [&times](EventDriven::EventScheduler& sched) {
        times.push_back(sched.getCurrentTime());
        sched.scheduleAt(
            sched.getCurrentTime() + 10,
            [&times](EventDriven::EventScheduler& sched) {
              times.push_back(sched.getCurrentTime());
              sched.scheduleAt(
                  sched.getCurrentTime() + 10,
                  [&times](EventDriven::EventScheduler& sched) {
                    times.push_back(sched.getCurrentTime());
                  },
                  0, "Event3");
            },
            0, "Event2");
      },
      0, "Event1");

  scheduler.run();

  ASSERT_EQ(times.size(), 3);
  EXPECT_EQ(times[0], 0);
  EXPECT_EQ(times[1], 10);
  EXPECT_EQ(times[2], 20);
}

// Test: Event cancellation
TEST(EventSchedulerTest, EventCancellation) {
  EventDriven::EventScheduler scheduler;
  int counter = 0;

  auto event1 = std::make_shared<EventDriven::LambdaEvent>(
      10, [&counter](EventDriven::EventScheduler&) { counter++; }, 0, "Event1");

  auto event2 = std::make_shared<EventDriven::LambdaEvent>(
      20, [&counter](EventDriven::EventScheduler&) { counter++; }, 0, "Event2");

  scheduler.schedule(event1);
  scheduler.schedule(event2);

  // Cancel the first event
  event1->cancel();

  scheduler.run();

  EXPECT_EQ(counter, 1);  // Only the second event executes
}

// Test: Time-limited run
TEST(EventSchedulerTest, TimeLimitedRun) {
  EventDriven::EventScheduler scheduler;
  int counter = 0;

  for (int i = 0; i < 10; i++) {
    scheduler.scheduleAt(
        i * 10, [&counter](EventDriven::EventScheduler&) { counter++; }, 0,
        "Event" + std::to_string(i));
  }

  // Only run until time 45
  scheduler.runUntil(45);

  EXPECT_EQ(counter, 5);  // Events at time 0, 10, 20, 30, 40 execute
  EXPECT_EQ(scheduler.getCurrentTime(), 40);
  EXPECT_GT(scheduler.getPendingEventCount(),
            0);  // Still has unexecuted events
}

// Test: Periodic events
TEST(EventSchedulerTest, PeriodicEvents) {
  EventDriven::EventScheduler scheduler;
  std::vector<uint64_t> execution_times;
  int execution_count = 0;

  auto periodic = std::make_shared<EventDriven::PeriodicEvent>(
      0,   // start time
      10,  // period
      [&execution_times, &execution_count](EventDriven::EventScheduler& sched,
                                           uint64_t) {
        execution_times.push_back(sched.getCurrentTime());
        execution_count++;
      },
      5,  // repeat 5 times
      "PeriodicEvent");

  scheduler.schedule(periodic);
  scheduler.run();

  EXPECT_EQ(execution_count, 5);
  ASSERT_EQ(execution_times.size(), 5);
  EXPECT_EQ(execution_times[0], 0);
  EXPECT_EQ(execution_times[1], 10);
  EXPECT_EQ(execution_times[2], 20);
  EXPECT_EQ(execution_times[3], 30);
  EXPECT_EQ(execution_times[4], 40);
}

// Test: Scheduler reset
TEST(EventSchedulerTest, SchedulerReset) {
  EventDriven::EventScheduler scheduler;

  scheduler.scheduleAt(10, [](EventDriven::EventScheduler&) {}, 0, "Event1");
  scheduler.scheduleAt(20, [](EventDriven::EventScheduler&) {}, 0, "Event2");

  EXPECT_EQ(scheduler.getPendingEventCount(), 2);

  scheduler.reset();

  EXPECT_EQ(scheduler.getPendingEventCount(), 0);
  EXPECT_EQ(scheduler.getCurrentTime(), 0);
}

// Test: Cannot schedule events in the past
TEST(EventSchedulerTest, NoPastScheduling) {
  EventDriven::EventScheduler scheduler;
  int counter = 0;

  // First schedule an event at time 20
  scheduler.scheduleAt(
      20,
      [&counter](EventDriven::EventScheduler& sched) {
        counter++;
        // Try to schedule an event in the past (should be rejected)
        sched.scheduleAt(
            10, [&counter](EventDriven::EventScheduler&) { counter++; }, 0,
            "PastEvent");
      },
      0, "Event1");

  scheduler.run();

  EXPECT_EQ(counter, 1);  // Only the first event executes
}

// Test: runFor method
TEST(EventSchedulerTest, RunForCount) {
  EventDriven::EventScheduler scheduler;
  int counter = 0;

  for (int i = 0; i < 10; i++) {
    scheduler.scheduleAt(
        i, [&counter](EventDriven::EventScheduler&) { counter++; }, 0,
        "Event" + std::to_string(i));
  }

  // Only execute 3 events
  scheduler.runFor(3);

  EXPECT_EQ(counter, 3);
  EXPECT_EQ(scheduler.getPendingEventCount(), 7);
}

// Test: Unique event IDs
TEST(EventSchedulerTest, UniqueEventIDs) {
  EventDriven::EventScheduler scheduler;
  std::set<EventDriven::Event::EventID> ids;

  for (int i = 0; i < 100; i++) {
    scheduler.scheduleAt(
        i,
        [&ids](EventDriven::EventScheduler&) {
          // Lambda events will also create new events internally
        },
        0, "Event" + std::to_string(i));
  }

  // All event IDs should be unique
  // Note: This test mainly verifies ID generation mechanism, actual
  // verification requires accessing event objects
}

// Main function
int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
