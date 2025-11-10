#include <iostream>
#include <memory>

#include "../components/alu.h"
#include "../components/int_packet.h"
#include "../components/pe.h"
#include "../components/sink.h"
#include "../components/source.h"
#include "../ready_valid_connection.h"
#include "../scheduler.h"
#include "../tick_connection.h"

/**
 * @brief ALU instruction source component
 */
class ALUInstructionSource : public Architecture::TickingComponent {
 public:
  ALUInstructionSource(const std::string& name,
                       EventDriven::EventScheduler& scheduler, uint64_t period)
      : Architecture::TickingComponent(name, scheduler, period),
        current_index_(0) {
    auto out = std::make_shared<Architecture::Port>(
        "out", Architecture::PortDirection::OUTPUT, this);
    addPort(out);

    // Define test instructions
    instructions_.push_back({10, 20, ALUOp::ADD});      // 10 + 20 = 30
    instructions_.push_back({50, 15, ALUOp::SUB});      // 50 - 15 = 35
    instructions_.push_back({6, 7, ALUOp::MUL});        // 6 * 7 = 42
    instructions_.push_back({100, 4, ALUOp::DIV});      // 100 / 4 = 25
    instructions_.push_back({0xFF, 0x0F, ALUOp::AND});  // 255 & 15 = 15
  }

  void tick() override {
    auto out = getPort("out");

    if (current_index_ < instructions_.size()) {
      auto& inst = instructions_[current_index_];

      std::cout << "[" << scheduler_.getCurrentTime() << "] " << getName()
                << ": Sending instruction " << current_index_ << ": "
                << ALUComponent::getOpName(inst.op) << " " << inst.a << ", "
                << inst.b << std::endl;

      auto alu_packet =
          std::make_shared<ALUDataPacket>(inst.a, inst.b, inst.op);
      alu_packet->setTimestamp(scheduler_.getCurrentTime());
      out->write(
          std::static_pointer_cast<Architecture::DataPacket>(alu_packet));

      current_index_++;
    }

    if (current_index_ >= instructions_.size()) {
      stop();
    }
  }

 private:
  struct Instruction {
    int a;
    int b;
    ALUOp op;
  };

  std::vector<Instruction> instructions_;
  size_t current_index_;
};

/**
 * @brief PE instruction source component
 */
class PEInstructionSource : public Architecture::TickingComponent {
 public:
  PEInstructionSource(const std::string& name,
                      EventDriven::EventScheduler& scheduler, uint64_t period)
      : Architecture::TickingComponent(name, scheduler, period),
        current_index_(0) {
    auto out = std::make_shared<Architecture::Port>(
        "out", Architecture::PortDirection::OUTPUT, this);
    addPort(out);

    // Define test instructions (op, src_a, src_b, dst)
    // Assuming registers R0-R3 are initialized with values
    instructions_.push_back({ALUOp::ADD, 0, 1, 4});  // R4 = R0 + R1
    instructions_.push_back({ALUOp::MUL, 2, 3, 5});  // R5 = R2 * R3
    instructions_.push_back({ALUOp::SUB, 4, 5, 6});  // R6 = R4 - R5
    instructions_.push_back({ALUOp::MAX, 0, 2, 7});  // R7 = max(R0, R2)
  }

  void tick() override {
    auto out = getPort("out");

    if (current_index_ < instructions_.size()) {
      auto& inst = instructions_[current_index_];

      std::cout << "[" << scheduler_.getCurrentTime() << "] " << getName()
                << ": Sending instruction " << current_index_ << ": "
                << ALUComponent::getOpName(inst.op) << " R" << inst.src_a
                << ", R" << inst.src_b << " -> R" << inst.dst << std::endl;

      auto pe_packet = std::make_shared<PEInstructionPacket>(
          inst.op, inst.src_a, inst.src_b, inst.dst);
      pe_packet->setTimestamp(scheduler_.getCurrentTime());
      out->write(std::static_pointer_cast<Architecture::DataPacket>(pe_packet));

      current_index_++;
    }

    if (current_index_ >= instructions_.size()) {
      stop();
    }
  }

