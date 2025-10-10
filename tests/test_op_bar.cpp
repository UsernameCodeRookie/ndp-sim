#include <gtest/gtest.h>

#include <stdexcept>

#include "pe.h"

// Helper to tick PE until output ready
static uint32_t runPEUntilOutput(PE& pe, int maxCycles = 10) {
  Data d;
  for (int i = 0; i < maxCycles; ++i) {
    pe.tick();
    if (pe.outPort.read(d)) return d.data;
  }
  throw std::runtime_error("PE did not produce output in expected cycles");
}

// This test only checks operand barrier / timing, not computation
TEST(PETest, OperandBarrierTiming) {
  PE pe(2, 2, Op::mac);  // 3-input operation
  Data dummy_out;

  // Stage 1: only src0 arrives
  pe.inPort0.write({10, false});
  pe.tick();
  EXPECT_FALSE(pe.outPort.read(dummy_out));  // ALU should not fire

  // Stage 2: src2 arrives
  pe.inPort2.write({5, false});
  pe.tick();
  EXPECT_FALSE(pe.outPort.read(dummy_out));  // still waiting for src1

  // Stage 3: src1 arrives, now ALU should accept operands
  pe.inPort1.write({3, false});
  uint32_t val = runPEUntilOutput(pe);  // run until output ready
  EXPECT_GE(val, 0u);                   // operands processed

  // Stage 4: operands arriving simultaneously
  PE pe2(2, 2, Op::mac);
  pe2.inPort0.write({1, false});
  pe2.inPort1.write({2, false});
  pe2.inPort2.write({3, false});
  val = runPEUntilOutput(pe2);
  EXPECT_GE(val, 0u);  // all operands ready at once

  // Stage 5: staggered inputs with 2-input operation
  PE pe3(2, 2, Op::add);
  pe3.inPort0.write({7, false});
  pe3.tick();
  EXPECT_FALSE(pe3.outPort.read(dummy_out));  // waiting for src1
  pe3.inPort1.write({8, false});
  val = runPEUntilOutput(pe3);
  EXPECT_GE(val, 0u);  // now ready
}
