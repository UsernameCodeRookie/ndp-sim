#include <gtest/gtest.h>

#include "../src/comp/core/lsu.h"
#include "../src/scheduler.h"
#include "../src/trace.h"

class LSUTest : public ::testing::Test {
 protected:
  void SetUp() override {
    scheduler = std::make_unique<EventDriven::EventScheduler>();
  }

  void TearDown() override { scheduler.reset(); }

  std::unique_ptr<EventDriven::EventScheduler> scheduler;
};

TEST_F(LSUTest, MemoryBankCreation) {
  auto bank = std::make_shared<MemoryBank>("test_bank", *scheduler, 1, 64, 3);
  bank->start();

  EXPECT_TRUE(bank->isReady());
  EXPECT_FALSE(bank->isDone());
}

TEST_F(LSUTest, MemoryBankReadWrite) {
  auto bank = std::make_shared<MemoryBank>("test_bank", *scheduler, 1, 64, 3);
  bank->start();

  // Write to memory
  bank->write(0, 42);
  bank->write(5, 100);
  bank->write(10, 255);

  // Read from memory
  EXPECT_EQ(bank->read(0), 42);
  EXPECT_EQ(bank->read(5), 100);
  EXPECT_EQ(bank->read(10), 255);

  // Read uninitialized address
  EXPECT_EQ(bank->read(20), 0);
}

TEST_F(LSUTest, MemoryBankOutOfBounds) {
  auto bank = std::make_shared<MemoryBank>("test_bank", *scheduler, 1, 64, 3);
  bank->start();

  // Write out of bounds (should be ignored)
  bank->write(100, 42);

  // Read out of bounds (should return 0)
  EXPECT_EQ(bank->read(100), 0);
}

TEST_F(LSUTest, MemoryBankLoadRequest) {
  auto bank = std::make_shared<MemoryBank>("test_bank", *scheduler, 1, 64, 3);
  bank->start();

  // Initialize memory
  bank->write(10, 123);

  // Create load request
  auto request = std::make_shared<MemoryRequestPacket>(LSUOp::LOAD, 10);

  EXPECT_TRUE(bank->isReady());

  // Process request
  bank->processRequest(request);
  EXPECT_FALSE(bank->isReady());

  // Simulate latency (3 cycles)
  for (int i = 0; i < 3; i++) {
    bank->tick();
  }

  // Check if done
  EXPECT_TRUE(bank->isDone());

  // Get response
  auto response = bank->getResponse();
  ASSERT_NE(response, nullptr);
  EXPECT_EQ(response->data, 123);
  EXPECT_EQ(response->address, 10);
}

TEST_F(LSUTest, MemoryBankStoreRequest) {
  auto bank = std::make_shared<MemoryBank>("test_bank", *scheduler, 1, 64, 3);
  bank->start();

  // Create store request
  auto request = std::make_shared<MemoryRequestPacket>(LSUOp::STORE, 15, 999);

  // Process request
  bank->processRequest(request);

  // Simulate latency
  for (int i = 0; i < 3; i++) {
    bank->tick();
  }

  EXPECT_TRUE(bank->isDone());

  // Verify data was stored
  EXPECT_EQ(bank->read(15), 999);
}

TEST_F(LSUTest, LSUCreation) {
  auto lsu =
      std::make_shared<LoadStoreUnit>("test_lsu", *scheduler, 1, 4, 2, 64);
  lsu->start();

  // Check that LSU has the required ports
  EXPECT_NE(lsu->getPort("req_in"), nullptr);
  EXPECT_NE(lsu->getPort("resp_out"), nullptr);
  EXPECT_NE(lsu->getPort("ready"), nullptr);
  EXPECT_NE(lsu->getPort("valid"), nullptr);
}

TEST_F(LSUTest, LSULoadOperation) {
  auto lsu =
      std::make_shared<LoadStoreUnit>("test_lsu", *scheduler, 1, 4, 2, 64);
  lsu->start();

  auto req_in = lsu->getPort("req_in");
  auto valid_in = lsu->getPort("valid");
  auto resp_out = lsu->getPort("resp_out");

  // First, store a value
  auto store_request =
      std::make_shared<MemoryRequestPacket>(LSUOp::STORE, 5, 777);
  req_in->write(store_request);
  valid_in->write(std::make_shared<Architecture::IntDataPacket>(1));

  // Process store operation
  for (int i = 0; i < 10; i++) {
    lsu->tick();
    if (resp_out->hasData()) {
      auto resp =
          std::dynamic_pointer_cast<MemoryResponsePacket>(resp_out->read());
      break;
    }
  }

  // Now load the value
  auto load_request = std::make_shared<MemoryRequestPacket>(LSUOp::LOAD, 5);
  req_in->write(load_request);
  valid_in->write(std::make_shared<Architecture::IntDataPacket>(1));

  // Wait for response
  bool got_response = false;
  for (int i = 0; i < 10; i++) {
    lsu->tick();
    if (resp_out->hasData()) {
      auto response =
          std::dynamic_pointer_cast<MemoryResponsePacket>(resp_out->read());
      ASSERT_NE(response, nullptr);
      EXPECT_EQ(response->data, 777);
      EXPECT_EQ(response->address, 5);
      got_response = true;
      break;
    }
  }

  ASSERT_TRUE(got_response);
}

