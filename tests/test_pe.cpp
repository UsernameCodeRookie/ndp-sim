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
static uint32_t runPEUntilOutput(PE& pe, int maxCycles = 20) {
  Data d;
  for (int i = 0; i < maxCycles; ++i) {
    pe.tick();
    if (pe.outPort.read(d)) return d.data;
  }
  throw std::runtime_error("PE did not produce output in expected cycles");
}

// =====================================================
//                 TEST CASES
// =====================================================

// Unsigned integer arithmetic
TEST(PETest, AddUnsigned) {
  PE pe(2, 2, Op::add);
  pe.inPort0.write({10, false});
  pe.inPort1.write({20, false});
  pe.inPort2.write({0, false});
  EXPECT_EQ(runPEUntilOutput(pe), 30u);
}

TEST(PETest, SubUnsigned) {
  PE pe(2, 2, Op::sub);
  pe.inPort0.write({50, false});
  pe.inPort1.write({8, false});
  pe.inPort2.write({0, false});
  EXPECT_EQ(runPEUntilOutput(pe), 42u);
}

TEST(PETest, MulUnsigned) {
  PE pe(2, 2, Op::mul);
  pe.inPort0.write({6, false});
  pe.inPort1.write({7, false});
  pe.inPort2.write({0, false});
  EXPECT_EQ(runPEUntilOutput(pe), 42u);
}

TEST(PETest, MacUnsigned) {
  PE pe(2, 2, Op::mac);
  pe.inPort0.write({4, false});
  pe.inPort1.write({5, false});
  pe.inPort2.write({6, false});
  EXPECT_EQ(runPEUntilOutput(pe), 26u);  // 4*5+6
}

// Signed integer arithmetic
TEST(PETest, SaddSigned) {
  PE pe(2, 2, Op::sadd);
  pe.inPort0.write({uint32_t(-5), false});
  pe.inPort1.write({uint32_t(3), false});
  pe.inPort2.write({0, false});
  EXPECT_EQ(int32_t(runPEUntilOutput(pe)), -2);
}

TEST(PETest, SsubSigned) {
  PE pe(2, 2, Op::ssub);
  pe.inPort0.write({uint32_t(-5), false});
  pe.inPort1.write({uint32_t(-8), false});
  pe.inPort2.write({0, false});
  EXPECT_EQ(int32_t(runPEUntilOutput(pe)), 3);
}

TEST(PETest, SmacSigned) {
  PE pe(2, 2, Op::smac);
  pe.inPort0.write({uint32_t(-2), false});
  pe.inPort1.write({uint32_t(7), false});
  pe.inPort2.write({uint32_t(5), false});
  EXPECT_EQ(int32_t(runPEUntilOutput(pe)), -9);  // -2*7+5
}

// Logic / comparison
TEST(PETest, EqOperation) {
  PE pe(2, 2, Op::eq);
  pe.inPort0.write({42, false});
  pe.inPort1.write({42, false});
  pe.inPort2.write({0, false});
  EXPECT_EQ(runPEUntilOutput(pe), 1u);
}

TEST(PETest, NeqOperation) {
  PE pe(2, 2, Op::neq);
  pe.inPort0.write({42, false});
  pe.inPort1.write({7, false});
  pe.inPort2.write({0, false});
  EXPECT_EQ(runPEUntilOutput(pe), 1u);
}

TEST(PETest, LtUnsigned) {
  PE pe(2, 2, Op::lt);
  pe.inPort0.write({2, false});
  pe.inPort1.write({5, false});
  pe.inPort2.write({0, false});
  EXPECT_EQ(runPEUntilOutput(pe), 1u);
}

TEST(PETest, GteUnsigned) {
  PE pe(2, 2, Op::gte);
  pe.inPort0.write({10, false});
  pe.inPort1.write({10, false});
  pe.inPort2.write({0, false});
  EXPECT_EQ(runPEUntilOutput(pe), 1u);
}

// Bitwise ops
TEST(PETest, BitwiseAndOrXor) {
  {
    PE pe(2, 2, Op::bit_and);
    pe.inPort0.write({0b1100, false});
    pe.inPort1.write({0b1010, false});
    pe.inPort2.write({0, false});
    EXPECT_EQ(runPEUntilOutput(pe), 0b1000u);
  }
  {
    PE pe(2, 2, Op::bit_or);
    pe.inPort0.write({0b1100, false});
    pe.inPort1.write({0b1010, false});
    pe.inPort2.write({0, false});
    EXPECT_EQ(runPEUntilOutput(pe), 0b1110u);
  }
  {
    PE pe(2, 2, Op::bit_xor);
    pe.inPort0.write({0b1100, false});
    pe.inPort1.write({0b1010, false});
    pe.inPort2.write({0, false});
    EXPECT_EQ(runPEUntilOutput(pe), 0b0110u);
  }
}

// Floating point ops
TEST(PETest, FloatAdd) {
  PE pe(2, 2, Op::fadd);
  pe.inPort0.write({f2u(1.5f), false});
  pe.inPort1.write({f2u(2.25f), false});
  pe.inPort2.write({0, false});
  float res = u2f(runPEUntilOutput(pe));
  EXPECT_FLOAT_EQ(res, 3.75f);
}

