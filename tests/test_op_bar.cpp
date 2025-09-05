#include <gtest/gtest.h>

#include "pe.h"

// Helper to tick PE until output ready
uint32_t runPEUntilOutput(PE& pe, int maxCycles = 10) {
  uint32_t out = 0;
  for (int i = 0; i < maxCycles; ++i) {
    pe.tick();
    if (pe.readOut(out)) return out;
  }
  throw std::runtime_error("PE did not produce output in expected cycles");
}

// This test only checks operand barrier / timing, not computation
TEST(PETest, OperandBarrierTiming) {
  PE pe(2, 2, Op::mac);  // 3-input operation
  uint32_t dummy_out;

  // Stage 1: only src0 arrives
  pe.writeIn0(10);
  pe.tick();
  EXPECT_FALSE(pe.readOut(dummy_out));  // ALU should not fire

  // Stage 2: src2 arrives
  pe.writeIn2(5);
  pe.tick();
  EXPECT_FALSE(pe.readOut(dummy_out));  // still waiting for src1

  // Stage 3: src1 arrives, now ALU should accept operands
  pe.writeIn1(3);
  dummy_out = runPEUntilOutput(pe);  // run until output ready
  EXPECT_TRUE(dummy_out >= 0);       // operands processed

  // Stage 4: operands arriving simultaneously
  PE pe2(2, 2, Op::sum);
  pe2.writeIn0(1);
  pe2.writeIn1(2);
  pe2.writeIn2(3);
  dummy_out = runPEUntilOutput(pe2);
  EXPECT_TRUE(dummy_out >= 0);  // all operands ready at once

  // Stage 5: staggered inputs with 2-input operation
  PE pe3(2, 2, Op::add);
  pe3.writeIn0(7);
  pe3.tick();
  EXPECT_FALSE(pe3.readOut(dummy_out));  // waiting for src1
  pe3.writeIn1(8);
  dummy_out = runPEUntilOutput(pe3);
  EXPECT_TRUE(dummy_out >= 0);  // now ready
}
