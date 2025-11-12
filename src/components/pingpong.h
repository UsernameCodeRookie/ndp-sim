#ifndef PINGPONG_H
#define PINGPONG_H

#include <memory>
#include <string>
#include <vector>

#include "../component.h"
#include "../packet.h"
#include "../port.h"
#include "../tick.h"
#include "../trace.h"
#include "lsu.h"

/**
 * @brief PingPong Controller
 *
 * Simple controller that sends ping-pong requests to LSU:
 * 1. Store a value (PING)
 * 2. Load the value back (PONG)
 * 3. Verify the data matches
 * 4. Repeat for the next address
 *
 * Demonstrates use of ready-valid connection protocol
 */
class PingPongController : public Architecture::TickingComponent {
 public:
  PingPongController(const std::string& name,
                     EventDriven::EventScheduler& scheduler, uint64_t period,
                     uint32_t num_iterations = 10, uint32_t start_address = 0)
      : Architecture::TickingComponent(name, scheduler, period),
        num_iterations_(num_iterations),
        start_address_(start_address),
        current_state_(State::IDLE),
        iteration_(0),
        current_address_(start_address),
        current_value_(0),
        requests_sent_(0),
        responses_received_(0),
        errors_(0),
        wait_cycles_(0) {
    createPorts();
  }

  void tick() override {
    switch (current_state_) {
      case State::IDLE:
        handleIdle();
        break;
      case State::SEND_STORE:
        handleSendStore();
        break;
      case State::WAIT_STORE:
        handleWaitStore();
        break;
      case State::SEND_LOAD:
        handleSendLoad();
        break;
      case State::WAIT_LOAD:
        handleWaitLoad();
        break;
      case State::DONE:
        handleDone();
        break;
    }
  }

  // Start the controller
  void begin() {
    current_state_ = State::SEND_STORE;
    iteration_ = 0;
    current_address_ = start_address_;
  }

  // Statistics
  bool isDone() const { return current_state_ == State::DONE; }
  uint32_t getRequestsSent() const { return requests_sent_; }
  uint32_t getResponsesReceived() const { return responses_received_; }
  uint32_t getErrors() const { return errors_; }
  uint64_t getWaitCycles() const { return wait_cycles_; }

  void printStatistics() const {
    std::cout << "\n=== PingPong Controller Statistics: " << getName()
              << " ===" << std::endl;
    std::cout << "Iterations completed: " << iteration_ << "/"
              << num_iterations_ << std::endl;
    std::cout << "Requests sent: " << requests_sent_ << std::endl;
    std::cout << "Responses received: " << responses_received_ << std::endl;
    std::cout << "Errors: " << errors_ << std::endl;
    std::cout << "Wait cycles: " << wait_cycles_ << std::endl;
    std::cout << "Average latency: "
              << (responses_received_ > 0
                      ? (double)wait_cycles_ / responses_received_
                      : 0.0)
              << " cycles" << std::endl;
  }

 private:
  enum class State {
    IDLE,        // Initial state
    SEND_STORE,  // Send store request
    WAIT_STORE,  // Wait for store completion
    SEND_LOAD,   // Send load request
    WAIT_LOAD,   // Wait for load response
    DONE         // All iterations complete
  };

  void createPorts() {
    // Output ports to LSU
    addPort("req_out", Architecture::PortDirection::OUTPUT);
    addPort("valid_out", Architecture::PortDirection::OUTPUT);

    // Input ports from LSU
    addPort("resp_in", Architecture::PortDirection::INPUT);
    addPort("ready_in", Architecture::PortDirection::INPUT);
    addPort("done_in", Architecture::PortDirection::INPUT);
  }

  void handleIdle() {
    // Do nothing, waiting for begin() call
  }

