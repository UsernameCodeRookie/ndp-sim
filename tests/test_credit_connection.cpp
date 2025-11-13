#include <gtest/gtest.h>

#include <iostream>
#include <memory>
#include <queue>

#include "../src/scheduler.h"
#include "../src/tick.h"
#include "../src/trace.h"
#include "connections/credit.h"
#include "connections/ready_valid.h"

using namespace Architecture;

/*
 * Basic producer-consumer test using credit-based connection.
 * - Producer writes integers to out port each tick.
 * - Consumer reads, processes slower than producer, and publishes credits
 *   representing available buffer space via an IntDataPacket on credit_out.
 * - The CreditConnection uses the credit port to stall the producer when
 *   credits are exhausted.
 */
class Producer : public TickingComponent {
 public:
  Producer(const std::string& name, EventDriven::EventScheduler& scheduler,
           uint64_t period, int num_to_send)
      : TickingComponent(name, scheduler, period),
        next_value_(0),
        num_to_send_(num_to_send) {
    addPort("out", PortDirection::OUTPUT);
  }

  void tick() override {
    if (next_value_ < num_to_send_) {
      auto out = getPort("out");
      // Only write if output port is free (avoid overwriting unsent data)
      if (!out->hasData()) {
        out->write(std::make_shared<IntDataPacket>(next_value_));
        TRACE_EVENT(scheduler_.getCurrentTime(), getName(), "PRODUCE",
                    "val=" << next_value_);
        next_value_++;
      }
    }
  }

  int getNextValue() const { return next_value_; }

 private:
  int next_value_;
  int num_to_send_;
};

class Consumer : public TickingComponent {
 public:
  Consumer(const std::string& name, EventDriven::EventScheduler& scheduler,
           uint64_t period, int processing_rate, int buffer_capacity)
      : TickingComponent(name, scheduler, period),
        processing_rate_(processing_rate),
        buffer_capacity_(buffer_capacity),
        processed_(0),
        tick_count_(0),
        available_slots_(buffer_capacity) {
    addPort("in", PortDirection::INPUT);
    addPort("credit_out", PortDirection::OUTPUT);
  }

  void tick() override {
    // Occasionally consume data (slow consumer)
    // Accept incoming data into internal buffer if port has data
    if (getPort("in")->hasData()) {
      auto in = getPort("in");
      // Only accept if internal buffer has space
      if ((int)internal_buffer_.size() < buffer_capacity_) {
        auto data = std::dynamic_pointer_cast<IntDataPacket>(in->read());
        if (data) {
          internal_buffer_.push(data->getValue());
          // Update available slots
          available_slots_ = buffer_capacity_ - (int)internal_buffer_.size();
          TRACE_EVENT(scheduler_.getCurrentTime(), getName(), "ACCEPT",
                      "val=" << data->getValue());
        }
      }
    }

    // Processing stage: occasionally pop from internal buffer
    if (!internal_buffer_.empty() && (tick_count_ % processing_rate_ == 0)) {
      int v = internal_buffer_.front();
      internal_buffer_.pop();
      processed_++;
      // Update available slots
      available_slots_ = buffer_capacity_ - (int)internal_buffer_.size();
      TRACE_EVENT(scheduler_.getCurrentTime(), getName(), "CONSUME",
                  "val=" << v);
    }

    // Publish current available credit count
    auto credit_out = getPort("credit_out");
    credit_out->write(std::make_shared<IntDataPacket>(available_slots_));

    tick_count_++;
  }

  int getProcessed() const { return processed_; }

 private:
  std::queue<int> internal_buffer_;

 private:
  int processing_rate_;  // Consume every N ticks
  int buffer_capacity_;
  int processed_;
  int tick_count_;
  int available_slots_;  // Number of available credits
};