 private:
  struct Instruction {
    ALUOp op;
    int src_a;
    int src_b;
    int dst;
  };

  std::vector<Instruction> instructions_;
  size_t current_index_;
};

/**
 * @brief Example 1: ALU with standard connection
 */
void example1_alu_basic() {
  std::cout << "\n=== Example 1: ALU with Standard Connection ===" << std::endl;
  std::cout << "Testing ALU with various operations\n" << std::endl;

  EventDriven::EventScheduler scheduler;

  // Create components
  auto source =
      std::make_shared<ALUInstructionSource>("ALU_Source", scheduler, 10);
  auto alu = std::make_shared<ALUComponent>("ALU", scheduler, 10);
  auto sink = std::make_shared<DataSinkComponent>("Sink", scheduler, 10);

  alu->setVerbose(true);

  // Create connections
  auto conn1 = std::make_shared<Architecture::TickingConnection>(
      "source_to_alu", scheduler, 10);
  conn1->addSourcePort(source->getPort("out"));
  conn1->addDestinationPort(alu->getPort("in"));

  auto conn2 = std::make_shared<Architecture::TickingConnection>("alu_to_sink",
                                                                 scheduler, 10);
  conn2->addSourcePort(alu->getPort("out"));
  conn2->addDestinationPort(sink->getPort("in"));

  // Start components
  source->start(0);
  alu->start(0);
  sink->start(0);
  conn1->start(5);
  conn2->start(5);

  // Run simulation
  scheduler.run(200);

  // Print statistics
  alu->printStatistics();
}

/**
 * @brief Example 2: ALU with ready/valid connection (back pressure)
 */
void example2_alu_with_backpressure() {
  std::cout << "\n\n=== Example 2: ALU with Back Pressure ===" << std::endl;
  std::cout << "Testing ALU with ready/valid handshake protocol\n" << std::endl;

  EventDriven::EventScheduler scheduler;

  // Create components
  auto source =
      std::make_shared<ALUInstructionSource>("ALU_Source", scheduler, 10);
  auto alu = std::make_shared<ALUComponent>("ALU", scheduler, 10);
  auto sink = std::make_shared<DataSinkComponent>("Sink", scheduler, 10);

  alu->setVerbose(false);  // Less verbose for clarity

  // Create ready/valid connections with small buffer
  auto conn1 = std::make_shared<Architecture::ReadyValidConnection>(
      "source_to_alu", scheduler, 10, 2);  // Buffer size = 2
  conn1->addSourcePort(source->getPort("out"));
  conn1->addDestinationPort(alu->getPort("in"));
  conn1->setVerbose(true);

  auto conn2 = std::make_shared<Architecture::ReadyValidConnection>(
      "alu_to_sink", scheduler, 10, 2);
  conn2->addSourcePort(alu->getPort("out"));
  conn2->addDestinationPort(sink->getPort("in"));
  conn2->setVerbose(true);

  // Start components
  source->start(0);
  alu->start(0);
  sink->start(0);
  conn1->start(5);
  conn2->start(5);

  // Run simulation
  scheduler.run(250);

  // Print statistics
  std::cout << std::endl;
  alu->printStatistics();
  conn1->printStatistics();
  conn2->printStatistics();
}

/**
 * @brief Example 3: Processing Element (PE)
 */
