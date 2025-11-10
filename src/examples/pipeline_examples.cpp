#include <iostream>
#include <memory>

#include "../components/pipeline.h"
#include "../components/sink.h"
#include "../components/source.h"
#include "../scheduler.h"
#include "../tick_connection.h"

/**
 * @brief Example: Pipeline usage demonstration
 *
 * Demonstrates a simple pipeline with multiple stages
 */
void pipelineExample() {
  std::cout << "\n=== Pipeline Example ===" << std::endl;
  std::cout << "Creating a 3-stage arithmetic pipeline" << std::endl;
  std::cout << "Pipeline stages: Stage0(×2) -> Stage1(+10) -> Stage2(>>1)"
            << std::endl;
  std::cout << "Formula: ((input × 2) + 10) >> 1\n" << std::endl;

  EventDriven::EventScheduler scheduler;

  // Create components
  auto source =
      std::make_shared<DataSourceComponent>("Source", scheduler, 10, 5);
  auto pipeline = std::make_shared<ArithmeticPipeline>("ArithPipeline",
                                                       scheduler, 10, 2, 10, 1);
  auto sink = std::make_shared<DataSinkComponent>("Sink", scheduler, 10);

  // Enable verbose output for pipeline
  pipeline->setVerbose(true);

  // Create connections
  auto conn1 = std::make_shared<Architecture::TickingConnection>(
      "source_to_pipeline", scheduler, 10);
  conn1->addSourcePort(source->getPort("out"));
  conn1->addDestinationPort(pipeline->getPort("in"));

  auto conn2 = std::make_shared<Architecture::TickingConnection>(
      "pipeline_to_sink", scheduler, 10);
  conn2->addSourcePort(pipeline->getPort("out"));
  conn2->addDestinationPort(sink->getPort("in"));

  // Start components
  source->start(0);
  pipeline->start(0);
  sink->start(0);

  // Start connections
  conn1->start(5);
  conn2->start(5);

  // Run simulation
  scheduler.run(100);

  // Print statistics
  pipeline->printStatistics();

  std::cout << "\n=== Expected Results ===" << std::endl;
  std::cout
      << "Input: 5  -> ((5 × 2) + 10) >> 1 = (10 + 10) >> 1 = 20 >> 1 = 10"
      << std::endl;
  std::cout
      << "Input: 6  -> ((6 × 2) + 10) >> 1 = (12 + 10) >> 1 = 22 >> 1 = 11"
      << std::endl;
  std::cout
      << "Input: 7  -> ((7 × 2) + 10) >> 1 = (14 + 10) >> 1 = 24 >> 1 = 12"
      << std::endl;
  std::cout
      << "Input: 8  -> ((8 × 2) + 10) >> 1 = (16 + 10) >> 1 = 26 >> 1 = 13"
      << std::endl;
  std::cout
      << "Input: 9  -> ((9 × 2) + 10) >> 1 = (18 + 10) >> 1 = 28 >> 1 = 14"
      << std::endl;
}

/**
 * @brief Example: Custom pipeline with user-defined stages
 */
void customPipelineExample() {
  std::cout << "\n\n=== Custom Pipeline Example ===" << std::endl;
  std::cout << "Creating a custom 4-stage pipeline" << std::endl;

  EventDriven::EventScheduler scheduler;

  // Create components
  auto source =
      std::make_shared<DataSourceComponent>("Source", scheduler, 10, 1);
  auto pipeline =
      std::make_shared<PipelineComponent>("CustomPipeline", scheduler, 10, 4);
  auto sink = std::make_shared<DataSinkComponent>("Sink", scheduler, 10);

  // Configure custom stage functions
  // Stage 0: Square the input
  pipeline->setStageFunction(
      0, [](std::shared_ptr<Architecture::DataPacket> data) {
        auto int_data = std::dynamic_pointer_cast<IntDataPacket>(data);
        if (int_data) {
          int val = int_data->getValue();
          int result = val * val;
          std::cout << "  [Stage 0] Square: " << val << "² = " << result
                    << std::endl;
          return std::static_pointer_cast<Architecture::DataPacket>(
              std::make_shared<IntDataPacket>(result));
        }
        return data;
      });

  // Stage 1: Add 5
  pipeline->setStageFunction(
      1, [](std::shared_ptr<Architecture::DataPacket> data) {
        auto int_data = std::dynamic_pointer_cast<IntDataPacket>(data);
        if (int_data) {
          int val = int_data->getValue();
          int result = val + 5;
          std::cout << "  [Stage 1] Add 5: " << val << " + 5 = " << result
                    << std::endl;
          return std::static_pointer_cast<Architecture::DataPacket>(
              std::make_shared<IntDataPacket>(result));
        }
        return data;
      });

  // Stage 2: Multiply by 3
  pipeline->setStageFunction(
      2, [](std::shared_ptr<Architecture::DataPacket> data) {
        auto int_data = std::dynamic_pointer_cast<IntDataPacket>(data);
        if (int_data) {
          int val = int_data->getValue();
          int result = val * 3;
          std::cout << "  [Stage 2] Multiply by 3: " << val
                    << " × 3 = " << result << std::endl;
          return std::static_pointer_cast<Architecture::DataPacket>(
              std::make_shared<IntDataPacket>(result));
        }
        return data;
      });

  // Stage 3: Modulo 100
  pipeline->setStageFunction(
      3, [](std::shared_ptr<Architecture::DataPacket> data) {
        auto int_data = std::dynamic_pointer_cast<IntDataPacket>(data);
        if (int_data) {
          int val = int_data->getValue();
          int result = val % 100;
          std::cout << "  [Stage 3] Modulo 100: " << val
                    << " % 100 = " << result << std::endl;
          return std::static_pointer_cast<Architecture::DataPacket>(
              std::make_shared<IntDataPacket>(result));
        }
        return data;
      });

  // Create connections
  auto conn1 = std::make_shared<Architecture::TickingConnection>(
      "source_to_pipeline", scheduler, 10);
  conn1->addSourcePort(source->getPort("out"));
  conn1->addDestinationPort(pipeline->getPort("in"));

  auto conn2 = std::make_shared<Architecture::TickingConnection>(
      "pipeline_to_sink", scheduler, 10);
  conn2->addSourcePort(pipeline->getPort("out"));
  conn2->addDestinationPort(sink->getPort("in"));

  // Start components
  source->start(0);
  pipeline->start(0);
  sink->start(0);

  // Start connections
  conn1->start(5);
  conn2->start(5);

  // Run simulation
  scheduler.run(150);

  // Print statistics
  pipeline->printStatistics();
}

int main() {
  pipelineExample();
  customPipelineExample();
  return 0;
}
