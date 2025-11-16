#include <gtest/gtest.h>

#include "../src/comp/pipeline.h"
#include "../src/packet.h"
#include "../src/scheduler.h"
#include "../src/trace.h"

class PipelineTest : public ::testing::Test {
 protected:
  void SetUp() override {
    scheduler = std::make_unique<EventDriven::EventScheduler>();
  }

  void TearDown() override { scheduler.reset(); }

  std::unique_ptr<EventDriven::EventScheduler> scheduler;
};

TEST_F(PipelineTest, BasicCreation) {
  auto pipeline =
      std::make_shared<PipelineComponent>("test_pipeline", *scheduler, 1, 3);
  pipeline->start();

  EXPECT_EQ(pipeline->getNumStages(), 3);
  EXPECT_EQ(pipeline->getOccupancy(), 0);
}

TEST_F(PipelineTest, SingleStagePassthrough) {
  auto pipeline =
      std::make_shared<PipelineComponent>("test_pipeline", *scheduler, 1, 1);
  pipeline->start();

  auto input_port = pipeline->getPort("in");
  auto output_port = pipeline->getPort("out");

  // Send data through pipeline
  auto packet = std::make_shared<Architecture::IntDataPacket>(42);
  input_port->write(packet);

  // Execute two ticks (one to load, one to output)
  pipeline->tick();
  pipeline->tick();

  // Check output
  ASSERT_TRUE(output_port->hasData());
  auto result = std::dynamic_pointer_cast<Architecture::IntDataPacket>(
      output_port->read());
  ASSERT_NE(result, nullptr);
  EXPECT_EQ(result->getValue(), 42);
}

TEST_F(PipelineTest, MultiStagePassthrough) {
  auto pipeline =
      std::make_shared<PipelineComponent>("test_pipeline", *scheduler, 1, 3);
  pipeline->start();

  auto input_port = pipeline->getPort("in");
  auto output_port = pipeline->getPort("out");

  // Send data through pipeline
  auto packet = std::make_shared<Architecture::IntDataPacket>(100);
  input_port->write(packet);

  // Execute 4 ticks (3 stages + 1 to output)
  for (int i = 0; i < 4; i++) {
    pipeline->tick();
  }

  // Check output
  ASSERT_TRUE(output_port->hasData());
  auto result = std::dynamic_pointer_cast<Architecture::IntDataPacket>(
      output_port->read());
  ASSERT_NE(result, nullptr);
  EXPECT_EQ(result->getValue(), 100);
}

TEST_F(PipelineTest, CustomStageFunction) {
  auto pipeline =
      std::make_shared<PipelineComponent>("test_pipeline", *scheduler, 1, 2);
  pipeline->start();

  // Stage 0: Double the value
  pipeline->setStageFunction(
      0,
      [](std::shared_ptr<Architecture::DataPacket> data)
          -> std::shared_ptr<Architecture::DataPacket> {
        auto int_data =
            std::dynamic_pointer_cast<Architecture::IntDataPacket>(data);
        if (int_data) {
          int value = int_data->getValue();
          return std::make_shared<Architecture::IntDataPacket>(value * 2);
        }
        return data;
      });

  // Stage 1: Add 10
  pipeline->setStageFunction(
      1,
      [](std::shared_ptr<Architecture::DataPacket> data)
          -> std::shared_ptr<Architecture::DataPacket> {
        auto int_data =
            std::dynamic_pointer_cast<Architecture::IntDataPacket>(data);
        if (int_data) {
          int value = int_data->getValue();
          return std::make_shared<Architecture::IntDataPacket>(value + 10);
        }
        return data;
      });

  auto input_port = pipeline->getPort("in");
  auto output_port = pipeline->getPort("out");

  // Input: 5, Stage 0: 5*2=10, Stage 1: 10+10=20
  auto packet = std::make_shared<Architecture::IntDataPacket>(5);
  input_port->write(packet);

  // Execute 3 ticks (2 stages + 1 to output)
  for (int i = 0; i < 3; i++) {
    pipeline->tick();
  }

  // Check output
  ASSERT_TRUE(output_port->hasData());
  auto result = std::dynamic_pointer_cast<Architecture::IntDataPacket>(
      output_port->read());
  ASSERT_NE(result, nullptr);
  EXPECT_EQ(result->getValue(), 20);
}