void example3_processing_element() {
  std::cout << "\n\n=== Example 3: Processing Element ===" << std::endl;
  std::cout << "Testing PE with register file and instruction queue\n"
            << std::endl;

  EventDriven::EventScheduler scheduler;

  // Create PE
  auto pe = std::make_shared<ProcessingElement>("PE0", scheduler, 10, 32, 4);
  pe->setVerbose(true);

  // Initialize registers
  pe->initRegister(0, 10);  // R0 = 10
  pe->initRegister(1, 20);  // R1 = 20
  pe->initRegister(2, 5);   // R2 = 5
  pe->initRegister(3, 7);   // R3 = 7

  std::cout << "Initial register values:" << std::endl;
  std::cout << "R0 = 10, R1 = 20, R2 = 5, R3 = 7\n" << std::endl;

  // Create instruction source
  auto inst_source =
      std::make_shared<PEInstructionSource>("InstSource", scheduler, 10);
  auto sink = std::make_shared<DataSinkComponent>("Sink", scheduler, 10);

  // Create connections
  auto conn1 = std::make_shared<Architecture::TickingConnection>("source_to_pe",
                                                                 scheduler, 10);
  conn1->addSourcePort(inst_source->getPort("out"));
  conn1->addDestinationPort(pe->getPort("inst_in"));

  auto conn2 = std::make_shared<Architecture::TickingConnection>("pe_to_sink",
                                                                 scheduler, 10);
  conn2->addSourcePort(pe->getPort("data_out"));
  conn2->addDestinationPort(sink->getPort("in"));

  // Start components
  inst_source->start(0);
  pe->start(0);
  sink->start(0);
  conn1->start(5);
  conn2->start(5);

  // Run simulation
  scheduler.run(150);

  // Print results
  pe->printRegisters();
  pe->printStatistics();

  std::cout << "\nExpected results:" << std::endl;
  std::cout << "R4 = R0 + R1 = 10 + 20 = 30" << std::endl;
  std::cout << "R5 = R2 * R3 = 5 * 7 = 35" << std::endl;
  std::cout << "R6 = R4 - R5 = 30 - 35 = -5" << std::endl;
  std::cout << "R7 = max(R0, R2) = max(10, 5) = 10" << std::endl;
}

/**
 * @brief Example 4: PE with back pressure
 */
void example4_pe_with_backpressure() {
  std::cout << "\n\n=== Example 4: PE with Back Pressure ===" << std::endl;
  std::cout << "Testing PE with ready/valid handshake\n" << std::endl;

  EventDriven::EventScheduler scheduler;

  // Create PE with smaller queue to demonstrate back pressure
  auto pe = std::make_shared<ProcessingElement>("PE0", scheduler, 10, 32, 2);
  pe->setVerbose(true);

  // Initialize registers
  pe->initRegister(0, 100);
  pe->initRegister(1, 50);
  pe->initRegister(2, 25);
  pe->initRegister(3, 10);

  // Create instruction source
  auto inst_source =
      std::make_shared<PEInstructionSource>("InstSource", scheduler, 10);
  auto sink = std::make_shared<DataSinkComponent>("Sink", scheduler, 10);

  // Create ready/valid connections
  auto conn1 = std::make_shared<Architecture::ReadyValidConnection>(
      "source_to_pe", scheduler, 10, 1);  // Very small buffer
  conn1->addSourcePort(inst_source->getPort("out"));
  conn1->addDestinationPort(pe->getPort("inst_in"));
  conn1->setVerbose(true);

  auto conn2 = std::make_shared<Architecture::ReadyValidConnection>(
      "pe_to_sink", scheduler, 10, 2);
  conn2->addSourcePort(pe->getPort("data_out"));
  conn2->addDestinationPort(sink->getPort("in"));
  conn2->setVerbose(true);

  // Start components
  inst_source->start(0);
  pe->start(0);
  sink->start(0);
  conn1->start(5);
  conn2->start(5);

  // Run simulation
  scheduler.run(200);

  // Print results
  std::cout << std::endl;
  pe->printStatistics();
  conn1->printStatistics();
  conn2->printStatistics();
}

int main() {
  example1_alu_basic();
  example2_alu_with_backpressure();
  example3_processing_element();
  example4_pe_with_backpressure();
  return 0;
}
