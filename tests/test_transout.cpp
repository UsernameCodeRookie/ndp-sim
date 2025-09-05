#include <gtest/gtest.h>

#include <memory>

#include "debug.h"
#include "pe.h"

// Helper: tick PE until outPort produces output (last_flag optional)
static uint32_t runPEUntilOutput(PE& pe,
                                 std::shared_ptr<Debugger> dbg = nullptr,
                                 int maxCycles = 50) {
  for (int i = 0; i < maxCycles; ++i) {
    pe.tick(dbg);  // pass raw ptr to tick
    Data d;
    if (pe.outPort.read(d)) return d.value;
  }
  throw std::runtime_error("PE did not produce output in expected cycles");
}

// -------------------- Transout Accumulation Test --------------------
TEST(PETest, TransoutAccumulator) {
  auto dbg = std::make_shared<PrintDebugger>();

  // Create a PE in transout mode (signed multiply-accumulate)
  PE pe(4, 4, Op::smac, true);

  // Stage 1: Initial input: 3 * 2
  pe.inPort0.write({3, false});
  pe.inPort1.write({2, false});  // last = false

  // for (int i = 0; i < 10; ++i) {
  //   pe.tick(dbg);  // pass raw ptr to tick
  //   Data d;
  //   if (pe.outPort.read(d)) throw std::runtime_error("Unexpected output");
  // }
  pe.tick(dbg);

  // Stage 2: Next input: 3 * 4 + previous output
  pe.inPort0.write({3, true});
  pe.inPort1.write({4, true});  // mark last = true

  uint32_t val = runPEUntilOutput(pe, dbg);
  std::cout << "Second output: " << val << std::endl;
  EXPECT_EQ(val, (3 * 4) + (3 * 2));  // accumulate
}
