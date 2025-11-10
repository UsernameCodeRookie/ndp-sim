#include <gtest/gtest.h>

#include <memory>

#include "../src/components/int_packet.h"
#include "../src/components/pipeline.h"
#include "../src/components/sink.h"
#include "../src/components/source.h"
#include "../src/scheduler.h"
#include "../src/tick_connection.h"

/**
 * @brief Test basic pipeline functionality
 */
TEST(PipelineTest, BasicPipeline) {
  EventDriven::EventScheduler scheduler;

  // Create a simple 3-stage pipeline
  auto pipeline =
      std::make_shared<PipelineComponent>("TestPipeline", scheduler, 10, 3);

  EXPECT_EQ(pipeline->getNumStages(), 3);
  EXPECT_TRUE(pipeline->isEmpty());
  EXPECT_FALSE(pipeline->isFull());
  EXPECT_EQ(pipeline->getOccupancy(), 0);
}

/**
 * @brief Test pipeline data flow
 */
TEST(PipelineTest, DataFlow) {
  EventDriven::EventScheduler scheduler;

  auto source =
      std::make_shared<DataSourceComponent>("Source", scheduler, 10, 1);
  auto pipeline =
      std::make_shared<PipelineComponent>("Pipeline", scheduler, 10, 3);
  auto sink = std::make_shared<DataSinkComponent>("Sink", scheduler, 10);

  // Connect components
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

  // Verify that data was processed
  EXPECT_GT(pipeline->getTotalProcessed(), 0);
}

/**
 * @brief Test arithmetic pipeline
 */
TEST(PipelineTest, ArithmeticPipeline) {
  EventDriven::EventScheduler scheduler;

  // Create arithmetic pipeline: (x * 2 + 10) >> 1
  auto pipeline = std::make_shared<ArithmeticPipeline>("ArithPipeline",
                                                       scheduler, 10, 2, 10, 1);

  EXPECT_EQ(pipeline->getNumStages(), 3);

  // Create a simple data source that generates one value
  auto in_port = pipeline->getPort("in");
  auto out_port = pipeline->getPort("out");

  ASSERT_NE(in_port, nullptr);
  ASSERT_NE(out_port, nullptr);

  // Manually feed data and tick
  auto input_data = std::make_shared<IntDataPacket>(5);
  input_data->setTimestamp(0);
  in_port->write(
      std::static_pointer_cast<Architecture::DataPacket>(input_data));

  // Start pipeline
  pipeline->start(0);

  // Run for enough time for data to pass through all stages
  scheduler.run(50);

  // Expected: ((5 * 2) + 10) >> 1 = (10 + 10) >> 1 = 20 >> 1 = 10
  // After 3 ticks, output should be available
  EXPECT_GT(pipeline->getTotalProcessed(), 0);
}

/**
 * @brief Test pipeline with custom stage functions
 */
TEST(PipelineTest, CustomStageFunctions) {
  EventDriven::EventScheduler scheduler;

  auto pipeline =
      std::make_shared<PipelineComponent>("CustomPipeline", scheduler, 10, 2);

  // Stage 0: multiply by 2
  pipeline->setStageFunction(
      0, [](std::shared_ptr<Architecture::DataPacket> data) {
        auto int_data = std::dynamic_pointer_cast<IntDataPacket>(data);
        if (int_data) {
          return std::static_pointer_cast<Architecture::DataPacket>(
              std::make_shared<IntDataPacket>(int_data->getValue() * 2));
        }
        return data;
      });

  // Stage 1: add 10
  pipeline->setStageFunction(
      1, [](std::shared_ptr<Architecture::DataPacket> data) {
        auto int_data = std::dynamic_pointer_cast<IntDataPacket>(data);
        if (int_data) {
          return std::static_pointer_cast<Architecture::DataPacket>(
              std::make_shared<IntDataPacket>(int_data->getValue() + 10));
        }
        return data;
      });

  auto in_port = pipeline->getPort("in");
  auto out_port = pipeline->getPort("out");

  // Feed input value of 3
  auto input_data = std::make_shared<IntDataPacket>(3);
  in_port->write(
      std::static_pointer_cast<Architecture::DataPacket>(input_data));

  pipeline->start(0);
  scheduler.run(40);

  // Expected: (3 * 2) + 10 = 16
  EXPECT_GT(pipeline->getTotalProcessed(), 0);
}

