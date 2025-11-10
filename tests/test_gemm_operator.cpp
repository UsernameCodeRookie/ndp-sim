#include <gtest/gtest.h>

#include "../src/operators/gemm_operator.h"

using namespace Operators;

// Test fixture for GEMM operator tests
class GEMMOperatorTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Setup code if needed
  }

  void TearDown() override {
    // Cleanup code if needed
  }
};

// Test small GEMM (2x2)
TEST_F(GEMMOperatorTest, SmallGEMM_2x2) {
  GEMMOperator<int> gemm("GEMM_2x2");

  Tensor<int> A(TensorShape{2, 2});
  Tensor<int> B(TensorShape{2, 2});

  // A = [[1, 2], [3, 4]]
  A.at(0, 0) = 1;
  A.at(0, 1) = 2;
  A.at(1, 0) = 3;
  A.at(1, 1) = 4;

  // B = [[5, 6], [7, 8]]
  B.at(0, 0) = 5;
  B.at(0, 1) = 6;
  B.at(1, 0) = 7;
  B.at(1, 1) = 8;

  gemm.setInputs(A, B);
  gemm.compute();

  const Tensor<int>& C = gemm.getOutput();

  // C = [[19, 22], [43, 50]]
  EXPECT_EQ(C.at(0, 0), 19);
  EXPECT_EQ(C.at(0, 1), 22);
  EXPECT_EQ(C.at(1, 0), 43);
  EXPECT_EQ(C.at(1, 1), 50);

  EXPECT_TRUE(verifyGEMM(A, B, C, false));
}

// Test identity matrix multiplication
TEST_F(GEMMOperatorTest, IdentityMultiplication) {
  GEMMOperator<int> gemm("GEMM_Identity");

  Tensor<int> A(TensorShape{4, 4});
  Tensor<int> B(TensorShape{4, 4});

  // A = Identity matrix
  for (size_t i = 0; i < 4; ++i) {
    for (size_t j = 0; j < 4; ++j) {
      A.at(i, j) = (i == j) ? 1 : 0;
    }
  }

  // B = Sequential values
  for (size_t i = 0; i < 4; ++i) {
    for (size_t j = 0; j < 4; ++j) {
      B.at(i, j) = i * 4 + j + 1;
    }
  }

  gemm.setInputs(A, B);
  gemm.compute();

  const Tensor<int>& C = gemm.getOutput();

  // C should equal B (I * B = B)
  for (size_t i = 0; i < 4; ++i) {
    for (size_t j = 0; j < 4; ++j) {
      EXPECT_EQ(C.at(i, j), B.at(i, j));
    }
  }

  EXPECT_TRUE(verifyGEMM(A, B, C, false));
}

// Test GEMM with zero matrix
TEST_F(GEMMOperatorTest, ZeroMatrix) {
  GEMMOperator<int> gemm("GEMM_Zero");

  Tensor<int> A(TensorShape{3, 3}, 0);  // Zero matrix
  Tensor<int> B(TensorShape{3, 3}, 5);  // All 5s

  gemm.setInputs(A, B);
  gemm.compute();

  const Tensor<int>& C = gemm.getOutput();

  // C should be all zeros
  for (size_t i = 0; i < 3; ++i) {
    for (size_t j = 0; j < 3; ++j) {
      EXPECT_EQ(C.at(i, j), 0);
    }
  }

  EXPECT_TRUE(verifyGEMM(A, B, C, false));
}

// Test 64x64 GEMM (naive)
TEST_F(GEMMOperatorTest, GEMM_64x64_Naive) {
  GEMMOperator<int> gemm("GEMM_64x64_Naive");

  Tensor<int> A(TensorShape{64, 64});
  Tensor<int> B(TensorShape{64, 64});

  // Initialize with known pattern
  for (size_t i = 0; i < 64; ++i) {
    for (size_t j = 0; j < 64; ++j) {
      A.at(i, j) = (i == j) ? 1 : 0;  // Identity
      B.at(i, j) = i + j;
    }
  }

  gemm.setInputs(A, B);
  gemm.disableTiling();
  gemm.compute();

  const Tensor<int>& C = gemm.getOutput();

  EXPECT_TRUE(verifyGEMM(A, B, C, false));
}

