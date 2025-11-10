#include <cassert>
#include <iostream>
#include <memory>
#include <vector>

#include "../src/components/tpu.h"
#include "../src/scheduler.h"

/**
 * @brief CPU reference GEMM
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

void test_mac_unit() {
  std::cout << "\n=== Test: MAC Unit ===" << std::endl;
  std::cout << "✓ MAC unit is tested as part of TPU GEMM operations"
            << std::endl;
}

void test_simple_gemm() {
  std::cout << "\n=== Test: Simple 2x2 GEMM ===" << std::endl;

  EventDriven::EventScheduler scheduler;
  auto tpu = std::make_shared<SystolicArrayTPU>("TPU", scheduler, 1, 4);
  tpu->setVerbose(true);
  tpu->start();

  // A = [1 2]    B = [5 6]    C = [19 22]
  //     [3 4]        [7 8]        [43 50]
  size_t M = 2, K = 2, N = 2;
  std::vector<int> A = {1, 2, 3, 4};
  std::vector<int> B = {5, 6, 7, 8};

  uint32_t addr_a = 0;
  uint32_t addr_b = 100;
  uint32_t addr_c = 200;

  tpu->loadMatrixToMemory(A, addr_a);
  tpu->loadMatrixToMemory(B, addr_b);

  tpu->startGEMM(M, N, K, addr_a, addr_b, addr_c);

  for (int i = 0; i < 100 && !tpu->isDone(); ++i) {
    scheduler.run(1);
  }

  auto result = tpu->readMatrixFromMemory(addr_c, M * N);
  auto expected = cpuGEMM(A, B, M, K, N);

  std::cout << "\nExpected: [" << expected[0] << " " << expected[1] << " "
            << expected[2] << " " << expected[3] << "]" << std::endl;
  std::cout << "Got:      [" << result[0] << " " << result[1] << " "
            << result[2] << " " << result[3] << "]" << std::endl;

  for (size_t i = 0; i < expected.size(); ++i) {
    assert(result[i] == expected[i]);
  }

  std::cout << "✓ Simple GEMM test passed!" << std::endl;
}

void test_identity_matrix() {
  std::cout << "\n=== Test: Identity Matrix Multiplication ===" << std::endl;

  EventDriven::EventScheduler scheduler;
  auto tpu = std::make_shared<SystolicArrayTPU>("TPU", scheduler, 1, 4);
  tpu->setVerbose(false);
  tpu->start();

  size_t M = 3, K = 3, N = 3;

  // A = [1 2 3]   I = [1 0 0]   Result should be A
  //     [4 5 6]       [0 1 0]
  //     [7 8 9]       [0 0 1]
  std::vector<int> A = {1, 2, 3, 4, 5, 6, 7, 8, 9};
  std::vector<int> I = {1, 0, 0, 0, 1, 0, 0, 0, 1};  // Identity

  uint32_t addr_a = 0;
  uint32_t addr_b = 100;
  uint32_t addr_c = 200;

  tpu->loadMatrixToMemory(A, addr_a);
  tpu->loadMatrixToMemory(I, addr_b);

  tpu->startGEMM(M, N, K, addr_a, addr_b, addr_c);

  for (int i = 0; i < 200 && !tpu->isDone(); ++i) {
    scheduler.run(1);
  }

  auto result = tpu->readMatrixFromMemory(addr_c, M * N);
  auto expected = cpuGEMM(A, I, M, K, N);

  std::cout << "Expected result should equal A:" << std::endl;
  for (size_t i = 0; i < expected.size(); ++i) {
    std::cout << "  Element " << i << ": " << result[i] << " (expected "
              << expected[i] << ")" << std::endl;
    assert(result[i] == expected[i]);
  }

  std::cout << "✓ Identity matrix test passed!" << std::endl;
}

void test_zero_matrix() {
  std::cout << "\n=== Test: Zero Matrix Multiplication ===" << std::endl;

  EventDriven::EventScheduler scheduler;
  auto tpu = std::make_shared<SystolicArrayTPU>("TPU", scheduler, 1, 4);
  tpu->setVerbose(false);
  tpu->start();

  size_t M = 2, K = 2, N = 2;

  std::vector<int> A = {1, 2, 3, 4};
  std::vector<int> Zero = {0, 0, 0, 0};

  uint32_t addr_a = 0;
  uint32_t addr_b = 100;
  uint32_t addr_c = 200;

  tpu->loadMatrixToMemory(A, addr_a);
  tpu->loadMatrixToMemory(Zero, addr_b);

  tpu->startGEMM(M, N, K, addr_a, addr_b, addr_c);

  for (int i = 0; i < 100 && !tpu->isDone(); ++i) {
    scheduler.run(1);
  }

  auto result = tpu->readMatrixFromMemory(addr_c, M * N);

  std::cout << "Result should be all zeros:" << std::endl;
  for (size_t i = 0; i < result.size(); ++i) {
    std::cout << "  Element " << i << ": " << result[i] << std::endl;
    assert(result[i] == 0);
  }

  std::cout << "✓ Zero matrix test passed!" << std::endl;
}

void test_rectangular_gemm() {
  std::cout << "\n=== Test: Rectangular Matrix Multiplication ===" << std::endl;

  EventDriven::EventScheduler scheduler;
  auto tpu = std::make_shared<SystolicArrayTPU>("TPU", scheduler, 1, 4);
  tpu->setVerbose(false);
  tpu->start();

  // A: 3x2, B: 2x4, C: 3x4
  size_t M = 3, K = 2, N = 4;

  std::vector<int> A = {1, 2, 3, 4, 5, 6};        // 3x2
  std::vector<int> B = {1, 2, 3, 4, 5, 6, 7, 8};  // 2x4

  uint32_t addr_a = 0;
  uint32_t addr_b = 100;
  uint32_t addr_c = 200;

  tpu->loadMatrixToMemory(A, addr_a);
  tpu->loadMatrixToMemory(B, addr_b);

  tpu->startGEMM(M, N, K, addr_a, addr_b, addr_c);

  for (int i = 0; i < 200 && !tpu->isDone(); ++i) {
    scheduler.run(1);
  }

  auto result = tpu->readMatrixFromMemory(addr_c, M * N);
  auto expected = cpuGEMM(A, B, M, K, N);

  std::cout << "Comparing TPU result with CPU reference:" << std::endl;
  for (size_t i = 0; i < expected.size(); ++i) {
    std::cout << "  Element " << i << ": " << result[i] << " (expected "
              << expected[i] << ")" << std::endl;
    assert(result[i] == expected[i]);
  }

  std::cout << "✓ Rectangular GEMM test passed!" << std::endl;
}

void test_performance() {
  std::cout << "\n=== Test: Performance Metrics ===" << std::endl;

  EventDriven::EventScheduler scheduler;
  auto tpu = std::make_shared<SystolicArrayTPU>("TPU", scheduler, 1, 4);
  tpu->setVerbose(false);
  tpu->start();

  size_t M = 4, K = 4, N = 4;
  std::vector<int> A(M * K, 1);
  std::vector<int> B(K * N, 1);

  uint32_t addr_a = 0;
  uint32_t addr_b = 100;
  uint32_t addr_c = 200;

  tpu->loadMatrixToMemory(A, addr_a);
  tpu->loadMatrixToMemory(B, addr_b);

  uint64_t start_time = scheduler.getCurrentTime();
  tpu->startGEMM(M, N, K, addr_a, addr_b, addr_c);

  int cycles = 0;
  for (cycles = 0; cycles < 500 && !tpu->isDone(); ++cycles) {
    scheduler.run(1);
  }
  uint64_t end_time = scheduler.getCurrentTime();

  std::cout << "Matrix size: " << M << "x" << K << " * " << K << "x" << N
            << std::endl;
  std::cout << "Cycles taken: " << (end_time - start_time) << std::endl;
  std::cout << "Operations: " << tpu->getOperationsCompleted() << std::endl;

  assert(tpu->isDone());
  std::cout << "✓ Performance test passed!" << std::endl;
}

int main() {
  std::cout << "TPU Component Tests" << std::endl;
  std::cout << "===================" << std::endl;

  try {
    test_mac_unit();
    test_simple_gemm();
    test_identity_matrix();
    test_zero_matrix();
    test_rectangular_gemm();
    test_performance();

    std::cout << "\n===================" << std::endl;
    std::cout << "All tests passed! ✓" << std::endl;
    std::cout << "===================" << std::endl;

    return 0;
  } catch (const std::exception& e) {
    std::cerr << "Test failed with exception: " << e.what() << std::endl;
    return 1;
  }
}
