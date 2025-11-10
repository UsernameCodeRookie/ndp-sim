#include <iomanip>
#include <iostream>
#include <memory>
#include <vector>

#include "../src/components/tpu.h"
#include "../src/components/tpu_dramsim3.h"
#include "../src/scheduler.h"

/**
 * @brief Print matrix
 */
void printMatrix(const std::string& name, const std::vector<int>& matrix,
                 size_t rows, size_t cols) {
  std::cout << name << " (" << rows << "x" << cols << "):" << std::endl;
  for (size_t i = 0; i < rows; ++i) {
    std::cout << "  [";
    for (size_t j = 0; j < cols; ++j) {
      std::cout << std::setw(4) << matrix[i * cols + j];
      if (j < cols - 1) std::cout << " ";
    }
    std::cout << "]" << std::endl;
  }
  std::cout << std::endl;
}

/**
 * @brief CPU reference GEMM for verification
 */
std::vector<int> cpuGEMM(const std::vector<int>& A, const std::vector<int>& B,
                         size_t M, size_t K, size_t N) {
  std::vector<int> C(M * N, 0);

  for (size_t i = 0; i < M; ++i) {
    for (size_t j = 0; j < N; ++j) {
      int sum = 0;
      for (size_t k = 0; k < K; ++k) {
        sum += A[i * K + k] * B[k * N + j];
      }
      C[i * N + j] = sum;
    }
  }

  return C;
}

/**
 * @brief Verify results
 */
bool verifyResults(const std::vector<int>& result,
                   const std::vector<int>& expected, float tolerance = 1e-5) {
  if (result.size() != expected.size()) {
    std::cout << "Size mismatch: " << result.size() << " != " << expected.size()
              << std::endl;
    return false;
  }

  for (size_t i = 0; i < result.size(); ++i) {
    if (std::abs(result[i] - expected[i]) > tolerance) {
      std::cout << "Mismatch at index " << i << ": " << result[i]
                << " != " << expected[i] << std::endl;
      return false;
    }
  }

  return true;
}

void example1_tpu_with_dramsim3() {
  std::cout << "\n" << std::string(60, '=') << std::endl;
  std::cout << "Example 1: TPU with DRAMsim3 Integration" << std::endl;
  std::cout << std::string(60, '=') << std::endl;

  // Create scheduler
  EventDriven::EventScheduler scheduler;

  // DRAMsim3 configuration
  std::string dram_config =
      "../third_party/DRAMsim3/configs/DDR4_8Gb_x8_3200.ini";
  std::string dram_output = "./dramsim3_output";

  std::cout << "Creating TPU with DRAMsim3..." << std::endl;
  std::cout << "Config file: " << dram_config << std::endl;

  // Create TPU with DRAMsim3
  auto tpu = std::make_shared<SystolicArrayTPUDRAM>(
      "TPU_DRAM", scheduler, 1, dram_config, dram_output, 4);
  tpu->setVerbose(false);  // Set to true for detailed output
  tpu->start();

  std::cout << "TPU created successfully!" << std::endl;

  // Define matrices
  // A = [1 2]    B = [5 6]    C = A*B = [19 22]
  //     [3 4]        [7 8]              [43 50]
  size_t M = 2, K = 2, N = 2;

  std::vector<int> A = {1, 2, 3, 4};
  std::vector<int> B = {5, 6, 7, 8};

  std::cout << "\nInput matrices:" << std::endl;
  printMatrix("Matrix A", A, M, K);
  printMatrix("Matrix B", B, K, N);

  // Memory layout
  uint32_t addr_a = 0;
  uint32_t addr_b = 100;
  uint32_t addr_c = 200;

  // Load matrices to DRAM memory
  std::cout << "Loading matrices to DRAM memory..." << std::endl;
  tpu->loadMatrixToMemory(A, addr_a);
  tpu->loadMatrixToMemory(B, addr_b);

  // Start GEMM computation
  std::cout << "Starting GEMM computation with DRAMsim3..." << std::endl;
  tpu->startGEMM(M, N, K, addr_a, addr_b, addr_c);

  // Run simulation
  std::cout << "Running simulation..." << std::endl;
  uint64_t max_cycles = 1000;
  uint64_t cycle = 0;

  while (!tpu->isDone() && cycle < max_cycles) {
    scheduler.run(1);
    cycle++;
  }

  std::cout << "Simulation completed in " << cycle << " cycles" << std::endl;

  // Read results from DRAM memory
  std::vector<int> C = tpu->readMatrixFromMemory(addr_c, M * N);

  std::cout << "\nResult:" << std::endl;
  printMatrix("Matrix C", C, M, N);

  // Verify results
  std::vector<int> expected = cpuGEMM(A, B, M, K, N);
  std::cout << "\nExpected result:" << std::endl;
  printMatrix("Expected C", expected, M, N);

  if (verifyResults(C, expected)) {
    std::cout << "✓ Results match! GEMM computation correct." << std::endl;
  } else {
    std::cout << "✗ Results mismatch! GEMM computation incorrect." << std::endl;
  }

  // Print statistics
  tpu->printStats();
}

int main() {
  std::cout << std::string(60, '=') << std::endl;
  std::cout << "TPU with DRAMsim3 Integration Examples" << std::endl;
  std::cout << std::string(60, '=') << std::endl;
  std::cout << std::flush;

  try {
    std::cout << "\nStarting Example 1..." << std::endl;
    std::cout << std::flush;

    example1_tpu_with_dramsim3();

    std::cout << "\n" << std::string(60, '=') << std::endl;
    std::cout << "Example completed successfully! ✓" << std::endl;
    std::cout << std::string(60, '=') << std::endl;

  } catch (const std::runtime_error& e) {
    std::cerr << "\n!!! RUNTIME ERROR !!!" << std::endl;
    std::cerr << "Message: " << e.what() << std::endl;
    return 1;
  } catch (const std::exception& e) {
    std::cerr << "\n!!! EXCEPTION !!!" << std::endl;
    std::cerr << "Type: " << typeid(e).name() << std::endl;
    std::cerr << "Message: " << e.what() << std::endl;
    return 1;
  } catch (...) {
    std::cerr << "\n!!! UNKNOWN ERROR !!!" << std::endl;
    return 1;
  }

  return 0;
}