// Test 64x64 GEMM with 16x16 tiling
TEST_F(GEMMOperatorTest, GEMM_64x64_Tiled_16x16) {
  GEMMOperator<int> gemm("GEMM_64x64_Tiled_16");

  Tensor<int> A(TensorShape{64, 64});
  Tensor<int> B(TensorShape{64, 64});

  // Initialize with known pattern
  for (size_t i = 0; i < 64; ++i) {
    for (size_t j = 0; j < 64; ++j) {
      A.at(i, j) = (i == j) ? 1 : 0;  // Identity
      B.at(i, j) = i + j;
    }
  }

  gemm.setInputs(A, B);
  gemm.enableTiling(TileConfig(16, 16, 16));
  gemm.compute();

  const Tensor<int>& C = gemm.getOutput();

  EXPECT_TRUE(verifyGEMM(A, B, C, false));
}

// Test 64x64 GEMM with 8x8 tiling
TEST_F(GEMMOperatorTest, GEMM_64x64_Tiled_8x8) {
  GEMMOperator<int> gemm("GEMM_64x64_Tiled_8");

  Tensor<int> A(TensorShape{64, 64});
  Tensor<int> B(TensorShape{64, 64});

  // Random initialization
  srand(123);
  A.fillRandom(0, 10);
  B.fillRandom(0, 10);

  gemm.setInputs(A, B);
  gemm.enableTiling(TileConfig(8, 8, 8));
  gemm.compute();

  const Tensor<int>& C = gemm.getOutput();

  EXPECT_TRUE(verifyGEMM(A, B, C, false));
}

// Test 64x64 GEMM with 32x32 tiling
TEST_F(GEMMOperatorTest, GEMM_64x64_Tiled_32x32) {
  GEMMOperator<int> gemm("GEMM_64x64_Tiled_32");

  Tensor<int> A(TensorShape{64, 64});
  Tensor<int> B(TensorShape{64, 64});

  // Random initialization
  srand(456);
  A.fillRandom(-5, 5);
  B.fillRandom(-5, 5);

  gemm.setInputs(A, B);
  gemm.enableTiling(TileConfig(32, 32, 32));
  gemm.compute();

  const Tensor<int>& C = gemm.getOutput();

  EXPECT_TRUE(verifyGEMM(A, B, C, false));
}

// Test non-square GEMM
TEST_F(GEMMOperatorTest, NonSquareGEMM) {
  GEMMOperator<int> gemm("GEMM_NonSquare");

  Tensor<int> A(TensorShape{64, 32});  // 64x32
  Tensor<int> B(TensorShape{32, 64});  // 32x64

  A.fillRandom(0, 5);
  B.fillRandom(0, 5);

  gemm.setInputs(A, B);
  gemm.enableTiling(TileConfig(16, 16, 16));
  gemm.compute();

  const Tensor<int>& C = gemm.getOutput();

  EXPECT_EQ(C.shape()[0], 64);
  EXPECT_EQ(C.shape()[1], 64);
  EXPECT_TRUE(verifyGEMM(A, B, C, false));
}

// Test error handling: dimension mismatch
TEST_F(GEMMOperatorTest, DimensionMismatch) {
  GEMMOperator<int> gemm("GEMM_Error");

  Tensor<int> A(TensorShape{4, 5});
  Tensor<int> B(TensorShape{3, 4});  // Wrong K dimension

  EXPECT_THROW(gemm.setInputs(A, B), std::runtime_error);
}

// Test error handling: 1D tensor
TEST_F(GEMMOperatorTest, OneDimensionalTensor) {
  GEMMOperator<int> gemm("GEMM_1D_Error");

  Tensor<int> A(TensorShape{10});
  Tensor<int> B(TensorShape{10});

  EXPECT_THROW(gemm.setInputs(A, B), std::runtime_error);
}

// Test consistency between tiled and naive implementations
TEST_F(GEMMOperatorTest, TiledVsNaiveConsistency) {
  Tensor<int> A(TensorShape{64, 64});
  Tensor<int> B(TensorShape{64, 64});

  srand(789);
  A.fillRandom(-10, 10);
  B.fillRandom(-10, 10);

  // Naive computation
  GEMMOperator<int> gemm_naive("GEMM_Naive");
  gemm_naive.setInputs(A, B);
  gemm_naive.disableTiling();
  gemm_naive.compute();
  const Tensor<int>& C_naive = gemm_naive.getOutput();

  // Tiled computation
  GEMMOperator<int> gemm_tiled("GEMM_Tiled");
  gemm_tiled.setInputs(A, B);
  gemm_tiled.enableTiling(TileConfig(16, 16, 16));
  gemm_tiled.compute();
  const Tensor<int>& C_tiled = gemm_tiled.getOutput();

  // Results should be identical
  for (size_t i = 0; i < 64; ++i) {
    for (size_t j = 0; j < 64; ++j) {
      EXPECT_EQ(C_naive.at(i, j), C_tiled.at(i, j))
          << "Mismatch at position (" << i << ", " << j << ")";
    }
  }
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