TEST(PETest, FloatSub) {
  PE pe(2, 2, Op::fsub);
  pe.inPort0.write({f2u(5.0f), false});
  pe.inPort1.write({f2u(2.5f), false});
  pe.inPort2.write({0, false});
  float res = u2f(runPEUntilOutput(pe));
  EXPECT_FLOAT_EQ(res, 2.5f);
}

TEST(PETest, FloatMul) {
  PE pe(2, 2, Op::fmul);
  pe.inPort0.write({f2u(3.0f), false});
  pe.inPort1.write({f2u(-0.5f), false});
  pe.inPort2.write({0, false});
  float res = u2f(runPEUntilOutput(pe));
  EXPECT_FLOAT_EQ(res, -1.5f);
}

TEST(PETest, FloatMac) {
  PE pe(2, 2, Op::fmac);
  pe.inPort0.write({f2u(2.0f), false});
  pe.inPort1.write({f2u(4.0f), false});
  pe.inPort2.write({f2u(1.0f), false});
  float res = u2f(runPEUntilOutput(pe));
  EXPECT_FLOAT_EQ(res, 9.0f);  // 2*4+1
}

TEST(PETest, FloatComparison) {
  {
    PE pe(2, 2, Op::feq);
    pe.inPort0.write({f2u(1.0f), false});
    pe.inPort1.write({f2u(1.0f), false});
    pe.inPort2.write({0, false});
    EXPECT_EQ(runPEUntilOutput(pe), 1u);
  }
  {
    PE pe(2, 2, Op::flt);
    pe.inPort0.write({f2u(1.0f), false});
    pe.inPort1.write({f2u(2.0f), false});
    pe.inPort2.write({0, false});
    EXPECT_EQ(runPEUntilOutput(pe), 1u);
  }
  {
    PE pe(2, 2, Op::flte);
    pe.inPort0.write({f2u(2.0f), false});
    pe.inPort1.write({f2u(2.0f), false});
    pe.inPort2.write({0, false});
    EXPECT_EQ(runPEUntilOutput(pe), 1u);
  }
}

// Mixed sequence
TEST(PETest, MixedOpsSequence) {
  // --- Case 1: unsigned add ---
  {
    PE pe(4, 4, Op::add);
    pe.inPort0.write({10, false});
    pe.inPort1.write({20, false});
    pe.inPort2.write({0, false});
    EXPECT_EQ(runPEUntilOutput(pe), 30u);
  }

  // --- Case 2: signed smac ---
  {
    PE pe(4, 4, Op::smac);
    pe.inPort0.write({uint32_t(-3), false});
    pe.inPort1.write({uint32_t(7), false});
    pe.inPort2.write({uint32_t(5), false});
    EXPECT_EQ(int32_t(runPEUntilOutput(pe)), -16);
  }

  // --- Case 3: float fmac ---
  {
    PE pe(4, 4, Op::fmac);
    pe.inPort0.write({f2u(1.5f), false});
    pe.inPort1.write({f2u(2.0f), false});
    pe.inPort2.write({f2u(3.0f), false});
    float res = u2f(runPEUntilOutput(pe));
    EXPECT_FLOAT_EQ(res, 6.0f);
  }

  // --- Case 4: bitwise and/or/xor ---
  {
    PE pe_and(4, 4, Op::bit_and);
    pe_and.inPort0.write({0b1100, false});
    pe_and.inPort1.write({0b1010, false});
    pe_and.inPort2.write({0, false});
    EXPECT_EQ(runPEUntilOutput(pe_and), 0b1000u);

    PE pe_or(4, 4, Op::bit_or);
    pe_or.inPort0.write({0b1100, false});
    pe_or.inPort1.write({0b1010, false});
    pe_or.inPort2.write({0, false});
    EXPECT_EQ(runPEUntilOutput(pe_or), 0b1110u);

    PE pe_xor(4, 4, Op::bit_xor);
    pe_xor.inPort0.write({0b1100, false});
    pe_xor.inPort1.write({0b1010, false});
    pe_xor.inPort2.write({0, false});
    EXPECT_EQ(runPEUntilOutput(pe_xor), 0b0110u);
  }

  // --- Case 5: float comparison ---
  {
    PE pe_eq(4, 4, Op::feq);
    pe_eq.inPort0.write({f2u(2.5f), false});
    pe_eq.inPort1.write({f2u(2.5f), false});
    pe_eq.inPort2.write({0, false});
    EXPECT_EQ(runPEUntilOutput(pe_eq), 1u);

    PE pe_lt(4, 4, Op::flt);
    pe_lt.inPort0.write({f2u(-1.0f), false});
    pe_lt.inPort1.write({f2u(0.0f), false});
    pe_lt.inPort2.write({0, false});
    EXPECT_EQ(runPEUntilOutput(pe_lt), 1u);

    PE pe_lte(4, 4, Op::flte);
    pe_lte.inPort0.write({f2u(3.0f), false});
    pe_lte.inPort1.write({f2u(3.0f), false});
    pe_lte.inPort2.write({0, false});
    EXPECT_EQ(runPEUntilOutput(pe_lte), 1u);
  }
}