TEST_F(LSUTest, LSUStoreOperation) {
  auto lsu =
      std::make_shared<LoadStoreUnit>("test_lsu", *scheduler, 1, 4, 2, 64);
  lsu->start();

  auto req_in = lsu->getPort("req_in");
  auto valid_in = lsu->getPort("valid");
  auto resp_out = lsu->getPort("resp_out");

  // Store a value
  auto store_request =
      std::make_shared<MemoryRequestPacket>(LSUOp::STORE, 20, 888);
  req_in->write(store_request);
  valid_in->write(std::make_shared<Architecture::IntDataPacket>(1));

  // Process store request
  for (int i = 0; i < 10; i++) {
    lsu->tick();
    if (resp_out->hasData()) {
      resp_out->read();  // Acknowledge response
      break;
    }
  }

  // Verify by loading it back
  auto load_request = std::make_shared<MemoryRequestPacket>(LSUOp::LOAD, 20);
  req_in->write(load_request);
  valid_in->write(std::make_shared<Architecture::IntDataPacket>(1));

  bool got_response = false;
  for (int i = 0; i < 10; i++) {
    lsu->tick();
    if (resp_out->hasData()) {
      auto response =
          std::dynamic_pointer_cast<MemoryResponsePacket>(resp_out->read());
      ASSERT_NE(response, nullptr);
      EXPECT_EQ(response->data, 888);
      got_response = true;
      break;
    }
  }

  ASSERT_TRUE(got_response);
}

TEST_F(LSUTest, LSUMultipleRequests) {
  auto lsu =
      std::make_shared<LoadStoreUnit>("test_lsu", *scheduler, 1, 4, 2, 64);
  lsu->start();

  auto req_in = lsu->getPort("req_in");
  auto valid_in = lsu->getPort("valid");
  auto resp_out = lsu->getPort("resp_out");

  // Store multiple values first
  std::vector<std::pair<uint32_t, int32_t>> data = {{0, 10}, {1, 20}, {2, 30}};

  for (auto [addr, value] : data) {
    auto request =
        std::make_shared<MemoryRequestPacket>(LSUOp::STORE, addr, value);
    req_in->write(request);
    valid_in->write(std::make_shared<Architecture::IntDataPacket>(1));

    // Process store
    for (int i = 0; i < 10; i++) {
      lsu->tick();
      if (resp_out->hasData()) {
        resp_out->read();
        break;
      }
    }
  }

  // Now load them back
  for (auto [addr, expected_value] : data) {
    auto request = std::make_shared<MemoryRequestPacket>(LSUOp::LOAD, addr);
    req_in->write(request);
    valid_in->write(std::make_shared<Architecture::IntDataPacket>(1));

    // Process load request
    bool got_response = false;
    for (int i = 0; i < 10; i++) {
      lsu->tick();
      if (resp_out->hasData()) {
        auto response =
            std::dynamic_pointer_cast<MemoryResponsePacket>(resp_out->read());
        ASSERT_NE(response, nullptr);
        EXPECT_EQ(response->data, expected_value);
        got_response = true;
        break;
      }
    }
    ASSERT_TRUE(got_response) << "Failed to get response for address " << addr;
  }
}

TEST_F(LSUTest, LSUReadySignal) {
  auto lsu = std::make_shared<LoadStoreUnit>("test_lsu", *scheduler, 1, 2, 2,
                                             64);  // Small queue
  lsu->start();

  auto ready_out = lsu->getPort("ready");

  // Initially should be ready
  lsu->tick();

  ASSERT_TRUE(ready_out->hasData());
  auto ready =
      std::dynamic_pointer_cast<Architecture::IntDataPacket>(ready_out->read());
  ASSERT_NE(ready, nullptr);
}

TEST_F(LSUTest, MemoryRequestPacketClone) {
  auto request =
      std::make_shared<MemoryRequestPacket>(LSUOp::LOAD, 100, 50, 2, 4, true);

  EXPECT_EQ(request->op, LSUOp::LOAD);
  EXPECT_EQ(request->address, 100);
  EXPECT_EQ(request->data, 50);
  EXPECT_EQ(request->stride, 2);
  EXPECT_EQ(request->length, 4);
  EXPECT_TRUE(request->mask);

  // Test clone
  auto cloned =
      std::dynamic_pointer_cast<MemoryRequestPacket>(request->clone());
  ASSERT_NE(cloned, nullptr);
  EXPECT_EQ(cloned->op, LSUOp::LOAD);
  EXPECT_EQ(cloned->address, 100);
  EXPECT_EQ(cloned->data, 50);
}

