#include <gtest/gtest.h>

#include <cmath>
#include <cstdlib>

#include "../src/components/tpu.h"
#include "../src/operators/gemm.h"
#include "../src/scheduler.h"
#include "../src/trace.h"

using namespace Operators;
using FloatGEMM = GEMMOperator<float, Float32PrecisionTraits>;
using FloatTensor = Tensor<float>;

// Test fixture for GEMM operator tests
class GEMMOperatorTest : public ::testing::Test {
 protected:
  void SetUp() override {
    verbose_ = (std::getenv("GEMM_TEST_VERBOSE") != nullptr);
    tpu_ = std::make_shared<SystolicArrayTPU<Float32PrecisionTraits>>(
        "TestTPU", scheduler_, 1, 4);
    tpu_->start();
  }

  void TearDown() override { tpu_.reset(); }

  EventDriven::EventScheduler scheduler_;
  std::shared_ptr<SystolicArrayTPU<Float32PrecisionTraits>> tpu_;
  static constexpr float kTolerance = 1e-3f;

  void configureVerbose(FloatGEMM& op) const { op.setVerbose(verbose_); }

  bool verbose_ = false;
};

// Test small GEMM (2x2)
TEST_F(GEMMOperatorTest, SmallGEMM_2x2) {
  FloatGEMM gemm("GEMM_2x2");
  configureVerbose(gemm);
  gemm.bindTPU(tpu_);

  FloatTensor A(TensorShape{2, 2});
  FloatTensor B(TensorShape{2, 2});

  A.at(0, 0) = 1.0f;
  A.at(0, 1) = 2.0f;
  A.at(1, 0) = 3.0f;
  A.at(1, 1) = 4.0f;

  B.at(0, 0) = 5.0f;
  B.at(0, 1) = 6.0f;
  B.at(1, 0) = 7.0f;
  B.at(1, 1) = 8.0f;

  gemm.setInputs(A, B);
  gemm.compute();

  const FloatTensor& C = gemm.getOutput();

  EXPECT_NEAR(C.at(0, 0), 19.0f, kTolerance);
  EXPECT_NEAR(C.at(0, 1), 22.0f, kTolerance);
  EXPECT_NEAR(C.at(1, 0), 43.0f, kTolerance);
  EXPECT_NEAR(C.at(1, 1), 50.0f, kTolerance);

  EXPECT_TRUE(verifyGEMM(A, B, C, false));
}

// Test identity matrix multiplication
TEST_F(GEMMOperatorTest, IdentityMultiplication) {
  FloatGEMM gemm("GEMM_Identity");
  configureVerbose(gemm);
  gemm.bindTPU(tpu_);

  FloatTensor A(TensorShape{4, 4});
  FloatTensor B(TensorShape{4, 4});

  for (size_t i = 0; i < 4; ++i) {
    for (size_t j = 0; j < 4; ++j) {
      A.at(i, j) = (i == j) ? 1.0f : 0.0f;
    }
  }

  for (size_t i = 0; i < 4; ++i) {
    for (size_t j = 0; j < 4; ++j) {
      B.at(i, j) = static_cast<float>(i * 4 + j + 1);
    }
  }

  gemm.setInputs(A, B);
  gemm.compute();

  const FloatTensor& C = gemm.getOutput();

  for (size_t i = 0; i < 4; ++i) {
    for (size_t j = 0; j < 4; ++j) {
      EXPECT_NEAR(C.at(i, j), B.at(i, j), kTolerance);
    }
  }

  EXPECT_TRUE(verifyGEMM(A, B, C, false));
}

// Test GEMM with zero matrix
TEST_F(GEMMOperatorTest, ZeroMatrix) {
  FloatGEMM gemm("GEMM_Zero");
  configureVerbose(gemm);
  gemm.bindTPU(tpu_);

  FloatTensor A(TensorShape{3, 3}, 0.0f);
  FloatTensor B(TensorShape{3, 3}, 5.0f);

  gemm.setInputs(A, B);
  gemm.compute();

  const FloatTensor& C = gemm.getOutput();

  for (size_t i = 0; i < 3; ++i) {
    for (size_t j = 0; j < 3; ++j) {
      EXPECT_NEAR(C.at(i, j), 0.0f, kTolerance);
    }
  }

  EXPECT_TRUE(verifyGEMM(A, B, C, false));
}

