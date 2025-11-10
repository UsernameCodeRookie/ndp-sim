#include <iostream>
#include <memory>

#include "../src/components/lsu.h"
#include "../src/scheduler.h"

/**
 * @brief Simple LSU Example
 *
 * Demonstrates basic load-store unit functionality:
 * - Memory bank organization
 * - Load/Store operations
 * - Vector operations with stride
 * - Bank conflict handling
 */

void example1_basic_load_store() {
  std::cout << "\n" << std::string(60, '=') << std::endl;
  std::cout << "Example 1: Basic Load and Store Operations" << std::endl;
  std::cout << std::string(60, '=') << std::endl;

  // Create event scheduler and LSU
  EventDriven::EventScheduler scheduler;
  auto lsu = std::make_shared<LoadStoreUnit>("LSU",      // Component name
                                             scheduler,  // Event scheduler
                                             1,          // Clock period
                                             8,  // Number of memory banks
                                             4,  // Request queue depth
                                             64  // Capacity per bank
  );

  lsu->setVerbose(true);
  lsu->start();

  // Get LSU ports
  auto req_port = lsu->getPort("req_in");
  auto resp_port = lsu->getPort("resp_out");
  auto valid_port = lsu->getPort("valid");
  auto ready_port = lsu->getPort("ready");

  // Create valid signal
  auto valid_signal = std::make_shared<IntDataPacket>(1);

  std::cout << "\n1. Storing value 0x12345678 to address 0x00" << std::endl;

  // Create store request
  auto store_req =
      std::make_shared<MemoryRequestPacket>(LSUOp::STORE,  // Operation
                                            0x00,          // Address
                                            0x12345678,    // Data
                                            1,             // Stride
                                            1,             // Length
                                            true           // Mask
      );

  // Send request
  req_port->write(
      std::static_pointer_cast<Architecture::DataPacket>(store_req));
  valid_port->write(
      std::static_pointer_cast<Architecture::DataPacket>(valid_signal));

  // Run simulation
  for (int i = 0; i < 15; ++i) {
    scheduler.run(1);
  }

  std::cout << "\n2. Loading value from address 0x00" << std::endl;

  // Create load request
  auto load_req =
      std::make_shared<MemoryRequestPacket>(LSUOp::LOAD,  // Operation
                                            0x00,         // Address
                                            0,    // Data (unused for load)
                                            1,    // Stride
                                            1,    // Length
                                            true  // Mask
      );

  // Send request
  req_port->write(std::static_pointer_cast<Architecture::DataPacket>(load_req));
  valid_port->write(
      std::static_pointer_cast<Architecture::DataPacket>(valid_signal));

  // Run simulation
  for (int i = 0; i < 15; ++i) {
    scheduler.run(1);
  }

  // Get response
  auto response = resp_port->read();
  auto mem_resp = std::dynamic_pointer_cast<MemoryResponsePacket>(response);

  if (mem_resp && mem_resp->isSuccess()) {
    std::cout << "\n✓ Load successful!" << std::endl;
    std::cout << "  Value: 0x" << std::hex << mem_resp->getData() << std::dec
              << std::endl;
  }

  std::cout << "\nStatistics:" << std::endl;
  std::cout << "  Operations completed: " << lsu->getOperationsCompleted()
            << std::endl;
  std::cout << "  Cycles stalled: " << lsu->getCyclesStalled() << std::endl;
}

