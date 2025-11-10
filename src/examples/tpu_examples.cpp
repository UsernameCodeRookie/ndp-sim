#include <iomanip>
#include <iostream>
#include <memory>
#include <vector>

#include "../src/components/tpu.h"
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

void example1_simple_gemm() {
  std::cout << "\n" << std::string(60, '=') << std::endl;
  std::cout << "Example 1: Simple 2x2 Matrix Multiplication" << std::endl;
  std::cout << std::string(60, '=') << std::endl;

  // Create scheduler and TPU
  EventDriven::EventScheduler scheduler;
  auto tpu = std::make_shared<SystolicArrayTPU>("TPU", scheduler, 1, 4);
  tpu->setVerbose(true);
  tpu->start();

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

  // Load matrices to memory
  std::cout << "Loading matrices to memory..." << std::endl;
  tpu->loadMatrixToMemory(A, addr_a);
  tpu->loadMatrixToMemory(B, addr_b);

  // Start GEMM computation
  std::cout << "\nStarting GEMM computation..." << std::endl;
  tpu->startGEMM(M, N, K, addr_a, addr_b, addr_c);

  // Run simulation
  int max_cycles = 100;
  for (int i = 0; i < max_cycles && !tpu->isDone(); ++i) {
    scheduler.run(1);
  }

  // Read result from memory
  std::cout << "\nReading result from memory..." << std::endl;
  auto result = tpu->readMatrixFromMemory(addr_c, M * N);

  std::cout << "\nResult:" << std::endl;
  printMatrix("Matrix C (TPU)", result, M, N);

  // Verify with CPU
  auto expected = cpuGEMM(A, B, M, K, N);
  printMatrix("Matrix C (CPU Reference)", expected, M, N);

  if (verifyResults(result, expected)) {
    std::cout << "✓ Results match! GEMM is correct." << std::endl;
  } else {
    std::cout << "✗ Results don't match!" << std::endl;
  }

  std::cout << "\nStatistics:" << std::endl;
  std::cout << "  Operations completed: " << tpu->getOperationsCompleted()
            << std::endl;
}

void example2_larger_gemm() {
  std::cout << "\n" << std::string(60, '=') << std::endl;
  std::cout << "Example 2: 4x4 Matrix Multiplication" << std::endl;
  std::cout << std::string(60, '=') << std::endl;

  EventDriven::EventScheduler scheduler;
  auto tpu = std::make_shared<SystolicArrayTPU>("TPU", scheduler, 1, 4);
  tpu->setVerbose(false);  // Disable verbose for larger matrices
  tpu->start();

  // Define 4x4 matrices
  size_t M = 4, K = 4, N = 4;

  std::vector<int> A = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};

  std::vector<int> B = {1, 0, 0, 0, 0, 1, 0, 0,
                        0, 0, 1, 0, 0, 0, 0, 1};  // Identity-like

  std::cout << "\nInput matrices:" << std::endl;
  printMatrix("Matrix A", A, M, K);
  printMatrix("Matrix B", B, K, N);

  // Memory layout
  uint32_t addr_a = 0;
  uint32_t addr_b = 100;
  uint32_t addr_c = 200;

  // Load matrices to memory
  std::cout << "Loading matrices to memory..." << std::endl;
  tpu->loadMatrixToMemory(A, addr_a);
  tpu->loadMatrixToMemory(B, addr_b);

  // Start GEMM computation
  std::cout << "Starting GEMM computation..." << std::endl;
  tpu->startGEMM(M, N, K, addr_a, addr_b, addr_c);

  // Run simulation
  int max_cycles = 200;
  for (int i = 0; i < max_cycles && !tpu->isDone(); ++i) {
    scheduler.run(1);
  }

  // Read result from memory
  std::cout << "Reading result from memory..." << std::endl;
  auto result = tpu->readMatrixFromMemory(addr_c, M * N);

  std::cout << "\nResult:" << std::endl;
  printMatrix("Matrix C (TPU)", result, M, N);

  // Verify with CPU
  auto expected = cpuGEMM(A, B, M, K, N);
  printMatrix("Matrix C (CPU Reference)", expected, M, N);

  if (verifyResults(result, expected)) {
    std::cout << "✓ Results match! GEMM is correct." << std::endl;
  } else {
    std::cout << "✗ Results don't match!" << std::endl;
  }

  std::cout << "\nStatistics:" << std::endl;
  std::cout << "  Operations completed: " << tpu->getOperationsCompleted()
            << std::endl;
}