// Test 64x64 GEMM (naive)
TEST_F(GEMMOperatorTest, GEMM_64x64_Naive) {
  FloatTensor A(TensorShape{64, 64});
  FloatTensor B(TensorShape{64, 64});

  // Initialize with known pattern
  for (size_t i = 0; i < 64; ++i) {
    for (size_t j = 0; j < 64; ++j) {
      A.at(i, j) = (i == j) ? 1.0f : 0.0f;  // Identity
      B.at(i, j) = static_cast<float>(i + j);
    }
  }

  FloatGEMM gemm_tpu("GEMM_64x64_TPU");
  configureVerbose(gemm_tpu);
  gemm_tpu.bindTPU(tpu_);
  gemm_tpu.setInputs(A, B);
  gemm_tpu.compute();
  const FloatTensor& C_tpu = gemm_tpu.getOutput();

  FloatGEMM gemm_cpu("GEMM_64x64_CPU");
  configureVerbose(gemm_cpu);
  gemm_cpu.setInputs(A, B);
  gemm_cpu.disableTiling();
  gemm_cpu.compute();
  const FloatTensor& C_cpu = gemm_cpu.getOutput();

  for (size_t i = 0; i < 64; ++i) {
    for (size_t j = 0; j < 64; ++j) {
      EXPECT_NEAR(C_tpu.at(i, j), C_cpu.at(i, j), kTolerance);
    }
  }

  EXPECT_TRUE(verifyGEMM(A, B, C_cpu, false));
}

// Test 64x64 GEMM with 16x16 tiling
TEST_F(GEMMOperatorTest, GEMM_64x64_Tiled_16x16) {
  FloatGEMM gemm("GEMM_64x64_Tiled_16");
  configureVerbose(gemm);

  FloatTensor A(TensorShape{64, 64});
  FloatTensor B(TensorShape{64, 64});

  // Initialize with known pattern
  for (size_t i = 0; i < 64; ++i) {
    for (size_t j = 0; j < 64; ++j) {
      A.at(i, j) = (i == j) ? 1.0f : 0.0f;  // Identity
      B.at(i, j) = static_cast<float>(i + j);
    }
  }

  gemm.setInputs(A, B);
  gemm.enableTiling(TileConfig(16, 16, 16));
  gemm.compute();

  const FloatTensor& C = gemm.getOutput();

  EXPECT_TRUE(verifyGEMM(A, B, C, false));
}

// Test 64x64 GEMM with 8x8 tiling
TEST_F(GEMMOperatorTest, GEMM_64x64_Tiled_8x8) {
  FloatGEMM gemm("GEMM_64x64_Tiled_8");
  configureVerbose(gemm);

  FloatTensor A(TensorShape{64, 64});
  FloatTensor B(TensorShape{64, 64});

  // Random initialization
  srand(123);
  A.fillRandom(0, 10);
  B.fillRandom(0, 10);

  gemm.setInputs(A, B);
  gemm.enableTiling(TileConfig(8, 8, 8));
  gemm.compute();

  const FloatTensor& C = gemm.getOutput();

  EXPECT_TRUE(verifyGEMM(A, B, C, false));
}

// Test 64x64 GEMM with 32x32 tiling
TEST_F(GEMMOperatorTest, GEMM_64x64_Tiled_32x32) {
  FloatGEMM gemm("GEMM_64x64_Tiled_32");
  configureVerbose(gemm);

  FloatTensor A(TensorShape{64, 64});
  FloatTensor B(TensorShape{64, 64});

  // Random initialization
  srand(456);
  A.fillRandom(-5, 5);
  B.fillRandom(-5, 5);

  gemm.setInputs(A, B);
  gemm.enableTiling(TileConfig(32, 32, 32));
  gemm.compute();

  const FloatTensor& C = gemm.getOutput();

  EXPECT_TRUE(verifyGEMM(A, B, C, false));
}

