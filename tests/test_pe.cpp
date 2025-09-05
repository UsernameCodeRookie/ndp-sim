#include <gtest/gtest.h>

#include <cstring>
#include <stdexcept>

#include "pe.h"

// -------- Float helpers ----------
inline uint32_t f2u(float f) {
  uint32_t x;
  std::memcpy(&x, &f, sizeof(f));
  return x;
}
inline float u2f(uint32_t x) {
  float f;
  std::memcpy(&f, &x, sizeof(f));
  return f;
}

// -------- Run helper ----------
uint32_t runPEUntilOutput(PE& pe, int maxCycles = 20) {
  uint32_t outVal = 0;
  for (int i = 0; i < maxCycles; ++i) {
    pe.tick();
    if (pe.readOut(outVal)) return outVal;
  }
  throw std::runtime_error("PE did not produce output in expected cycles");
}

// =====================================================
//                 TEST CASES
// =====================================================

// Unsigned integer arithmetic
TEST(PETest, AddUnsigned) {
  PE pe(2, 2, Op::add);
  pe.writeIn0(10);
  pe.writeIn1(20);
  pe.writeIn2(0);
  EXPECT_EQ(runPEUntilOutput(pe), 30u);
}

TEST(PETest, SubUnsigned) {
  PE pe(2, 2, Op::sub);
  pe.writeIn0(50);
  pe.writeIn1(8);
  pe.writeIn2(0);
  EXPECT_EQ(runPEUntilOutput(pe), 42u);
}

TEST(PETest, MulUnsigned) {
  PE pe(2, 2, Op::mul);
  pe.writeIn0(6);
  pe.writeIn1(7);
  pe.writeIn2(0);
  EXPECT_EQ(runPEUntilOutput(pe), 42u);
}

TEST(PETest, MacUnsigned) {
  PE pe(2, 2, Op::mac);
  pe.writeIn0(4);
  pe.writeIn1(5);
  pe.writeIn2(6);
  EXPECT_EQ(runPEUntilOutput(pe), 26u);  // 4*5+6
}

// Signed integer arithmetic
TEST(PETest, SaddSigned) {
  PE pe(2, 2, Op::sadd);
  pe.writeIn0(uint32_t(-5));
  pe.writeIn1(uint32_t(3));
  pe.writeIn2(0);
  EXPECT_EQ(int32_t(runPEUntilOutput(pe)), -2);
}

TEST(PETest, SsubSigned) {
  PE pe(2, 2, Op::ssub);
  pe.writeIn0(uint32_t(-5));
  pe.writeIn1(uint32_t(-8));
  pe.writeIn2(0);
  EXPECT_EQ(int32_t(runPEUntilOutput(pe)), 3);
}

TEST(PETest, SmacSigned) {
  PE pe(2, 2, Op::smac);
  pe.writeIn0(uint32_t(-2));
  pe.writeIn1(uint32_t(7));
  pe.writeIn2(uint32_t(5));
  EXPECT_EQ(int32_t(runPEUntilOutput(pe)), -9);  // -2*7+5
}

// Logic / comparison
TEST(PETest, EqOperation) {
  PE pe(2, 2, Op::eq);
  pe.writeIn0(42);
  pe.writeIn1(42);
  pe.writeIn2(0);
  EXPECT_EQ(runPEUntilOutput(pe), 1u);
}

TEST(PETest, NeqOperation) {
  PE pe(2, 2, Op::neq);
  pe.writeIn0(42);
  pe.writeIn1(7);
  pe.writeIn2(0);
  EXPECT_EQ(runPEUntilOutput(pe), 1u);
}

TEST(PETest, LtUnsigned) {
  PE pe(2, 2, Op::lt);
  pe.writeIn0(2);
  pe.writeIn1(5);
  pe.writeIn2(0);
  EXPECT_EQ(runPEUntilOutput(pe), 1u);
}

TEST(PETest, GteUnsigned) {
  PE pe(2, 2, Op::gte);
  pe.writeIn0(10);
  pe.writeIn1(10);
  pe.writeIn2(0);
  EXPECT_EQ(runPEUntilOutput(pe), 1u);
}

// Bitwise ops
TEST(PETest, BitwiseAndOrXor) {
  {
    PE pe(2, 2, Op::bit_and);
    pe.writeIn0(0b1100);
    pe.writeIn1(0b1010);
    pe.writeIn2(0);
    EXPECT_EQ(runPEUntilOutput(pe), 0b1000u);
  }
  {
    PE pe(2, 2, Op::bit_or);
    pe.writeIn0(0b1100);
    pe.writeIn1(0b1010);
    pe.writeIn2(0);
    EXPECT_EQ(runPEUntilOutput(pe), 0b1110u);
  }
  {
    PE pe(2, 2, Op::bit_xor);
    pe.writeIn0(0b1100);
    pe.writeIn1(0b1010);
    pe.writeIn2(0);
    EXPECT_EQ(runPEUntilOutput(pe), 0b0110u);
  }
}