TEST_F(LSUTest, MemoryResponsePacketClone) {
  auto response = std::make_shared<MemoryResponsePacket>(42, 200);

  EXPECT_EQ(response->data, 42);
  EXPECT_EQ(response->address, 200);

  // Test clone
  auto cloned =
      std::dynamic_pointer_cast<MemoryResponsePacket>(response->clone());
  ASSERT_NE(cloned, nullptr);
  EXPECT_EQ(cloned->data, 42);
  EXPECT_EQ(cloned->address, 200);
}

TEST_F(LSUTest, MemoryRequestPacketModification) {
  auto request = std::make_shared<MemoryRequestPacket>(LSUOp::LOAD, 50);

  // Modify request
  request->op = LSUOp::STORE;
  request->address = 100;
  request->data = 999;

  EXPECT_EQ(request->op, LSUOp::STORE);
  EXPECT_EQ(request->address, 100);
  EXPECT_EQ(request->data, 999);
}

TEST_F(LSUTest, MultipleBanks) {
  auto lsu = std::make_shared<LoadStoreUnit>("test_lsu", *scheduler, 1, 4, 4,
                                             64);  // 4 banks
  lsu->start();

  auto req_in = lsu->getPort("req_in");
  auto valid_in = lsu->getPort("valid");
  auto resp_out = lsu->getPort("resp_out");

  // Store values that will go to different banks (addresses 0, 1, 2, 3 map to
  // banks based on % num_banks)
  std::vector<std::pair<uint32_t, int32_t>> data = {
      {0, 100}, {1, 200}, {2, 300}, {3, 400}};

  for (auto [addr, value] : data) {
    auto store_request =
        std::make_shared<MemoryRequestPacket>(LSUOp::STORE, addr, value);
    req_in->write(store_request);
    valid_in->write(std::make_shared<Architecture::IntDataPacket>(1));

    for (int i = 0; i < 10; i++) {
      lsu->tick();
      if (resp_out->hasData()) {
        resp_out->read();
        break;
      }
    }
  }

  // Load them back to verify different banks work
  for (auto [addr, expected_value] : data) {
    auto load_request =
        std::make_shared<MemoryRequestPacket>(LSUOp::LOAD, addr);
    req_in->write(load_request);
    valid_in->write(std::make_shared<Architecture::IntDataPacket>(1));

    bool got_response = false;
    for (int i = 0; i < 10; i++) {
      lsu->tick();
      if (resp_out->hasData()) {
        auto response =
            std::dynamic_pointer_cast<MemoryResponsePacket>(resp_out->read());
        ASSERT_NE(response, nullptr);
        EXPECT_EQ(response->data, expected_value)
            << "Failed for address " << addr;
        got_response = true;
        break;
      }
    }
    ASSERT_TRUE(got_response) << "Failed to get response for address " << addr;
  }
}

// Event-driven execution mode tests
TEST_F(LSUTest, EventDrivenMemoryAccess) {
  EventDriven::Tracer::getInstance().initialize("test_lsu_event_driven.log",
                                                true);

  auto lsu =
      std::make_shared<LoadStoreUnit>("test_lsu_ed", *scheduler, 2, 4, 4, 64);
  lsu->start();

  auto req_in = lsu->getPort("req_in");
  auto valid_in = lsu->getPort("valid");
  auto resp_out = lsu->getPort("resp_out");

  // Schedule store operation at time 0
  scheduler->scheduleAt(0, [&](EventDriven::EventScheduler& sched) {
    auto store_req =
        std::make_shared<MemoryRequestPacket>(LSUOp::STORE, 10, 999);
    req_in->write(store_req);
    valid_in->write(std::make_shared<Architecture::IntDataPacket>(1));
    EventDriven::Tracer::getInstance().traceMemoryWrite(sched.getCurrentTime(),
                                                        "Test", 10, 999);
  });

  // Schedule load operation at time 10
  scheduler->scheduleAt(10, [&](EventDriven::EventScheduler& sched) {
    auto load_req = std::make_shared<MemoryRequestPacket>(LSUOp::LOAD, 10);
    req_in->write(load_req);
    valid_in->write(std::make_shared<Architecture::IntDataPacket>(1));
    EventDriven::Tracer::getInstance().traceMemoryRead(sched.getCurrentTime(),
                                                       "Test", 10, 0);
  });

  // Run simulation
  scheduler->run(30);

  // Verify load result
  bool found_result = false;
  while (resp_out->hasData()) {
    auto response =
        std::dynamic_pointer_cast<MemoryResponsePacket>(resp_out->read());
    if (response && response->address == 10) {
      EXPECT_EQ(response->data, 999);
      found_result = true;
    }
  }
  ASSERT_TRUE(found_result);

  std::cout << "\n=== Event-Driven LSU Memory Access ===" << std::endl;
  std::cout << "Store and Load verified" << std::endl;
  std::cout << "Final time: " << scheduler->getCurrentTime() << std::endl;

  EventDriven::Tracer::getInstance().dump();
  EventDriven::Tracer::getInstance().setEnabled(false);
}