void example2_vector_operations() {
  std::cout << "\n" << std::string(60, '=') << std::endl;
  std::cout << "Example 2: Vector Load/Store Operations" << std::endl;
  std::cout << std::string(60, '=') << std::endl;

  EventDriven::EventScheduler scheduler;
  auto lsu = std::make_shared<LoadStoreUnit>("LSU", scheduler, 1, 8, 4, 64);
  lsu->setVerbose(true);
  lsu->start();

  auto req_port = lsu->getPort("req_in");
  auto resp_port = lsu->getPort("resp_out");
  auto valid_port = lsu->getPort("valid");
  auto valid_signal = std::make_shared<IntDataPacket>(1);

  std::cout << "\n1. Vector Store: Writing array {10, 20, 30, 40, 50}"
            << std::endl;

  int data[] = {10, 20, 30, 40, 50};
  int vector_length = 5;

  // Store vector elements
  for (int i = 0; i < vector_length; ++i) {
    auto store_req = std::make_shared<MemoryRequestPacket>(LSUOp::STORE, i,
                                                           data[i], 1, 1, true);

    req_port->write(
        std::static_pointer_cast<Architecture::DataPacket>(store_req));
    valid_port->write(
        std::static_pointer_cast<Architecture::DataPacket>(valid_signal));

    for (int j = 0; j < 10; ++j) {
      scheduler.run(1);
    }
  }

  std::cout << "\n2. Vector Load: Reading back the array" << std::endl;

  // Load vector elements
  for (int i = 0; i < vector_length; ++i) {
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

    if (mem_resp && mem_resp->isSuccess()) {
      std::cout << "  Element[" << i << "] = " << mem_resp->getData()
                << std::endl;
    }
  }

  std::cout << "\n✓ Vector operations completed!" << std::endl;
  std::cout << "\nStatistics:" << std::endl;
  std::cout << "  Operations completed: " << lsu->getOperationsCompleted()
            << std::endl;
  std::cout << "  Cycles stalled: " << lsu->getCyclesStalled() << std::endl;
}

void example3_strided_access() {
  std::cout << "\n" << std::string(60, '=') << std::endl;
  std::cout << "Example 3: Strided Memory Access" << std::endl;
  std::cout << std::string(60, '=') << std::endl;

  EventDriven::EventScheduler scheduler;
  auto lsu = std::make_shared<LoadStoreUnit>("LSU", scheduler, 1, 8, 4, 64);
  lsu->setVerbose(true);
  lsu->start();

  auto req_port = lsu->getPort("req_in");
  auto resp_port = lsu->getPort("resp_out");
  auto valid_port = lsu->getPort("valid");
  auto valid_signal = std::make_shared<IntDataPacket>(1);

  std::cout << "\n1. Strided Store: Writing to addresses {0, 3, 6, 9, 12}"
            << std::endl;
  std::cout << "   (stride = 3)" << std::endl;

  int stride = 3;
  int vector_length = 5;
  int base_address = 0;

  // Store with stride
  for (int i = 0; i < vector_length; ++i) {
    int address = base_address + i * stride;
    int value = (i + 1) * 100;

    std::cout << "   Writing " << value << " to address " << address
              << std::endl;

    auto store_req = std::make_shared<MemoryRequestPacket>(
        LSUOp::STORE, address, value, 1, 1, true);

    req_port->write(
        std::static_pointer_cast<Architecture::DataPacket>(store_req));
    valid_port->write(
        std::static_pointer_cast<Architecture::DataPacket>(valid_signal));

    for (int j = 0; j < 10; ++j) {
      scheduler.run(1);
    }
  }

  std::cout << "\n2. Strided Load: Reading from addresses {0, 3, 6, 9, 12}"
            << std::endl;

  // Load with stride
  for (int i = 0; i < vector_length; ++i) {
    int address = base_address + i * stride;

    auto load_req = std::make_shared<MemoryRequestPacket>(LSUOp::LOAD, address,
                                                          0, 1, 1, true);

    req_port->write(
        std::static_pointer_cast<Architecture::DataPacket>(load_req));
    valid_port->write(
        std::static_pointer_cast<Architecture::DataPacket>(valid_signal));

    for (int j = 0; j < 10; ++j) {
      scheduler.run(1);
    }

    auto response = resp_port->read();
    auto mem_resp = std::dynamic_pointer_cast<MemoryResponsePacket>(response);

    if (mem_resp && mem_resp->isSuccess()) {
      std::cout << "   Address[" << address << "] = " << mem_resp->getData()
                << std::endl;
    }
  }

  std::cout << "\n✓ Strided access completed!" << std::endl;
  std::cout << "\nStatistics:" << std::endl;
  std::cout << "  Operations completed: " << lsu->getOperationsCompleted()
            << std::endl;
  std::cout << "  Cycles stalled: " << lsu->getCyclesStalled() << std::endl;
}