// Test non-square GEMM
TEST_F(GEMMOperatorTest, NonSquareGEMM) {
  FloatGEMM gemm("GEMM_NonSquare");
  configureVerbose(gemm);

  FloatTensor A(TensorShape{64, 32});  // 64x32
  FloatTensor B(TensorShape{32, 64});  // 32x64

  A.fillRandom(0, 5);
  B.fillRandom(0, 5);

  gemm.setInputs(A, B);
  gemm.enableTiling(TileConfig(16, 16, 16));
  gemm.compute();

  const FloatTensor& C = gemm.getOutput();

  EXPECT_EQ(C.shape()[0], 64);
  EXPECT_EQ(C.shape()[1], 64);
  EXPECT_TRUE(verifyGEMM(A, B, C, false));
}

// Test error handling: dimension mismatch
TEST_F(GEMMOperatorTest, DimensionMismatch) {
  FloatGEMM gemm("GEMM_Error");
  configureVerbose(gemm);

  FloatTensor A(TensorShape{4, 5});
  FloatTensor B(TensorShape{3, 4});  // Wrong K dimension

  EXPECT_THROW(gemm.setInputs(A, B), std::runtime_error);
}

// Test error handling: 1D tensor
TEST_F(GEMMOperatorTest, OneDimensionalTensor) {
  FloatGEMM gemm("GEMM_1D_Error");
  configureVerbose(gemm);

  FloatTensor A(TensorShape{10});
  FloatTensor B(TensorShape{10});

  EXPECT_THROW(gemm.setInputs(A, B), std::runtime_error);
}

// Test consistency between tiled and naive implementations
TEST_F(GEMMOperatorTest, TiledVsNaiveConsistency) {
  FloatTensor A(TensorShape{64, 64});
  FloatTensor B(TensorShape{64, 64});

  srand(789);
  A.fillRandom(-10, 10);
  B.fillRandom(-10, 10);

  // Naive computation
  FloatGEMM gemm_naive("GEMM_Naive");
  configureVerbose(gemm_naive);
  gemm_naive.setInputs(A, B);
  gemm_naive.disableTiling();
  gemm_naive.compute();
  const FloatTensor& C_naive = gemm_naive.getOutput();

  // Tiled computation
  FloatGEMM gemm_tiled("GEMM_Tiled");
  configureVerbose(gemm_tiled);
  gemm_tiled.setInputs(A, B);
  gemm_tiled.enableTiling(TileConfig(16, 16, 16));
  gemm_tiled.compute();
  const FloatTensor& C_tiled = gemm_tiled.getOutput();

  // Results should be identical
  for (size_t i = 0; i < 64; ++i) {
    for (size_t j = 0; j < 64; ++j) {
      EXPECT_NEAR(C_naive.at(i, j), C_tiled.at(i, j), kTolerance)
          << "Mismatch at position (" << i << ", " << j << ")";
    }
  }
}

int main(int argc, char** argv) {
  // Initialize tracer with component filtering
  EventDriven::Tracer::getInstance().initialize("test_gemm_operator_trace.log",
                                                true);
  EventDriven::Tracer::getInstance().setVerbose(false);

  // Only trace Scheduler and specific components to reduce trace size
  // Comment out these lines to trace all components
  EventDriven::Tracer::getInstance().addComponentFilter("Scheduler");
  EventDriven::Tracer::getInstance().addComponentFilter("TestTPU");
  // Don't trace individual MACs to avoid millions of entries
  // If you want to trace MACs, uncomment:
  // EventDriven::Tracer::getInstance().addComponentFilter("MAC_0_0");
  // EventDriven::Tracer::getInstance().addComponentFilter("MAC_0_1");

  ::testing::InitGoogleTest(&argc, argv);
  int result = RUN_ALL_TESTS();

  // Dump trace file
  EventDriven::Tracer::getInstance().dump();
  std::cout << "\nTrace file saved to: "
            << EventDriven::Tracer::getInstance().getOutputPath() << "\n";

  return result;
}