// Floating point ops
TEST(PETest, FloatAdd) {
  PE pe(2, 2, Op::fadd);
  pe.writeIn0(f2u(1.5f));
  pe.writeIn1(f2u(2.25f));
  pe.writeIn2(0);
  float res = u2f(runPEUntilOutput(pe));
  EXPECT_FLOAT_EQ(res, 3.75f);
}

TEST(PETest, FloatSub) {
  PE pe(2, 2, Op::fsub);
  pe.writeIn0(f2u(5.0f));
  pe.writeIn1(f2u(2.5f));
  pe.writeIn2(0);
  float res = u2f(runPEUntilOutput(pe));
  EXPECT_FLOAT_EQ(res, 2.5f);
}

TEST(PETest, FloatMul) {
  PE pe(2, 2, Op::fmul);
  pe.writeIn0(f2u(3.0f));
  pe.writeIn1(f2u(-0.5f));
  pe.writeIn2(0);
  float res = u2f(runPEUntilOutput(pe));
  EXPECT_FLOAT_EQ(res, -1.5f);
}

TEST(PETest, FloatMac) {
  PE pe(2, 2, Op::fmac);
  pe.writeIn0(f2u(2.0f));
  pe.writeIn1(f2u(4.0f));
  pe.writeIn2(f2u(1.0f));
  float res = u2f(runPEUntilOutput(pe));
  EXPECT_FLOAT_EQ(res, 9.0f);  // 2*4+1
}

TEST(PETest, FloatComparison) {
  PE pe1(2, 2, Op::feq);
  pe1.writeIn0(f2u(1.0f));
  pe1.writeIn1(f2u(1.0f));
  pe1.writeIn2(0);
  EXPECT_EQ(runPEUntilOutput(pe1), 1u);

  PE pe2(2, 2, Op::flt);
  pe2.writeIn0(f2u(1.0f));
  pe2.writeIn1(f2u(2.0f));
  pe2.writeIn2(0);
  EXPECT_EQ(runPEUntilOutput(pe2), 1u);

  PE pe3(2, 2, Op::flte);
  pe3.writeIn0(f2u(2.0f));
  pe3.writeIn1(f2u(2.0f));
  pe3.writeIn2(0);
  EXPECT_EQ(runPEUntilOutput(pe3), 1u);
}

TEST(PETest, MixedOpsSequence) {
  // --- Case 1: unsigned add ---
  {
    PE pe(4, 4, Op::add);
    pe.writeIn0(10);
    pe.writeIn1(20);
    pe.writeIn2(0);
    EXPECT_EQ(runPEUntilOutput(pe), 30u);  // 10+20
  }

  // --- Case 2: signed smac ---
  {
    PE pe(4, 4, Op::smac);
    pe.writeIn0(uint32_t(-3));
    pe.writeIn1(uint32_t(7));
    pe.writeIn2(uint32_t(5));
    EXPECT_EQ(int32_t(runPEUntilOutput(pe)), -16);  // -3*7+5
  }

  // --- Case 3: float fmac ---
  {
    PE pe(4, 4, Op::fmac);
    pe.writeIn0(f2u(1.5f));
    pe.writeIn1(f2u(2.0f));
    pe.writeIn2(f2u(3.0f));
    float res = u2f(runPEUntilOutput(pe));
    EXPECT_FLOAT_EQ(res, 6.0f);  // 1.5*2 + 3
  }

  // --- Case 4: bitwise and/or/xor ---
  {
    PE pe_and(4, 4, Op::bit_and);
    pe_and.writeIn0(0b1100);
    pe_and.writeIn1(0b1010);
    pe_and.writeIn2(0);
    EXPECT_EQ(runPEUntilOutput(pe_and), 0b1000u);

    PE pe_or(4, 4, Op::bit_or);
    pe_or.writeIn0(0b1100);
    pe_or.writeIn1(0b1010);
    pe_or.writeIn2(0);
    EXPECT_EQ(runPEUntilOutput(pe_or), 0b1110u);

    PE pe_xor(4, 4, Op::bit_xor);
    pe_xor.writeIn0(0b1100);
    pe_xor.writeIn1(0b1010);
    pe_xor.writeIn2(0);
    EXPECT_EQ(runPEUntilOutput(pe_xor), 0b0110u);
  }

  // --- Case 5: float comparison ---
  {
    PE pe_eq(4, 4, Op::feq);
    pe_eq.writeIn0(f2u(2.5f));
    pe_eq.writeIn1(f2u(2.5f));
    pe_eq.writeIn2(0);
    EXPECT_EQ(runPEUntilOutput(pe_eq), 1u);

    PE pe_lt(4, 4, Op::flt);
    pe_lt.writeIn0(f2u(-1.0f));
    pe_lt.writeIn1(f2u(0.0f));
    pe_lt.writeIn2(0);
    EXPECT_EQ(runPEUntilOutput(pe_lt), 1u);

    PE pe_lte(4, 4, Op::flte);
    pe_lte.writeIn0(f2u(3.0f));
    pe_lte.writeIn1(f2u(3.0f));
    pe_lte.writeIn2(0);
    EXPECT_EQ(runPEUntilOutput(pe_lte), 1u);
  }
}
