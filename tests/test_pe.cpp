#include <gtest/gtest.h>

#include <stdexcept>

#include "pe.h"

// Helper function: run the PE until it produces an output
int runPEUntilOutput(PE& pe, int maxCycles = 10) {
  int outVal = 0;
  for (int i = 0; i < maxCycles; ++i) {
    pe.tick();
    if (pe.readOut(outVal)) {
      return outVal;
    }
  }
  throw std::runtime_error("PE did not produce output in expected cycles");
}

// Test 1: ADD operation with latency = 1
TEST(PETest, AddOperation) {
  PE pe(2, 2, Op::Add, 1);

  // Feed operands
  pe.writeIn0(10);
  pe.writeIn1(20);

  int outVal = runPEUntilOutput(pe);
  EXPECT_EQ(outVal, 30);  // 10 + 20 = 30
}

// Test 2: MUL operation with latency = 2
TEST(PETest, MulOperationLatency2) {
  PE pe(2, 2, Op::Mul, 2);

  // Feed operands
  pe.writeIn0(3);
  pe.writeIn1(7);

  int outVal = runPEUntilOutput(pe, 15);  // allow more cycles for latency=2
  EXPECT_EQ(outVal, 21);                  // 3 * 7 = 21
}

// Test 3: multiple operands in sequence
TEST(PETest, MultipleOperands) {
  PE pe(4, 4, Op::Add, 1);

  // Feed first operand pair
  pe.writeIn0(1);
  pe.writeIn1(2);

  int outVal1 = runPEUntilOutput(pe);
  EXPECT_EQ(outVal1, 3);

  // Feed second operand pair
  pe.writeIn0(5);
  pe.writeIn1(7);

  int outVal2 = runPEUntilOutput(pe);
  EXPECT_EQ(outVal2, 12);
}
