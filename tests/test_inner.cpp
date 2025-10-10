#include <gtest/gtest.h>

#include <iostream>
#include <memory>
#include <vector>

#include "debug.h"
#include "pe.h"

// Helper to tick a PE until output is ready
static uint32_t runPEUntilOutput(PE& pe,
                                 std::shared_ptr<Debugger> dbg = nullptr,
                                 int maxCycles = 50) {
  for (int i = 0; i < maxCycles; ++i) {
    pe.tick(dbg);
    Data d;
    if (pe.outPort.read(d)) return d.data;
  }
  throw std::runtime_error("PE did not produce output in expected cycles");
}

// -------------------- 2x2 Systolic Inner-Product Test --------------------
TEST(PETest, InnerProduct2x2) {
  auto dbg = std::make_shared<PrintDebugger>();

  // 2x2 matrix A and B
  std::vector<std::vector<int>> A = {{1, 2}, {3, 4}};
  std::vector<std::vector<int>> B = {{5, 6}, {7, 8}};

  // Initialize 2x2 PE array
  PE pe00(4, 4, Op::smac, true);
  PE pe01(4, 4, Op::smac, true);
  PE pe10(4, 4, Op::smac, true);
  PE pe11(4, 4, Op::smac, true);

  // Feed first cycle inputs
  pe00.inPort0.write({1, false});  // A[0][0]
  pe00.inPort1.write({5, false});  // B[0][0]

  pe01.inPort0.write({1, false});  // A[0][0]
  pe01.inPort1.write({6, false});  // B[0][1]

  pe10.inPort0.write({3, false});  // A[1][0]
  pe10.inPort1.write({5, false});  // B[0][0]

  pe11.inPort0.write({3, false});  // A[1][0]
  pe11.inPort1.write({6, false});  // B[0][1]

  // Tick first multiplication
  pe00.tick(dbg);
  pe01.tick(dbg);
  pe10.tick(dbg);
  pe11.tick(dbg);

  // Feed second cycle inputs (last=true to trigger output)
  pe00.inPort0.write({2, true});  // A[0][1]
  pe00.inPort1.write({7, true});  // B[1][0]

  pe01.inPort0.write({2, true});  // A[0][1]
  pe01.inPort1.write({8, true});  // B[1][1]

  pe10.inPort0.write({4, true});  // A[1][1]
  pe10.inPort1.write({7, true});  // B[1][0]

  pe11.inPort0.write({4, true});  // A[1][1]
  pe11.inPort1.write({8, true});  // B[1][1]

  // Run until output
  uint32_t c00 = runPEUntilOutput(pe00, dbg);
  uint32_t c01 = runPEUntilOutput(pe01, dbg);
  uint32_t c10 = runPEUntilOutput(pe10, dbg);
  uint32_t c11 = runPEUntilOutput(pe11, dbg);

  // Check results: C = A*B
  std::cout << "C = \n"
            << c00 << " " << c01 << "\n"
            << c10 << " " << c11 << "\n";

  EXPECT_EQ(c00, 1 * 5 + 2 * 7);
  EXPECT_EQ(c01, 1 * 6 + 2 * 8);
  EXPECT_EQ(c10, 3 * 5 + 4 * 7);
  EXPECT_EQ(c11, 3 * 6 + 4 * 8);
}