void example3_rectangular_gemm() {
  std::cout << "\n" << std::string(60, '=') << std::endl;
  std::cout << "Example 3: Rectangular Matrix Multiplication (3x2 * 2x3)"
            << std::endl;
  std::cout << std::string(60, '=') << std::endl;

  EventDriven::EventScheduler scheduler;
  auto tpu = std::make_shared<SystolicArrayTPU>("TPU", scheduler, 1, 4);
  tpu->setVerbose(false);
  tpu->start();

  // Define rectangular matrices
  // A: 3x2, B: 2x3, C: 3x3
  size_t M = 3, K = 2, N = 3;

  std::vector<int> A = {1, 2, 3, 4, 5, 6};  // 3x2

  std::vector<int> B = {1, 2, 3, 4, 5, 6};  // 2x3

  std::cout << "\nInput matrices:" << std::endl;
  printMatrix("Matrix A", A, M, K);
  printMatrix("Matrix B", B, K, N);

  // Memory layout
  uint32_t addr_a = 0;
  uint32_t addr_b = 100;
  uint32_t addr_c = 200;

  // Load matrices to memory
  std::cout << "Loading matrices to memory..." << std::endl;
  tpu->loadMatrixToMemory(A, addr_a);
  tpu->loadMatrixToMemory(B, addr_b);

  // Start GEMM computation
  std::cout << "Starting GEMM computation..." << std::endl;
  tpu->startGEMM(M, N, K, addr_a, addr_b, addr_c);

  // Run simulation
  int max_cycles = 200;
  for (int i = 0; i < max_cycles && !tpu->isDone(); ++i) {
    scheduler.run(1);
  }

  // Read result from memory
  std::cout << "Reading result from memory..." << std::endl;
  auto result = tpu->readMatrixFromMemory(addr_c, M * N);

  std::cout << "\nResult:" << std::endl;
  printMatrix("Matrix C (TPU)", result, M, N);

  // Verify with CPU
  auto expected = cpuGEMM(A, B, M, K, N);
  printMatrix("Matrix C (CPU Reference)", expected, M, N);

  if (verifyResults(result, expected)) {
    std::cout << "✓ Results match! GEMM is correct." << std::endl;
  } else {
    std::cout << "✗ Results don't match!" << std::endl;
  }

  std::cout << "\nStatistics:" << std::endl;
  std::cout << "  Operations completed: " << tpu->getOperationsCompleted()
            << std::endl;
}

void example4_performance_analysis() {
  std::cout << "\n" << std::string(60, '=') << std::endl;
  std::cout << "Example 4: Performance Analysis" << std::endl;
  std::cout << std::string(60, '=') << std::endl;

  std::cout << "\nComparing different matrix sizes:" << std::endl;
  std::cout << std::string(60, '-') << std::endl;
  std::cout << std::left << std::setw(20) << "Matrix Size" << std::setw(15)
            << "Operations" << std::setw(15) << "Cycles" << std::setw(15)
            << "Throughput" << std::endl;
  std::cout << std::string(60, '-') << std::endl;

  std::vector<size_t> sizes = {2, 3, 4};

  for (size_t size : sizes) {
    EventDriven::EventScheduler scheduler;
    auto tpu = std::make_shared<SystolicArrayTPU>("TPU", scheduler, 1, 4);
    tpu->setVerbose(false);
    tpu->start();

    size_t M = size, K = size, N = size;

    // Create simple matrices
    std::vector<int> A(M * K, 1);  // All ones
    std::vector<int> B(K * N, 1);  // All ones

    uint32_t addr_a = 0;
    uint32_t addr_b = 100;
    uint32_t addr_c = 200;

    tpu->loadMatrixToMemory(A, addr_a);
    tpu->loadMatrixToMemory(B, addr_b);

    uint64_t start_time = scheduler.getCurrentTime();
    tpu->startGEMM(M, N, K, addr_a, addr_b, addr_c);

    int max_cycles = 500;
    int cycles = 0;
    for (cycles = 0; cycles < max_cycles && !tpu->isDone(); ++cycles) {
      scheduler.run(1);
    }
    uint64_t end_time = scheduler.getCurrentTime();

    uint64_t elapsed = end_time - start_time;
    size_t operations = tpu->getOperationsCompleted();
    double throughput = elapsed > 0 ? (double)operations / elapsed : 0;

    std::cout << std::left << std::setw(20)
              << (std::to_string(size) + "x" + std::to_string(size))
              << std::setw(15) << operations << std::setw(15) << elapsed
              << std::setw(15) << std::fixed << std::setprecision(2)
              << throughput << std::endl;
  }

  std::cout << std::string(60, '-') << std::endl;
}

int main() {
  std::cout << "\n" << std::string(60, '=') << std::endl;
  std::cout << "TPU (Tensor Processing Unit) Examples" << std::endl;
  std::cout << std::string(60, '=') << std::endl;
  std::cout << "\nFeatures:" << std::endl;
  std::cout << "  - Systolic Array Architecture (4x4 MAC units)" << std::endl;
  std::cout << "  - Weight-stationary dataflow" << std::endl;
  std::cout << "  - Integrated LSU for memory access" << std::endl;
  std::cout << "  - GEMM (General Matrix Multiply) support" << std::endl;
  std::cout << "  - MAC (Multiply-Accumulate) operations" << std::endl;

  try {
    example1_simple_gemm();
    example2_larger_gemm();
    example3_rectangular_gemm();
    example4_performance_analysis();

    std::cout << "\n" << std::string(60, '=') << std::endl;
    std::cout << "All examples completed successfully! ✓" << std::endl;
    std::cout << std::string(60, '=') << std::endl;

    return 0;
  } catch (const std::exception& e) {
    std::cerr << "\nError: " << e.what() << std::endl;
    return 1;
  }
}
