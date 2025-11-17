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
  auto bank = std::make_shared<MemoryBank>("test_bank", *scheduler, 1, 64, 2);
  EXPECT_NE(bank, nullptr);
  EXPECT_EQ(bank->getCapacity(), 64);
  EXPECT_EQ(bank->getLatency(), 2);
}

TEST_F(LSUTest, MemoryBankReadWrite) {
  auto bank = std::make_shared<MemoryBank>("test_bank", *scheduler, 1, 64, 2);

  // Write to memory
  bank->store(0, 42);
  bank->store(5, 100);
  bank->store(10, 255);

  // Read from memory
  EXPECT_EQ(bank->load(0), 42);
  EXPECT_EQ(bank->load(5), 100);
  EXPECT_EQ(bank->load(10), 255);

  // Read uninitialized address
  EXPECT_EQ(bank->load(20), 0);
}

TEST_F(LSUTest, MemoryBankOutOfBounds) {
  auto bank = std::make_shared<MemoryBank>("test_bank", *scheduler, 1, 64, 2);

  // Write out of bounds (should be ignored)
  bank->store(100, 42);

  // Read out of bounds (should return 0)
  EXPECT_EQ(bank->load(100), 0);
}

TEST_F(LSUTest, LSUCreation) {
  LoadStoreUnit::Config config;
  config.period = 1;
  config.num_banks = 4;
  config.bank_capacity = 64;
  config.queue_depth = 8;
  config.bank_latency = 2;

  auto lsu = std::make_shared<LoadStoreUnit>("test_lsu", *scheduler, config);
  lsu->start();

  // Check that LSU has the required ports
  EXPECT_NE(lsu->getPort("req_in"), nullptr);
  EXPECT_NE(lsu->getPort("resp_out"), nullptr);
  EXPECT_NE(lsu->getPort("ready"), nullptr);
  EXPECT_NE(lsu->getPort("valid"), nullptr);
}

TEST_F(LSUTest, LSULoadOperation) {
  LoadStoreUnit::Config config;
  config.period = 1;
  config.num_banks = 4;
  config.bank_capacity = 128;
  config.queue_depth = 8;
  config.bank_latency = 2;

  auto lsu = std::make_shared<LoadStoreUnit>("test_lsu", *scheduler, config);
  lsu->start();

  auto req_in = lsu->getPort("req_in");
  auto resp_out = lsu->getPort("resp_out");

  // First, store a value using LSU's loadData method
  lsu->loadData(5, 777);

  // Verify the data was stored
  int32_t read_val = lsu->readData(5);
  EXPECT_EQ(read_val, 777);

  // Now load the value
  auto load_request = std::make_shared<MemoryRequestPacket>(LSUOp::LW, 5, 0);
  req_in->write(load_request);

  // Process through pipeline stages
  // Tick 0: Stage 0 reads request from req_in port
  // Tick 1: Stage 0->Stage 1 transition
  // Tick 2: Stage 1->Stage 2 transition (Stage 2 has response)
  // Tick 3: Stage 2 outputs to resp_out
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
  LoadStoreUnit::Config config;
  config.period = 1;
  config.num_banks = 4;
  config.bank_capacity = 128;
  config.queue_depth = 8;
  config.bank_latency = 2;

  auto lsu = std::make_shared<LoadStoreUnit>("test_lsu", *scheduler, config);
  lsu->start();

  auto req_in = lsu->getPort("req_in");
  auto resp_out = lsu->getPort("resp_out");

  // Store a value
  auto store_request =
      std::make_shared<MemoryRequestPacket>(LSUOp::SW, 20, 888);
  req_in->write(store_request);

  // Process store request
  for (int i = 0; i < 20; i++) {
    lsu->tick();
    if (resp_out->hasData()) {
      resp_out->read();  // Acknowledge response
      break;
    }
  }

  // Verify by reading it back
  int32_t stored_value = lsu->readData(20);
  EXPECT_EQ(stored_value, 888);
}

TEST_F(LSUTest, LSUMultipleRequests) {
  LoadStoreUnit::Config config;
  config.period = 1;
  config.num_banks = 4;
  config.bank_capacity = 128;
  config.queue_depth = 8;
  config.bank_latency = 2;

  auto lsu = std::make_shared<LoadStoreUnit>("test_lsu", *scheduler, config);
  lsu->start();

  auto req_in = lsu->getPort("req_in");
  auto resp_out = lsu->getPort("resp_out");

  // Store multiple values first
  std::vector<std::pair<uint32_t, int32_t>> data = {{0, 10}, {1, 20}, {2, 30}};

  for (auto [addr, value] : data) {
    auto request =
        std::make_shared<MemoryRequestPacket>(LSUOp::SW, addr, value);
    req_in->write(request);

    // Process store
    for (int i = 0; i < 20; i++) {
      lsu->tick();
      if (resp_out->hasData()) {
        resp_out->read();
        break;
      }
    }
  }

  // Verify all values
  for (auto [addr, expected_value] : data) {
    int32_t actual_value = lsu->readData(addr);
    EXPECT_EQ(actual_value, expected_value) << "Failed for address " << addr;
  }
}

TEST_F(LSUTest, MemoryRequestPacketClone) {
  auto request = std::make_shared<MemoryRequestPacket>(LSUOp::LW, 100, 50);

  EXPECT_EQ(request->op, LSUOp::LW);
  EXPECT_EQ(request->address, 100);
  EXPECT_EQ(request->data, 50);

  // Test clone
  auto cloned =
      std::dynamic_pointer_cast<MemoryRequestPacket>(request->clone());
  ASSERT_NE(cloned, nullptr);
  EXPECT_EQ(cloned->op, LSUOp::LW);
  EXPECT_EQ(cloned->address, 100);
  EXPECT_EQ(cloned->data, 50);
}

TEST_F(LSUTest, MemoryResponsePacketClone) {
  auto response = std::make_shared<MemoryResponsePacket>(42, 200, 1);

  EXPECT_EQ(response->data, 42);
  EXPECT_EQ(response->address, 200);
  EXPECT_EQ(response->request_id, 1);

  // Test clone
  auto cloned =
      std::dynamic_pointer_cast<MemoryResponsePacket>(response->clone());
  ASSERT_NE(cloned, nullptr);
  EXPECT_EQ(cloned->data, 42);
  EXPECT_EQ(cloned->address, 200);
}

TEST_F(LSUTest, MultipleBanks) {
  LoadStoreUnit::Config config;
  config.period = 1;
  config.num_banks = 4;
  config.bank_capacity = 128;
  config.queue_depth = 8;
  config.bank_latency = 2;

  auto lsu = std::make_shared<LoadStoreUnit>("test_lsu", *scheduler, config);
  lsu->start();

  auto req_in = lsu->getPort("req_in");
  auto resp_out = lsu->getPort("resp_out");

  // Store values that will go to different banks
  std::vector<std::pair<uint32_t, int32_t>> data = {
      {0, 100}, {4, 200}, {8, 300}, {12, 400}};

  for (auto [addr, value] : data) {
    auto store_request =
        std::make_shared<MemoryRequestPacket>(LSUOp::SW, addr, value);
    req_in->write(store_request);

    for (int i = 0; i < 20; i++) {
      lsu->tick();
      if (resp_out->hasData()) {
        resp_out->read();
        break;
      }
    }
  }

  // Load them back to verify different banks work
  for (auto [addr, expected_value] : data) {
    auto load_request = std::make_shared<MemoryRequestPacket>(LSUOp::LW, addr);
    req_in->write(load_request);

    bool got_response = false;
    for (int i = 0; i < 20; i++) {
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

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