TEST_F(PipelineTest, PipelineDepth) {
  auto pipeline =
      std::make_shared<PipelineComponent>("test_pipeline", *scheduler, 1, 3);
  pipeline->start();

  auto input_port = pipeline->getPort("in");

  EXPECT_EQ(pipeline->getOccupancy(), 0);

  // Add first packet
  auto packet1 = std::make_shared<Architecture::IntDataPacket>(1);
  input_port->write(packet1);
  pipeline->tick();
  EXPECT_EQ(pipeline->getOccupancy(), 1);

  // Add second packet
  auto packet2 = std::make_shared<Architecture::IntDataPacket>(2);
  input_port->write(packet2);
  pipeline->tick();
  EXPECT_EQ(pipeline->getOccupancy(), 2);

  // Add third packet
  auto packet3 = std::make_shared<Architecture::IntDataPacket>(3);
  input_port->write(packet3);
  pipeline->tick();
  EXPECT_EQ(pipeline->getOccupancy(), 3);
}

TEST_F(PipelineTest, PipelineFull) {
  auto pipeline =
      std::make_shared<PipelineComponent>("test_pipeline", *scheduler, 1, 2);
  pipeline->start();

  auto input_port = pipeline->getPort("in");

  // Fill pipeline
  input_port->write(std::make_shared<Architecture::IntDataPacket>(1));
  pipeline->tick();

  input_port->write(std::make_shared<Architecture::IntDataPacket>(2));
  pipeline->tick();

  EXPECT_TRUE(pipeline->isFull());
}

TEST_F(PipelineTest, PipelineEmpty) {
  auto pipeline =
      std::make_shared<PipelineComponent>("test_pipeline", *scheduler, 1, 3);
  pipeline->start();

  EXPECT_TRUE(pipeline->isEmpty());

  auto input_port = pipeline->getPort("in");
  input_port->write(std::make_shared<Architecture::IntDataPacket>(1));
  pipeline->tick();

  EXPECT_FALSE(pipeline->isEmpty());
}

TEST_F(PipelineTest, PipelineFlush) {
  auto pipeline =
      std::make_shared<PipelineComponent>("test_pipeline", *scheduler, 1, 3);
  pipeline->start();

  auto input_port = pipeline->getPort("in");

  // Fill pipeline with data
  input_port->write(std::make_shared<Architecture::IntDataPacket>(1));
  pipeline->tick();

  input_port->write(std::make_shared<Architecture::IntDataPacket>(2));
  pipeline->tick();

  EXPECT_FALSE(pipeline->isEmpty());

  // Flush pipeline
  pipeline->flush();
  EXPECT_TRUE(pipeline->isEmpty());
  EXPECT_EQ(pipeline->getOccupancy(), 0);
}

TEST_F(PipelineTest, StallControl) {
  auto pipeline =
      std::make_shared<PipelineComponent>("test_pipeline", *scheduler, 1, 2);
  pipeline->start();

  auto input_port = pipeline->getPort("in");
  auto stall_port = pipeline->getPort("stall");
  auto output_port = pipeline->getPort("out");

  // Send data
  auto packet = std::make_shared<Architecture::IntDataPacket>(42);
  input_port->write(packet);

  // First tick: load data into stage 0
  pipeline->tick();

  // Set stall signal
  auto stall_signal = std::make_shared<Architecture::IntDataPacket>(1);
  stall_port->write(stall_signal);

  // Tick with stall - data should not advance
  pipeline->tick();

  // Pipeline should be stalled, data should not advance to output
  EXPECT_FALSE(output_port->hasData());

  // Clear stall and continue
  auto no_stall = std::make_shared<Architecture::IntDataPacket>(0);
  stall_port->write(no_stall);

  pipeline->tick();
  pipeline->tick();  // One more tick to output from last stage

  // Now output should have data
  ASSERT_TRUE(output_port->hasData());
}

