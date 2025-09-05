#include <gtest/gtest.h>

#include <iostream>
#include <memory>
#include <vector>

#include "debug.h"
#include "pe.h"

// Helper: tick PE until outPort produces output
static uint32_t runPEUntilOutput(PE& pe,
                                 std::shared_ptr<Debugger> dbg = nullptr,
                                 int maxCycles = 50) {
  for (int i = 0; i < maxCycles; ++i) {
    pe.tick(dbg);
    Data d;
    if (pe.outPort.read(d)) return d.value;
  }
  throw std::runtime_error("PE did not produce output in expected cycles");
}

// -------------------- Outer-Product Matrix Multiply (3x3) --------------------
TEST(PETest, OuterProduct3x3MatrixMultiply) {
  auto dbg = std::make_shared<PrintDebugger>();

  std::vector<std::vector<int>> A = {{1, 2, 3}, {4, 5, 6}, {7, 8, 9}};
  std::vector<std::vector<int>> B = {{1, 0, 2}, {0, 1, 3}, {4, 5, 6}};
  int N = 3;
  std::vector<std::vector<int>> C(N, std::vector<int>(N, 0));

  // Create 3x3 PE array
  std::vector<std::vector<PE>> pe(N,
                                  std::vector<PE>(N, PE(4, 4, Op::smac, true)));

  for (int k = 0; k < N; ++k) {
    // Feed outer product A[:,k] * B[k,:] into all PEs
    for (int i = 0; i < N; ++i) {
      for (int j = 0; j < N; ++j) {
        bool last = (k == N - 1);  // mark last iteration
        pe[i][j].inPort0.write({(uint32_t)A[i][k], last});
        pe[i][j].inPort1.write({(uint32_t)B[k][j], last});
        pe[i][j].tick(dbg);  // tick after feeding inputs
      }
    }
  }

  // Tick PEs until outputs ready
  for (int i = 0; i < N; ++i) {
    for (int j = 0; j < N; ++j) {
      uint32_t val = runPEUntilOutput(pe[i][j], dbg);
      C[i][j] = val;
    }
  }

  // Print the result
  std::cout << "Result matrix C = A*B:\n";
  for (int i = 0; i < N; ++i) {
    for (int j = 0; j < N; ++j) {
      std::cout << C[i][j] << " ";
    }
    std::cout << "\n";
  }

  // Optional: verify correctness
  EXPECT_EQ(C[0][0], 1 * 1 + 2 * 0 + 3 * 4);
  EXPECT_EQ(C[0][1], 1 * 0 + 2 * 1 + 3 * 5);
  EXPECT_EQ(C[0][2], 1 * 2 + 2 * 3 + 3 * 6);
  EXPECT_EQ(C[1][0], 4 * 1 + 5 * 0 + 6 * 4);
  EXPECT_EQ(C[1][1], 4 * 0 + 5 * 1 + 6 * 5);
  EXPECT_EQ(C[1][2], 4 * 2 + 5 * 3 + 6 * 6);
  EXPECT_EQ(C[2][0], 7 * 1 + 8 * 0 + 9 * 4);
  EXPECT_EQ(C[2][1], 7 * 0 + 8 * 1 + 9 * 5);
  EXPECT_EQ(C[2][2], 7 * 2 + 8 * 3 + 9 * 6);
}
