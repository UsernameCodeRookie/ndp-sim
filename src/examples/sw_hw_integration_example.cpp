#include <iostream>

#include "../src/components/tpu.h"
#include "../src/operators/gemm_operator.h"

using namespace Operators;

/**
 * @brief Example demonstrating integration between software operator layer
 *        and hardware TPU simulation
 */
int main() {
  std::cout << "========================================\n";
  std::cout << "Software-Hardware Integration Example\n";
  std::cout << "========================================\n\n";

  // Part 1: Software layer GEMM operator
  std::cout << "Part 1: Software Layer GEMM\n";
  std::cout << "----------------------------------------\n";

  GEMMOperator<int> gemm_sw("Software_GEMM");
  gemm_sw.setVerbose(true);

  // Create small matrices for demonstration
  const size_t SIZE = 4;
  Tensor<int> A(TensorShape{SIZE, SIZE});
  Tensor<int> B(TensorShape{SIZE, SIZE});

  // Initialize with simple values
  std::cout << "Initializing matrices...\n";
  for (size_t i = 0; i < SIZE; ++i) {
    for (size_t j = 0; j < SIZE; ++j) {
      A.at(i, j) = i + 1;
      B.at(i, j) = j + 1;
    }
  }

  A.print("Matrix A");
  B.print("Matrix B");

  gemm_sw.setInputs(A, B);
  gemm_sw.enableTiling(TileConfig(2, 2, 2));
  gemm_sw.compute();

  const Tensor<int>& C = gemm_sw.getOutput();
  C.print("Result C = A x B");

  std::cout << "\nSoftware GEMM time: " << gemm_sw.getComputationTime()
            << " ms\n";
  std::cout << "Verification: "
            << (verifyGEMM(A, B, C, false) ? "PASSED ✓" : "FAILED ✗") << "\n\n";

  // Part 2: Hardware TPU simulation
  std::cout << "Part 2: Hardware TPU Simulation\n";
  std::cout << "----------------------------------------\n";

  EventDriven::EventScheduler scheduler;
  SystolicArrayTPU tpu("Hardware_TPU", scheduler, 1, SIZE);
  tpu.setVerbose(false);
  tpu.start();

  // Create memory for TPU (simplified)
  std::vector<int> memory(1024, 0);

  // Copy matrices to simulated memory
  uint32_t addr_a = 0;
  uint32_t addr_b = SIZE * SIZE * sizeof(int);
  uint32_t addr_c = 2 * SIZE * SIZE * sizeof(int);

  for (size_t i = 0; i < SIZE * SIZE; ++i) {
    memory[addr_a / sizeof(int) + i] = A[i];
    memory[addr_b / sizeof(int) + i] = B[i];
  }

  std::cout << "TPU array size: " << SIZE << "x" << SIZE << "\n";
  std::cout << "Starting hardware GEMM...\n";

  // Run TPU GEMM
  tpu.startGEMM(SIZE, SIZE, SIZE, addr_a, addr_b, addr_c);

  // Run scheduler (run until max time or no more events)
  scheduler.run(100);

  std::cout << "Hardware simulation completed\n";
  std::cout << "Scheduler time: " << scheduler.getCurrentTime()
            << " cycles\n\n";

  // Part 3: Comparison
  std::cout << "Part 3: Performance Comparison\n";
  std::cout << "----------------------------------------\n";

  std::cout << "Software Layer:\n";
  std::cout << "  - Flexible tile sizes\n";
  std::cout << "  - Easy to verify and debug\n";
  std::cout << "  - Portable across platforms\n";
  std::cout << "  - Time: " << gemm_sw.getComputationTime() << " ms\n\n";

  std::cout << "Hardware Simulation:\n";
  std::cout << "  - Cycle-accurate modeling\n";
  std::cout << "  - Models memory hierarchy\n";
  std::cout << "  - Event-driven execution\n";
  std::cout << "  - Cycles: " << scheduler.getCurrentTime() << "\n\n";

  // Part 4: Larger 64x64 example
  std::cout << "Part 4: 64x64 GEMM Performance\n";
  std::cout << "----------------------------------------\n";

  Tensor<int> A_64(TensorShape{64, 64});
  Tensor<int> B_64(TensorShape{64, 64});
  A_64.fillRandom(0, 10);
  B_64.fillRandom(0, 10);

  GEMMOperator<int> gemm_64("GEMM_64x64");
  gemm_64.setInputs(A_64, B_64);

  // Test different tile sizes
  std::vector<TileConfig> configs = {
      TileConfig(8, 8, 8), TileConfig(16, 16, 16), TileConfig(32, 32, 32),
      TileConfig(64, 64, 64)  // No tiling
  };

  std::cout << "Testing different tile configurations:\n\n";
  for (const auto& config : configs) {
    gemm_64.enableTiling(config);
    gemm_64.compute();

    const Tensor<int>& result = gemm_64.getOutput();
    bool correct = verifyGEMM(A_64, B_64, result, false);

    std::cout << "Tile " << config.tile_m << "x" << config.tile_n << "x"
              << config.tile_k << ": " << gemm_64.getComputationTime()
              << " ms - " << (correct ? "✓" : "✗") << "\n";
  }

  std::cout << "\n========================================\n";
  std::cout << "Integration example completed!\n";
  std::cout << "========================================\n";

  return 0;
}