TEST_F(PipelineTest, MultipleDataThroughput) {
  auto pipeline =
      std::make_shared<PipelineComponent>("test_pipeline", *scheduler, 1, 3);
  pipeline->start();

  auto input_port = pipeline->getPort("in");
  auto output_port = pipeline->getPort("out");

  // Send 5 packets through pipeline
  std::vector<int> input_values = {10, 20, 30, 40, 50};
  std::vector<int> output_values;

  for (int val : input_values) {
    input_port->write(std::make_shared<Architecture::IntDataPacket>(val));
    pipeline->tick();
    // Collect output if available
    if (output_port->hasData()) {
      auto result = std::dynamic_pointer_cast<Architecture::IntDataPacket>(
          output_port->read());
      if (result) {
        output_values.push_back(result->getValue());
      }
    }
  }

  // Continue ticking to flush remaining data
  for (int i = 0; i < 4; i++) {
    pipeline->tick();
    if (output_port->hasData()) {
      auto result = std::dynamic_pointer_cast<Architecture::IntDataPacket>(
          output_port->read());
      if (result) {
        output_values.push_back(result->getValue());
      }
    }
  }

  // Verify all values were output in correct order
  ASSERT_EQ(output_values.size(), input_values.size());
  for (size_t i = 0; i < input_values.size(); i++) {
    EXPECT_EQ(output_values[i], input_values[i]) << "Mismatch at index " << i;
  }
}

TEST_F(PipelineTest, StageChaining) {
  auto pipeline =
      std::make_shared<PipelineComponent>("test_pipeline", *scheduler, 1, 4);
  pipeline->start();

  // Create a chain: input -> *2 -> +5 -> *3 -> -1
  pipeline->setStageFunction(
      0,
      [](std::shared_ptr<Architecture::DataPacket> data)
          -> std::shared_ptr<Architecture::DataPacket> {
        auto int_data =
            std::dynamic_pointer_cast<Architecture::IntDataPacket>(data);
        return int_data ? std::make_shared<Architecture::IntDataPacket>(
                              int_data->getValue() * 2)
                        : data;
      });

  pipeline->setStageFunction(
      1,
      [](std::shared_ptr<Architecture::DataPacket> data)
          -> std::shared_ptr<Architecture::DataPacket> {
        auto int_data =
            std::dynamic_pointer_cast<Architecture::IntDataPacket>(data);
        return int_data ? std::make_shared<Architecture::IntDataPacket>(
                              int_data->getValue() + 5)
                        : data;
      });

  pipeline->setStageFunction(
      2,
      [](std::shared_ptr<Architecture::DataPacket> data)
          -> std::shared_ptr<Architecture::DataPacket> {
        auto int_data =
            std::dynamic_pointer_cast<Architecture::IntDataPacket>(data);
        return int_data ? std::make_shared<Architecture::IntDataPacket>(
                              int_data->getValue() * 3)
                        : data;
      });

  pipeline->setStageFunction(
      3,
      [](std::shared_ptr<Architecture::DataPacket> data)
          -> std::shared_ptr<Architecture::DataPacket> {
        auto int_data =
            std::dynamic_pointer_cast<Architecture::IntDataPacket>(data);
        return int_data ? std::make_shared<Architecture::IntDataPacket>(
                              int_data->getValue() - 1)
                        : data;
      });

  auto input_port = pipeline->getPort("in");
  auto output_port = pipeline->getPort("out");

  // Input: 3, Expected: ((3*2+5)*3-1) = ((6+5)*3-1) = (11*3-1) = 33-1 = 32
  input_port->write(std::make_shared<Architecture::IntDataPacket>(3));

  // Execute 5 ticks (4 stages + 1 to output)
  for (int i = 0; i < 5; i++) {
    pipeline->tick();
  }

  ASSERT_TRUE(output_port->hasData());
  auto result = std::dynamic_pointer_cast<Architecture::IntDataPacket>(
      output_port->read());
  ASSERT_NE(result, nullptr);
  EXPECT_EQ(result->getValue(), 32);
}