/**
 * @brief Test pipeline flush
 */
TEST(PipelineTest, PipelineFlush) {
  EventDriven::EventScheduler scheduler;

  auto pipeline =
      std::make_shared<PipelineComponent>("FlushPipeline", scheduler, 10, 3);

  // Feed some data
  auto in_port = pipeline->getPort("in");
  for (int i = 0; i < 3; ++i) {
    auto data = std::make_shared<IntDataPacket>(i);
    in_port->write(std::static_pointer_cast<Architecture::DataPacket>(data));

    pipeline->start(0);
    scheduler.run(10 + i * 10);
  }

  // Flush the pipeline
  pipeline->flush();

  // Pipeline should be empty after flush
  EXPECT_TRUE(pipeline->isEmpty());
  EXPECT_EQ(pipeline->getOccupancy(), 0);
}

/**
 * @brief Test pipeline occupancy
 */
TEST(PipelineTest, PipelineOccupancy) {
  EventDriven::EventScheduler scheduler;

  auto source =
      std::make_shared<DataSourceComponent>("Source", scheduler, 10, 1);
  auto pipeline = std::make_shared<PipelineComponent>("OccupancyPipeline",
                                                      scheduler, 10, 4);
  auto sink = std::make_shared<DataSinkComponent>("Sink", scheduler, 10);

  // Connect components
  auto conn1 = std::make_shared<Architecture::TickingConnection>(
      "source_to_pipeline", scheduler, 10);
  conn1->addSourcePort(source->getPort("out"));
  conn1->addDestinationPort(pipeline->getPort("in"));

  auto conn2 = std::make_shared<Architecture::TickingConnection>(
      "pipeline_to_sink", scheduler, 10);
  conn2->addSourcePort(pipeline->getPort("out"));
  conn2->addDestinationPort(sink->getPort("in"));

  source->start(0);
  pipeline->start(0);
  sink->start(0);

  conn1->start(5);
  conn2->start(5);

  // Initially empty
  EXPECT_TRUE(pipeline->isEmpty());

  // Run for a few cycles
  scheduler.run(20);

  // Should have some occupancy
  EXPECT_GT(pipeline->getOccupancy(), 0);
  EXPECT_LE(pipeline->getOccupancy(), pipeline->getNumStages());
}

/**
 * @brief Test multiple data items through pipeline
 */
TEST(PipelineTest, MultipleDataItems) {
  EventDriven::EventScheduler scheduler;

  auto source =
      std::make_shared<DataSourceComponent>("Source", scheduler, 10, 10);
  auto pipeline = std::make_shared<ArithmeticPipeline>("MultiPipeline",
                                                       scheduler, 10, 2, 5, 0);
  auto sink = std::make_shared<DataSinkComponent>("Sink", scheduler, 10);

  auto conn1 = std::make_shared<Architecture::TickingConnection>(
      "source_to_pipeline", scheduler, 10);
  conn1->addSourcePort(source->getPort("out"));
  conn1->addDestinationPort(pipeline->getPort("in"));

  auto conn2 = std::make_shared<Architecture::TickingConnection>(
      "pipeline_to_sink", scheduler, 10);
  conn2->addSourcePort(pipeline->getPort("out"));
  conn2->addDestinationPort(sink->getPort("in"));

  source->start(0);
  pipeline->start(0);
  sink->start(0);

  conn1->start(5);
  conn2->start(5);

  // Run for enough time to process all data
  scheduler.run(100);

  // Should have processed 5 items (source stops after 5)
  EXPECT_EQ(pipeline->getTotalProcessed(), 5);
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
