#include <cmath>
#include <iomanip>
#include <iostream>

#include "../components/tpu.h"
#include "../operators/gemm.h"

using namespace Operators;
using Precision = Float32PrecisionTraits;
using TPUPrecision = SystolicArrayTPU<Precision>;

int main() {
  std::cout << "========================================\n";
  std::cout << "GEMM Operator with TPU Acceleration\n";
  std::cout << "========================================\n\n";

  std::cout << std::fixed << std::setprecision(3);

  // Create event scheduler
  EventDriven::EventScheduler scheduler;

  // Create TPU (4x4 systolic array)
  auto tpu = std::make_shared<TPUPrecision>("TPU", scheduler, 1, 4);
  tpu->setVerbose(true);
  tpu->start();

  // Create GEMM operator
  GEMMOperator<float, Precision> gemm_op("GEMM_TPU");
  gemm_op.setVerbose(true);

  // Bind operator to TPU
  gemm_op.bindTPU(tpu);

  std::cout << "Creating input matrices...\n";

  // Test 1: Small matrices (4x4) that fit in TPU array
  std::cout << "\n========================================\n";
  std::cout << "Test 1: 4x4 Matrix Multiplication on TPU\n";
  std::cout << "========================================\n";

  Tensor<float> A1(TensorShape{4, 4});
  Tensor<float> B1(TensorShape{4, 4});

  // Initialize A1 as identity matrix
  for (size_t i = 0; i < 4; ++i) {
    for (size_t j = 0; j < 4; ++j) {
      A1.at(i, j) = (i == j) ? 1.0f : 0.0f;
    }
  }

  // Initialize B1 with values 1-16
  for (size_t i = 0; i < 4; ++i) {
    for (size_t j = 0; j < 4; ++j) {
      B1.at(i, j) = static_cast<float>(i * 4 + j + 1);
    }
  }

  std::cout << "Matrix A (4x4 Identity):\n";
  A1.print("A");
  std::cout << "\nMatrix B (4x4 Sequential):\n";
  B1.print("B");

  gemm_op.setInputs(A1, B1);
  gemm_op.printInfo();

  std::cout << "\nExecuting on TPU...\n";
  gemm_op.compute();

  const Tensor<float>& C1 = gemm_op.getOutput();
  std::cout << "\nResult Matrix C:\n";
  C1.print("C");

  // Verify result
  bool correct1 = verifyGEMM(A1, B1, C1, true);
  std::cout << "\nVerification: " << (correct1 ? "PASSED ✓" : "FAILED ✗")
            << "\n";

  // Test 2: Larger matrices (8x8)
  std::cout << "\n========================================\n";
  std::cout << "Test 2: 8x8 Matrix Multiplication on TPU\n";
  std::cout << "========================================\n";

  Tensor<float> A2(TensorShape{8, 8});
  Tensor<float> B2(TensorShape{8, 8});

  // Simple patterns for testing
  for (size_t i = 0; i < 8; ++i) {
    for (size_t j = 0; j < 8; ++j) {
      A2.at(i, j) = static_cast<float>(i + 1);  // Row-based values
      B2.at(i, j) = static_cast<float>(j + 1);  // Column-based values
    }
  }

  gemm_op.setInputs(A2, B2);
  gemm_op.setVerbose(false);  // Reduce verbosity for larger matrices
  tpu->setVerbose(false);

  std::cout << "Computing 8x8 GEMM on TPU...\n";
  gemm_op.compute();

  const Tensor<float>& C2 = gemm_op.getOutput();
  std::cout << "Computation time: " << gemm_op.getComputationTime() << " ms\n";

  // Display corner values
  std::cout << "\nOutput C (corner values):\n";
  std::cout << "C[0,0] = " << C2.at(0, 0) << "\n";
  std::cout << "C[0,7] = " << C2.at(0, 7) << "\n";
  std::cout << "C[7,0] = " << C2.at(7, 0) << "\n";
  std::cout << "C[7,7] = " << C2.at(7, 7) << "\n";

  bool correct2 = verifyGEMM(A2, B2, C2, true);
  std::cout << "Verification: " << (correct2 ? "PASSED ✓" : "FAILED ✗") << "\n";

  // Test 3: Compare TPU vs CPU execution
  std::cout << "\n========================================\n";
  std::cout << "Test 3: TPU vs CPU Performance Comparison\n";
  std::cout << "========================================\n";

  Tensor<float> A3(TensorShape{16, 16});
  Tensor<float> B3(TensorShape{16, 16});

  A3.fillRandom(0, 10);
  B3.fillRandom(0, 10);

  // Run on TPU
  gemm_op.setInputs(A3, B3);
  gemm_op.compute();
  double tpu_time = gemm_op.getComputationTime();
  Tensor<float> C_tpu = gemm_op.getOutput();

  // Run on CPU (naive)
  gemm_op.unbindTPU();
  gemm_op.disableTiling();
  gemm_op.compute();
  double cpu_naive_time = gemm_op.getComputationTime();
  Tensor<float> C_cpu_naive = gemm_op.getOutput();

  // Run on CPU (tiled)
  gemm_op.enableTiling(TileConfig(8, 8, 8));
  gemm_op.compute();
  double cpu_tiled_time = gemm_op.getComputationTime();
  Tensor<float> C_cpu_tiled = gemm_op.getOutput();

  std::cout << "\nPerformance Results (16x16 GEMM):\n";
  std::cout << "  TPU:        " << tpu_time << " ms\n";
  std::cout << "  CPU Naive:  " << cpu_naive_time << " ms\n";
  std::cout << "  CPU Tiled:  " << cpu_tiled_time << " ms\n";

  // Verify all results match
  bool match_naive = true;
  bool match_tiled = true;
  const float tolerance = 1e-3f;
  for (size_t i = 0; i < 16; ++i) {
    for (size_t j = 0; j < 16; ++j) {
      if (std::fabs(C_tpu.at(i, j) - C_cpu_naive.at(i, j)) > tolerance)
        match_naive = false;
      if (std::fabs(C_tpu.at(i, j) - C_cpu_tiled.at(i, j)) > tolerance)
        match_tiled = false;
    }
  }

  std::cout << "\nConsistency Check:\n";
  std::cout << "  TPU vs CPU Naive: "
            << (match_naive ? "MATCH ✓" : "MISMATCH ✗") << "\n";
  std::cout << "  TPU vs CPU Tiled: "
            << (match_tiled ? "MATCH ✓" : "MISMATCH ✗") << "\n";

  // Test 4: Non-square matrices
  std::cout << "\n========================================\n";
  std::cout << "Test 4: Non-square Matrix Multiplication\n";
  std::cout << "========================================\n";

  Tensor<float> A4(TensorShape{4, 8});  // 4x8
  Tensor<float> B4(TensorShape{8, 4});  // 8x4
  // Result will be 4x4

  A4.fillRandom(1, 5);
  B4.fillRandom(1, 5);

  gemm_op.bindTPU(tpu);
  gemm_op.setInputs(A4, B4);

  std::cout << "Computing (4x8) * (8x4) = (4x4) on TPU...\n";
  gemm_op.compute();

  const Tensor<float>& C4 = gemm_op.getOutput();
  C4.print("Result");

  bool correct4 = verifyGEMM(A4, B4, C4, true);
  std::cout << "Verification: " << (correct4 ? "PASSED ✓" : "FAILED ✗") << "\n";

  // Summary
  std::cout << "\n========================================\n";
  std::cout << "Summary\n";
  std::cout << "========================================\n";
  std::cout << "All tests demonstrate that GEMM operator can run on:\n";
  std::cout << "  1. TPU hardware (systolic array) for acceleration\n";
  std::cout << "  2. CPU with naive implementation (fallback)\n";
  std::cout << "  3. CPU with tiled implementation (optimized fallback)\n";
  std::cout << "\nThe operator abstracts the execution backend, allowing\n";
  std::cout << "flexible deployment based on hardware availability.\n";
  std::cout << "========================================\n";

  return 0;
}