class CreditConnectionTest : public ::testing::Test {
 protected:
  void SetUp() override {
    scheduler = std::make_unique<EventDriven::EventScheduler>();
    EventDriven::Tracer::getInstance().initialize("test_credit_connection.log",
                                                  true);
    EventDriven::Tracer::getInstance().setVerbose(false);
  }

  void TearDown() override {
    EventDriven::Tracer::getInstance().dump();
    scheduler.reset();
  }

  std::unique_ptr<EventDriven::EventScheduler> scheduler;
};

TEST_F(CreditConnectionTest, BasicCreditFlow) {
  const uint64_t CLOCK_PERIOD = 1;
  const uint64_t CONN_PERIOD = 1;
  const int NUM_TO_SEND = 10;
  const int CONSUMER_RATE = 1;  // consumer consumes every tick (fast consumer)
  const int CONSUMER_BUFFER = 2;  // capacity -> initial credits

  auto producer = std::make_shared<Producer>("Producer", *scheduler,
                                             CLOCK_PERIOD, NUM_TO_SEND);
  auto consumer = std::make_shared<Consumer>(
      "Consumer", *scheduler, CLOCK_PERIOD, CONSUMER_RATE, CONSUMER_BUFFER);

  // Get ports
  auto prod_out = producer->getPort("out");
  auto cons_in = consumer->getPort("in");
  auto cons_credit_out = consumer->getPort("credit_out");

  // Create credit connection and bind ports
  auto conn = std::make_shared<Architecture::CreditConnection>(
      "CreditConn", *scheduler, CONN_PERIOD, 4 /* connection buffer */);
  conn->addSourcePort(prod_out);
  conn->addDestinationPort(cons_in);
  conn->bindCreditPort(cons_credit_out);
  conn->setLatency(0);

  // Start components and connection
  producer->start();
  consumer->start();
  conn->start();

  // Run simulation
  scheduler->run(500);

  std::cout << "Processed: " << consumer->getProcessed() << std::endl;
  std::cout << "Connection transfers: " << conn->getTransfers() << std::endl;
  std::cout << "Connection stalls: " << conn->getStalls() << std::endl;

  EXPECT_EQ(consumer->getProcessed(), NUM_TO_SEND);
}

TEST_F(CreditConnectionTest, CreditFlowBackPressure) {
  const uint64_t CLOCK_PERIOD = 1;
  const uint64_t CONN_PERIOD = 1;
  const int NUM_TO_SEND = 10;
  const int CONSUMER_RATE = 3;    // consumer consumes every 3 ticks (slow)
  const int CONSUMER_BUFFER = 2;  // consumer internal buffer capacity

  auto producer = std::make_shared<Producer>("Producer", *scheduler,
                                             CLOCK_PERIOD, NUM_TO_SEND);
  auto consumer = std::make_shared<Consumer>(
      "Consumer", *scheduler, CLOCK_PERIOD, CONSUMER_RATE, CONSUMER_BUFFER);

  auto prod_out = producer->getPort("out");
  auto cons_in = consumer->getPort("in");
  auto cons_credit_out = consumer->getPort("credit_out");

  // Create connection with small buffer to force backpressure
  auto conn = std::make_shared<Architecture::CreditConnection>(
      "CreditConn", *scheduler, CONN_PERIOD, 1 /* conn buffer */);
  conn->addSourcePort(prod_out);
  conn->addDestinationPort(cons_in);
  conn->bindCreditPort(cons_credit_out);
  conn->setLatency(0);

  producer->start();
  consumer->start();
  conn->start();

  scheduler->run(2000);

  std::cout << "Processed: " << consumer->getProcessed() << std::endl;
  std::cout << "Transfers: " << conn->getTransfers() << std::endl;
  std::cout << "Stalls: " << conn->getStalls() << std::endl;

  // All items should be eventually processed
  EXPECT_EQ(consumer->getProcessed(), NUM_TO_SEND);
  // With a slower consumer and small buffers, stalls should be > 0
  EXPECT_GT(conn->getStalls(), 0);
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