TEST_F(LSUTest, EventDrivenMultipleBankAccess) {
  EventDriven::Tracer::getInstance().initialize("test_lsu_multiple_banks.log",
                                                true);

  auto lsu = std::make_shared<LoadStoreUnit>("test_lsu_banks", *scheduler, 2, 4,
                                             8, 64);
  lsu->start();

  auto req_in = lsu->getPort("req_in");
  auto valid_in = lsu->getPort("valid");
  auto resp_out = lsu->getPort("resp_out");

  std::map<uint32_t, int32_t> results;

  // Test 2 banks to verify multi-bank operation
  // Bank 0: address 0
  scheduler->scheduleAt(1, [&](EventDriven::EventScheduler& sched) {
    auto store_req =
        std::make_shared<MemoryRequestPacket>(LSUOp::STORE, 0, 100);
    req_in->write(store_req);
    valid_in->write(std::make_shared<Architecture::IntDataPacket>(1));
    EventDriven::Tracer::getInstance().traceMemoryWrite(sched.getCurrentTime(),
                                                        "Test", 0, 100);
  });

  scheduler->scheduleAt(10, [&](EventDriven::EventScheduler& sched) {
    auto load_req = std::make_shared<MemoryRequestPacket>(LSUOp::LOAD, 0);
    req_in->write(load_req);
    valid_in->write(std::make_shared<Architecture::IntDataPacket>(1));
    EventDriven::Tracer::getInstance().traceMemoryRead(sched.getCurrentTime(),
                                                       "Test", 0, 0);
  });

  scheduler->scheduleAt(15, [&](EventDriven::EventScheduler&) {
    if (resp_out->hasData()) {
      auto response =
          std::dynamic_pointer_cast<MemoryResponsePacket>(resp_out->read());
      if (response) results[response->address] = response->data;
    }
  });

  // Bank 1: address 1
  scheduler->scheduleAt(20, [&](EventDriven::EventScheduler& sched) {
    auto store_req =
        std::make_shared<MemoryRequestPacket>(LSUOp::STORE, 1, 200);
    req_in->write(store_req);
    valid_in->write(std::make_shared<Architecture::IntDataPacket>(1));
    EventDriven::Tracer::getInstance().traceMemoryWrite(sched.getCurrentTime(),
                                                        "Test", 1, 200);
  });

  scheduler->scheduleAt(30, [&](EventDriven::EventScheduler& sched) {
    auto load_req = std::make_shared<MemoryRequestPacket>(LSUOp::LOAD, 1);
    req_in->write(load_req);
    valid_in->write(std::make_shared<Architecture::IntDataPacket>(1));
    EventDriven::Tracer::getInstance().traceMemoryRead(sched.getCurrentTime(),
                                                       "Test", 1, 0);
  });

  scheduler->scheduleAt(35, [&](EventDriven::EventScheduler&) {
    if (resp_out->hasData()) {
      auto response =
          std::dynamic_pointer_cast<MemoryResponsePacket>(resp_out->read());
      if (response) results[response->address] = response->data;
    }
  });

  // Run simulation
  scheduler->run(50);

  // Collect any remaining responses
  std::cout << "\n=== Checking LSU responses ===" << std::endl;
  while (resp_out->hasData()) {
    auto response =
        std::dynamic_pointer_cast<MemoryResponsePacket>(resp_out->read());
    if (response) {
      std::cout << "Got response: addr=" << response->address
                << " data=" << response->data << std::endl;
      results[response->address] = response->data;
    }
  }
  std::cout << "Total responses: " << results.size() << std::endl;

  // Check results - verify we got responses from both banks
  ASSERT_GE(results.size(), 2) << "Should have responses from at least 2 banks";
  std::cout << "Bank 0 data: " << results[0] << std::endl;
  std::cout << "Bank 1 data: " << results[1] << std::endl;

  std::cout << "\n=== Event-Driven Multiple Bank Access ===" << std::endl;
  std::cout << "Banks accessed: " << results.size() << std::endl;
  std::cout << "Final time: " << scheduler->getCurrentTime() << std::endl;
  std::cout << "Operations completed: " << lsu->getOperationsCompleted()
            << std::endl;

  EventDriven::Tracer::getInstance().dump();
  EventDriven::Tracer::getInstance().setEnabled(false);
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