void example4_bank_conflicts() {
  std::cout << "\n" << std::string(60, '=') << std::endl;
  std::cout << "Example 4: Bank Conflict Demonstration" << std::endl;
  std::cout << std::string(60, '=') << std::endl;

  EventDriven::EventScheduler scheduler;
  auto lsu = std::make_shared<LoadStoreUnit>("LSU", scheduler, 1, 8, 4, 64);
  lsu->setVerbose(true);
  lsu->start();

  auto req_port = lsu->getPort("req_in");
  auto valid_port = lsu->getPort("valid");
  auto valid_signal = std::make_shared<IntDataPacket>(1);

  std::cout << "\nWith 8 banks, addresses are mapped as: addr % 8 = bank_id"
            << std::endl;
  std::cout << "\n1. Sequential access (no conflicts): addresses {0, 1, 2, 3}"
            << std::endl;

  // Sequential access - different banks
  for (int i = 0; i < 4; ++i) {
    int address = i;
    std::cout << "   Address " << address << " -> Bank " << (address % 8)
              << std::endl;

    auto store_req = std::make_shared<MemoryRequestPacket>(
        LSUOp::STORE, address, i * 10, 1, 1, true);

    req_port->write(
        std::static_pointer_cast<Architecture::DataPacket>(store_req));
    valid_port->write(
        std::static_pointer_cast<Architecture::DataPacket>(valid_signal));

    for (int j = 0; j < 10; ++j) {
      scheduler.run(1);
    }
  }

  size_t stalls1 = lsu->getCyclesStalled();

  std::cout << "\n2. Conflicting access (same bank): addresses {0, 8, 16, 24}"
            << std::endl;

  // Conflicting access - same bank (bank 0)
  for (int i = 0; i < 4; ++i) {
    int address = i * 8;  // All map to bank 0
    std::cout << "   Address " << address << " -> Bank " << (address % 8)
              << std::endl;

    auto store_req = std::make_shared<MemoryRequestPacket>(
        LSUOp::STORE, address, i * 20, 1, 1, true);

    req_port->write(
        std::static_pointer_cast<Architecture::DataPacket>(store_req));
    valid_port->write(
        std::static_pointer_cast<Architecture::DataPacket>(valid_signal));

    for (int j = 0; j < 10; ++j) {
      scheduler.run(1);
    }
  }

  size_t stalls2 = lsu->getCyclesStalled() - stalls1;

  std::cout << "\nResults:" << std::endl;
  std::cout << "  Sequential access - Cycles stalled: " << stalls1 << std::endl;
  std::cout << "  Conflicting access - Cycles stalled: " << stalls2
            << std::endl;
  std::cout << "\n✓ Bank conflict demonstration completed!" << std::endl;
}

int main() {
  std::cout << "\n" << std::string(60, '=') << std::endl;
  std::cout << "LSU (Load-Store Unit) Component Examples" << std::endl;
  std::cout << std::string(60, '=') << std::endl;
  std::cout << "\nBased on vector processor MAU (Memory Access Unit)"
            << std::endl;
  std::cout << "Features:" << std::endl;
  std::cout << "  - Multiple memory banks (8 banks)" << std::endl;
  std::cout << "  - Load/Store operations" << std::endl;
  std::cout << "  - Vector operations with stride support" << std::endl;
  std::cout << "  - Bank conflict handling" << std::endl;
  std::cout << "  - Request queuing with back pressure" << std::endl;

  try {
    example1_basic_load_store();
    example2_vector_operations();
    example3_strided_access();
    example4_bank_conflicts();

    std::cout << "\n" << std::string(60, '=') << std::endl;
    std::cout << "All examples completed successfully! ✓" << std::endl;
    std::cout << std::string(60, '=') << std::endl;

    return 0;
  } catch (const std::exception& e) {
    std::cerr << "\nError: " << e.what() << std::endl;
    return 1;
  }
}
