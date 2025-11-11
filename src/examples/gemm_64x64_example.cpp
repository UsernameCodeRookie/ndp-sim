#include <iostream>

#include "../src/operators/gemm.h"
#include "../src/trace.h"

using namespace Operators;

int main() {
  std::cout << "========================================\n";
  std::cout << "GEMM Operator Example - 64x64 Matrix\n";
  std::cout << "========================================\n\n";

  // Initialize tracer
  EventDriven::Tracer::getInstance().initialize("gemm_64x64_trace.log", true);
  EventDriven::Tracer::getInstance().setVerbose(false);

  // Create 64x64 GEMM operator
  GEMMOperator<int> gemm_op("GEMM_64x64");
  gemm_op.setVerbose(true);

  // Create input tensors: A(64x64) * B(64x64) = C(64x64)
  std::cout << "Creating input matrices...\n";
  Tensor<int> A(TensorShape{64, 64});
  Tensor<int> B(TensorShape{64, 64});

  // Initialize with simple patterns for easy verification
  // A: identity-like pattern
  for (size_t i = 0; i < 64; ++i) {
    for (size_t j = 0; j < 64; ++j) {
      A.at(i, j) = (i == j) ? 1 : 0;  // Identity matrix
    }
  }

  // B: sequential values
  for (size_t i = 0; i < 64; ++i) {
    for (size_t j = 0; j < 64; ++j) {
      B.at(i, j) = i * 64 + j + 1;  // Values from 1 to 4096
    }
  }

  std::cout << "Matrix A: 64x64 Identity Matrix\n";
  std::cout << "Matrix B: 64x64 Sequential Values (1-4096)\n\n";

  // Set inputs
  gemm_op.setInputs(A, B);
  gemm_op.printInfo();

  // Test 1: Naive computation
  std::cout << "\n========================================\n";
  std::cout << "Test 1: Naive GEMM (no tiling)\n";
  std::cout << "========================================\n";
  gemm_op.disableTiling();
  gemm_op.compute();

  const Tensor<int>& C_naive = gemm_op.getOutput();
  std::cout << "Computation time: " << gemm_op.getComputationTime() << " ms\n";

  // Verify result
  bool correct = verifyGEMM(A, B, C_naive, true);
  std::cout << "Verification: " << (correct ? "PASSED ✓" : "FAILED ✗") << "\n";

  // Test 2: Tiled computation with 16x16 tiles
  std::cout << "\n========================================\n";
  std::cout << "Test 2: Tiled GEMM (16x16x16 tiles)\n";
  std::cout << "========================================\n";
  gemm_op.enableTiling(TileConfig(16, 16, 16));
  gemm_op.setVerbose(false);  // Reduce verbosity for tiled computation
  gemm_op.compute();

  const Tensor<int>& C_tiled_16 = gemm_op.getOutput();
  std::cout << "Computation time: " << gemm_op.getComputationTime() << " ms\n";

  correct = verifyGEMM(A, B, C_tiled_16, true);
  std::cout << "Verification: " << (correct ? "PASSED ✓" : "FAILED ✗") << "\n";

  // Test 3: Tiled computation with 8x8 tiles
  std::cout << "\n========================================\n";
  std::cout << "Test 3: Tiled GEMM (8x8x8 tiles)\n";
  std::cout << "========================================\n";
  gemm_op.enableTiling(TileConfig(8, 8, 8));
  gemm_op.compute();

  const Tensor<int>& C_tiled_8 = gemm_op.getOutput();
  std::cout << "Computation time: " << gemm_op.getComputationTime() << " ms\n";

  correct = verifyGEMM(A, B, C_tiled_8, true);
  std::cout << "Verification: " << (correct ? "PASSED ✓" : "FAILED ✗") << "\n";

  // Test 4: Tiled computation with 32x32 tiles
  std::cout << "\n========================================\n";
  std::cout << "Test 4: Tiled GEMM (32x32x32 tiles)\n";
  std::cout << "========================================\n";
  gemm_op.enableTiling(TileConfig(32, 32, 32));
  gemm_op.compute();

  const Tensor<int>& C_tiled_32 = gemm_op.getOutput();
  std::cout << "Computation time: " << gemm_op.getComputationTime() << " ms\n";

  correct = verifyGEMM(A, B, C_tiled_32, true);
  std::cout << "Verification: " << (correct ? "PASSED ✓" : "FAILED ✗") << "\n";

  // Display sample output values
  std::cout << "\n========================================\n";
  std::cout << "Sample Output Values (C = A * B)\n";
  std::cout << "========================================\n";
  std::cout << "Since A is identity, C should equal B\n";
  std::cout << "C[0][0-7]: ";
  for (size_t j = 0; j < 8; ++j) {
    std::cout << C_tiled_32.at(0, j) << " ";
  }
  std::cout << "\n";
  std::cout << "B[0][0-7]: ";
  for (size_t j = 0; j < 8; ++j) {
    std::cout << B.at(0, j) << " ";
  }
  std::cout << "\n\n";

  // Test with random matrices
  std::cout << "========================================\n";
  std::cout << "Test 5: Random Matrices\n";
  std::cout << "========================================\n";

  srand(42);  // Fixed seed for reproducibility
  Tensor<int> A_rand(TensorShape{64, 64});
  Tensor<int> B_rand(TensorShape{64, 64});
  A_rand.fillRandom(0, 10);
  B_rand.fillRandom(0, 10);

  GEMMOperator<int> gemm_rand("GEMM_Random");
  gemm_rand.setInputs(A_rand, B_rand);
  gemm_rand.enableTiling(TileConfig(16, 16, 16));
  gemm_rand.compute();

  const Tensor<int>& C_rand = gemm_rand.getOutput();
  std::cout << "Computation time: " << gemm_rand.getComputationTime()
            << " ms\n";

  correct = verifyGEMM(A_rand, B_rand, C_rand, true);
  std::cout << "Verification: " << (correct ? "PASSED ✓" : "FAILED ✗") << "\n";

  std::cout << "\n========================================\n";
  std::cout << "All tests completed!\n";
  std::cout << "========================================\n";

  // Dump trace file
  EventDriven::Tracer::getInstance().dump();
  std::cout << "\nTrace file saved to: "
            << EventDriven::Tracer::getInstance().getOutputPath() << "\n";

  return 0;
}