// Event-driven execution mode tests
TEST_F(PipelineTest, EventDrivenPipelineFlow) {
  EventDriven::Tracer::getInstance().initialize(
      "test_pipeline_event_driven.log", true);

  auto pipeline =
      std::make_shared<PipelineComponent>("test_pipe_ed", *scheduler, 2, 3);
  pipeline->start();

  auto input_port = pipeline->getPort("in");
  auto output_port = pipeline->getPort("out");

  std::vector<int> outputs;

  // Schedule input data at different times
  // With period=2 and 3 stages, ticks at t=0,2,4,6,8,10,...
  // Write between ticks to ensure they're seen
  scheduler->scheduleAt(1, [&](EventDriven::EventScheduler& sched) {
    auto packet = std::make_shared<Architecture::IntDataPacket>(100);
    input_port->write(packet);
    EventDriven::Tracer::getInstance().traceEvent(sched.getCurrentTime(),
                                                  "Test", "Input", "Data=100");
  });

  scheduler->scheduleAt(11, [&](EventDriven::EventScheduler& sched) {
    auto packet = std::make_shared<Architecture::IntDataPacket>(200);
    input_port->write(packet);
    EventDriven::Tracer::getInstance().traceEvent(sched.getCurrentTime(),
                                                  "Test", "Input", "Data=200");
  });

  // Collect outputs (3 stages * period 2 = 6 time units latency)
  // First output at ~t=7, second at ~t=17
  scheduler->scheduleAt(9, [&](EventDriven::EventScheduler&) {
    if (output_port->hasData()) {
      auto result = std::dynamic_pointer_cast<Architecture::IntDataPacket>(
          output_port->read());
      if (result) outputs.push_back(result->getValue());
    }
  });

  scheduler->scheduleAt(19, [&](EventDriven::EventScheduler&) {
    if (output_port->hasData()) {
      auto result = std::dynamic_pointer_cast<Architecture::IntDataPacket>(
          output_port->read());
      if (result) outputs.push_back(result->getValue());
    }
  });

  // Run simulation
  scheduler->run(25);

  // Collect any remaining outputs
  while (output_port->hasData()) {
    auto result = std::dynamic_pointer_cast<Architecture::IntDataPacket>(
        output_port->read());
    if (result) {
      outputs.push_back(result->getValue());
    }
  }

  // Verify outputs
  ASSERT_EQ(outputs.size(), 2);
  EXPECT_EQ(outputs[0], 100);
  EXPECT_EQ(outputs[1], 200);

  std::cout << "\n=== Event-Driven Pipeline Flow ===" << std::endl;
  std::cout << "Outputs collected: " << outputs.size() << std::endl;
  std::cout << "Final time: " << scheduler->getCurrentTime() << std::endl;

  EventDriven::Tracer::getInstance().dump();
  EventDriven::Tracer::getInstance().setEnabled(false);
}

TEST_F(PipelineTest, EventDrivenStageProcessing) {
  EventDriven::Tracer::getInstance().initialize(
      "test_pipeline_stage_processing.log", true);

  auto pipeline =
      std::make_shared<PipelineComponent>("test_pipe_stages", *scheduler, 1, 2);
  pipeline->start();

  // Set stage functions with logging
  pipeline->setStageFunction(
      0,
      [](std::shared_ptr<Architecture::DataPacket> data)
          -> std::shared_ptr<Architecture::DataPacket> {
        auto int_data =
            std::dynamic_pointer_cast<Architecture::IntDataPacket>(data);
        if (int_data) {
          int value = int_data->getValue() * 2;
          return std::make_shared<Architecture::IntDataPacket>(value);
        }
        return data;
      });

  pipeline->setStageFunction(
      1,
      [](std::shared_ptr<Architecture::DataPacket> data)
          -> std::shared_ptr<Architecture::DataPacket> {
        auto int_data =
            std::dynamic_pointer_cast<Architecture::IntDataPacket>(data);
        if (int_data) {
          int value = int_data->getValue() + 10;
          return std::make_shared<Architecture::IntDataPacket>(value);
        }
        return data;
      });

  auto input_port = pipeline->getPort("in");
  auto output_port = pipeline->getPort("out");

  // Schedule input: 5 -> Stage0(*2)=10 -> Stage1(+10)=20
  scheduler->scheduleAt(0, [&](EventDriven::EventScheduler& sched) {
    auto packet = std::make_shared<Architecture::IntDataPacket>(5);
    input_port->write(packet);
    EventDriven::Tracer::getInstance().traceCompute(
        sched.getCurrentTime(), "Test", "InputStage", "Value=5");
  });

  // Run simulation
  scheduler->run(10);

  // Verify output
  ASSERT_TRUE(output_port->hasData());
  auto result = std::dynamic_pointer_cast<Architecture::IntDataPacket>(
      output_port->read());
  ASSERT_NE(result, nullptr);
  EXPECT_EQ(result->getValue(), 20);  // (5*2)+10 = 20

  std::cout << "\n=== Event-Driven Stage Processing ===" << std::endl;
  std::cout << "Input: 5, Expected: 20, Got: " << result->getValue()
            << std::endl;
  std::cout << "Final time: " << scheduler->getCurrentTime() << std::endl;

  EventDriven::Tracer::getInstance().dump();
  EventDriven::Tracer::getInstance().setEnabled(false);
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