  void handleSendStore() {
    auto ready_port = getPort("ready_in");
    auto ready_data =
        std::dynamic_pointer_cast<IntDataPacket>(ready_port->getData());

    // Check if LSU is ready
    if (ready_data && ready_data->getValue() == 1) {
      // Generate a value to store (based on address for easy verification)
      current_value_ = static_cast<int32_t>(current_address_ * 100 + 42);

      // Create store request
      auto store_request = std::make_shared<MemoryRequestPacket>(
          LSUOp::STORE, current_address_, current_value_);

      // Send request
      auto req_out = getPort("req_out");
      auto valid_out = getPort("valid_out");

      req_out->write(store_request);
      valid_out->write(std::make_shared<IntDataPacket>(1));

      requests_sent_++;
      current_state_ = State::WAIT_STORE;
      wait_cycles_ = 0;

      TRACE_EVENT(scheduler_.getCurrentTime(), getName(), "PING_STORE",
                  "STORE addr=0x" << std::hex << current_address_ << std::dec
                                  << " data=" << current_value_
                                  << " iter=" << iteration_);

    } else {
      // LSU not ready, wait
      wait_cycles_++;
      TRACE_EVENT(scheduler_.getCurrentTime(), getName(), "WAIT_READY",
                  "Waiting for LSU ready");
    }
  }

  void handleWaitStore() {
    wait_cycles_++;

    auto resp_port = getPort("resp_in");
    if (resp_port->hasData()) {
      auto response =
          std::dynamic_pointer_cast<MemoryResponsePacket>(resp_port->read());

      if (response) {
        responses_received_++;

        TRACE_EVENT(scheduler_.getCurrentTime(), getName(), "PING_STORE_ACK",
                    "STORE_ACK addr=0x" << std::hex << response->getAddress()
                                        << std::dec
                                        << " wait=" << wait_cycles_);

        // Move to send load request
        current_state_ = State::SEND_LOAD;
      }
    }
  }

  void handleSendLoad() {
    auto ready_port = getPort("ready_in");
    auto ready_data =
        std::dynamic_pointer_cast<IntDataPacket>(ready_port->getData());

    // Check if LSU is ready
    if (ready_data && ready_data->getValue() == 1) {
      // Create load request
      auto load_request =
          std::make_shared<MemoryRequestPacket>(LSUOp::LOAD, current_address_);

      // Send request
      auto req_out = getPort("req_out");
      auto valid_out = getPort("valid_out");

      req_out->write(load_request);
      valid_out->write(std::make_shared<IntDataPacket>(1));

      requests_sent_++;
      current_state_ = State::WAIT_LOAD;
      wait_cycles_ = 0;

      TRACE_EVENT(scheduler_.getCurrentTime(), getName(), "PONG_LOAD",
                  "LOAD addr=0x" << std::hex << current_address_ << std::dec
                                 << " expect=" << current_value_
                                 << " iter=" << iteration_);

    } else {
      // LSU not ready, wait
      wait_cycles_++;
    }
  }

  void handleWaitLoad() {
    wait_cycles_++;

    auto resp_port = getPort("resp_in");
    if (resp_port->hasData()) {
      auto response =
          std::dynamic_pointer_cast<MemoryResponsePacket>(resp_port->read());

      if (response) {
        responses_received_++;

        // Verify data
        int32_t received_data = response->getData();
        if (received_data == current_value_) {
          TRACE_EVENT(scheduler_.getCurrentTime(), getName(), "PONG_VERIFY_OK",
                      "VERIFY_OK addr=0x" << std::hex << response->getAddress()
                                          << std::dec
                                          << " data=" << received_data
                                          << " wait=" << wait_cycles_);
        } else {
          errors_++;
          TRACE_EVENT(scheduler_.getCurrentTime(), getName(),
                      "PONG_VERIFY_ERROR",
                      "VERIFY_ERROR addr=0x"
                          << std::hex << response->getAddress() << std::dec
                          << " expected=" << current_value_
                          << " got=" << received_data);
        }

        // Move to next iteration
        iteration_++;
        current_address_++;

        if (iteration_ >= num_iterations_) {
          current_state_ = State::DONE;
          TRACE_EVENT(scheduler_.getCurrentTime(), getName(),
                      "PINGPONG_COMPLETE", "All iterations completed");
        } else {
          current_state_ = State::SEND_STORE;
        }
      }
    }
  }

  void handleDone() {
    // All iterations completed, stay in DONE state
  }

  uint32_t num_iterations_;
  uint32_t start_address_;
  State current_state_;
  uint32_t iteration_;
  uint32_t current_address_;
  int32_t current_value_;
  uint32_t requests_sent_;
  uint32_t responses_received_;
  uint32_t errors_;
  uint64_t wait_cycles_;
};

#endif  // PINGPONG_H
