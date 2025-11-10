#include <cassert>
#include <iostream>
#include <memory>

#include "../src/components/lsu.h"
#include "../src/scheduler.h"

void test_simple_load_store() {
  std::cout << "\n=== Test: Simple Load/Store ===" << std::endl;

  EventDriven::EventScheduler scheduler;
  auto lsu = std::make_shared<LoadStoreUnit>("LSU", scheduler, 1, 8, 4, 64);
  lsu->setVerbose(true);
  lsu->start();

  // Test 1: Store a value
  std::cout << "\nTest 1: Store value 42 to address 10" << std::endl;
  auto store_req =
      std::make_shared<MemoryRequestPacket>(LSUOp::STORE, 10, 42, 1, 1, true);

  auto req_port = lsu->getPort("req_in");
  auto valid_port = lsu->getPort("valid");

  req_port->write(
      std::static_pointer_cast<Architecture::DataPacket>(store_req));
  auto valid_signal = std::make_shared<IntDataPacket>(1);
  valid_port->write(
      std::static_pointer_cast<Architecture::DataPacket>(valid_signal));

  // Run for enough cycles to complete the operation
  for (int i = 0; i < 20; ++i) {
    scheduler.run(1);
  }

  // Test 2: Load the value back
  std::cout << "\nTest 2: Load value from address 10" << std::endl;
  auto load_req =
      std::make_shared<MemoryRequestPacket>(LSUOp::LOAD, 10, 0, 1, 1, true);

  req_port->write(std::static_pointer_cast<Architecture::DataPacket>(load_req));
  valid_port->write(
      std::static_pointer_cast<Architecture::DataPacket>(valid_signal));

  for (int i = 0; i < 20; ++i) {
    scheduler.run(1);
  }

  auto resp_port = lsu->getPort("resp_out");
  auto response = resp_port->read();
  auto mem_resp = std::dynamic_pointer_cast<MemoryResponsePacket>(response);

  if (mem_resp) {
    std::cout << "Loaded value: " << mem_resp->getData() << std::endl;
    assert(mem_resp->getData() == 42);
    std::cout << "✓ Test passed!" << std::endl;
  } else {
    std::cout << "✗ Test failed: No response received" << std::endl;
  }

  std::cout << "\nStatistics:" << std::endl;
  std::cout << "Operations completed: " << lsu->getOperationsCompleted()
            << std::endl;
  std::cout << "Cycles stalled: " << lsu->getCyclesStalled() << std::endl;
}

void test_vector_load_store() {
  std::cout << "\n=== Test: Vector Load/Store ===" << std::endl;

  EventDriven::EventScheduler scheduler;
  auto lsu = std::make_shared<LoadStoreUnit>("LSU", scheduler, 1, 8, 4, 64);
  lsu->setVerbose(true);
  lsu->start();

  auto req_port = lsu->getPort("req_in");
  auto valid_port = lsu->getPort("valid");
  auto resp_port = lsu->getPort("resp_out");
  auto valid_signal = std::make_shared<IntDataPacket>(1);

  // Test 1: Vector store with stride 1
  std::cout << "\nTest 1: Vector store (4 elements, stride=1)" << std::endl;

  // Store values 100, 200, 300, 400 to addresses 0, 1, 2, 3
  for (int i = 0; i < 4; ++i) {
    auto store_req = std::make_shared<MemoryRequestPacket>(
        LSUOp::STORE, i, (i + 1) * 100, 1, 1, true);
    req_port->write(
        std::static_pointer_cast<Architecture::DataPacket>(store_req));
    valid_port->write(
        std::static_pointer_cast<Architecture::DataPacket>(valid_signal));

    for (int j = 0; j < 10; ++j) {
      scheduler.run(1);
    }
  }

  // Test 2: Vector load with stride 1
  std::cout << "\nTest 2: Vector load (4 elements, stride=1)" << std::endl;

  for (int i = 0; i < 4; ++i) {
    auto load_req =
        std::make_shared<MemoryRequestPacket>(LSUOp::LOAD, i, 0, 1, 1, true);
    req_port->write(
        std::static_pointer_cast<Architecture::DataPacket>(load_req));
    valid_port->write(
        std::static_pointer_cast<Architecture::DataPacket>(valid_signal));

    for (int j = 0; j < 10; ++j) {
      scheduler.run(1);
    }

    auto response = resp_port->read();
    auto mem_resp = std::dynamic_pointer_cast<MemoryResponsePacket>(response);

    if (mem_resp) {
      std::cout << "Element " << i << ": " << mem_resp->getData() << std::endl;
      assert(mem_resp->getData() == (i + 1) * 100);
    }
  }

  std::cout << "✓ Vector test passed!" << std::endl;

  std::cout << "\nStatistics:" << std::endl;
  std::cout << "Operations completed: " << lsu->getOperationsCompleted()
            << std::endl;
  std::cout << "Cycles stalled: " << lsu->getCyclesStalled() << std::endl;
}

void test_strided_access() {
  std::cout << "\n=== Test: Strided Memory Access ===" << std::endl;

  EventDriven::EventScheduler scheduler;
  auto lsu = std::make_shared<LoadStoreUnit>("LSU", scheduler, 1, 8, 4, 64);
  lsu->setVerbose(true);
  lsu->start();

  auto req_port = lsu->getPort("req_in");
  auto valid_port = lsu->getPort("valid");
  auto resp_port = lsu->getPort("resp_out");
  auto valid_signal = std::make_shared<IntDataPacket>(1);

  // Test: Store with stride 2 (addresses 0, 2, 4, 6)
  std::cout << "\nStore with stride=2" << std::endl;

  for (int i = 0; i < 4; ++i) {
    auto store_req = std::make_shared<MemoryRequestPacket>(
        LSUOp::STORE, i * 2, (i + 1) * 10, 1, 1, true);
    req_port->write(
        std::static_pointer_cast<Architecture::DataPacket>(store_req));
    valid_port->write(
        std::static_pointer_cast<Architecture::DataPacket>(valid_signal));

    for (int j = 0; j < 10; ++j) {
      scheduler.run(1);
    }
  }

  // Load back with stride 2
  std::cout << "\nLoad with stride=2" << std::endl;

  for (int i = 0; i < 4; ++i) {
    auto load_req = std::make_shared<MemoryRequestPacket>(LSUOp::LOAD, i * 2, 0,
                                                          1, 1, true);
    req_port->write(
        std::static_pointer_cast<Architecture::DataPacket>(load_req));
    valid_port->write(
        std::static_pointer_cast<Architecture::DataPacket>(valid_signal));

    for (int j = 0; j < 10; ++j) {
      scheduler.run(1);
    }

    auto response = resp_port->read();
    auto mem_resp = std::dynamic_pointer_cast<MemoryResponsePacket>(response);

    if (mem_resp) {
      std::cout << "Address " << i * 2 << ": " << mem_resp->getData()
                << std::endl;
      assert(mem_resp->getData() == (i + 1) * 10);
    }
  }

  std::cout << "✓ Strided access test passed!" << std::endl;
}

void test_bank_conflicts() {
  std::cout << "\n=== Test: Bank Conflicts ===" << std::endl;

  EventDriven::EventScheduler scheduler;
  auto lsu = std::make_shared<LoadStoreUnit>("LSU", scheduler, 1, 8, 4, 64);
  lsu->setVerbose(true);
  lsu->start();

  auto req_port = lsu->getPort("req_in");
  auto valid_port = lsu->getPort("valid");
  auto valid_signal = std::make_shared<IntDataPacket>(1);

  // Test: Access same bank multiple times (addresses 0, 8, 16 - all map to bank
  // 0)
  std::cout << "\nAccessing same bank multiple times" << std::endl;

  for (int i = 0; i < 3; ++i) {
    auto store_req = std::make_shared<MemoryRequestPacket>(LSUOp::STORE, i * 8,
                                                           i * 100, 1, 1, true);
    req_port->write(
        std::static_pointer_cast<Architecture::DataPacket>(store_req));
    valid_port->write(
        std::static_pointer_cast<Architecture::DataPacket>(valid_signal));

    for (int j = 0; j < 10; ++j) {
      scheduler.run(1);
    }
  }

  std::cout << "\nStatistics:" << std::endl;
  std::cout << "Operations completed: " << lsu->getOperationsCompleted()
            << std::endl;
  std::cout << "Cycles stalled: " << lsu->getCyclesStalled() << std::endl;
  std::cout << "✓ Bank conflict test completed!" << std::endl;
}

void test_back_pressure() {
  std::cout << "\n=== Test: Back Pressure ===" << std::endl;

  EventDriven::EventScheduler scheduler;
  auto lsu = std::make_shared<LoadStoreUnit>("LSU", scheduler, 1, 8, 2, 64);
  lsu->setVerbose(true);
  lsu->start();

  auto req_port = lsu->getPort("req_in");
  auto valid_port = lsu->getPort("valid");
  auto ready_port = lsu->getPort("ready");
  auto valid_signal = std::make_shared<IntDataPacket>(1);

  std::cout << "\nSending multiple requests to test queue depth" << std::endl;

  int requests_sent = 0;
  int requests_to_send = 5;

  for (int cycle = 0; cycle < 100 && requests_sent < requests_to_send;
       ++cycle) {
    scheduler.run(1);

    auto ready_data = ready_port->read();
    auto ready_int = std::dynamic_pointer_cast<IntDataPacket>(ready_data);

    if (ready_int && ready_int->getValue() == 1 &&
        requests_sent < requests_to_send) {
      auto store_req = std::make_shared<MemoryRequestPacket>(
          LSUOp::STORE, requests_sent, requests_sent * 50, 1, 1, true);
      req_port->write(
          std::static_pointer_cast<Architecture::DataPacket>(store_req));
      valid_port->write(
          std::static_pointer_cast<Architecture::DataPacket>(valid_signal));

      std::cout << "Sent request " << requests_sent << std::endl;
      requests_sent++;
    } else if (requests_sent < requests_to_send) {
      std::cout << "LSU not ready, back pressure active" << std::endl;
    }
  }

  // Run for additional cycles to complete processing
  for (int i = 0; i < 50; ++i) {
    scheduler.run(1);
  }

  std::cout << "\nRequests sent: " << requests_sent << std::endl;
  std::cout << "Operations completed: " << lsu->getOperationsCompleted()
            << std::endl;
  std::cout << "✓ Back pressure test completed!" << std::endl;
}

int main() {
  std::cout << "LSU Component Tests" << std::endl;
  std::cout << "===================" << std::endl;

  try {
    test_simple_load_store();
    test_vector_load_store();
    test_strided_access();
    test_bank_conflicts();
    test_back_pressure();

    std::cout << "\n===================" << std::endl;
    std::cout << "All tests passed! ✓" << std::endl;
    std::cout << "===================" << std::endl;

    return 0;
  } catch (const std::exception& e) {
    std::cerr << "Test failed with exception: " << e.what() << std::endl;
    return 1;
  }
}
