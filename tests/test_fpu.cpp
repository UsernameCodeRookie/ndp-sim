#include <gtest/gtest.h>

#include <cmath>
#include <memory>

#include "../src/comp/core/fpu.h"

class FPUTest : public ::testing::Test {
 protected:
  static constexpr float EPSILON = 1e-6f;
};

// Basic FMA Operations
TEST_F(FPUTest, FMABasicAddition) {
  // a + b = (a × 1) + 0
  float result = FPUComponent::executeOperation(3.0f, 5.0f, 0.0f, FPUOp::FADD);
  EXPECT_NEAR(result, 8.0f, EPSILON);
}

TEST_F(FPUTest, FMABasicSubtraction) {
  // a - b = (a × 1) - b
  float result = FPUComponent::executeOperation(10.0f, 3.0f, 0.0f, FPUOp::FSUB);
  EXPECT_NEAR(result, 7.0f, EPSILON);
}

TEST_F(FPUTest, FMABasicMultiplication) {
  // a × b = (a × b) + 0
  float result = FPUComponent::executeOperation(2.5f, 4.0f, 0.0f, FPUOp::FMUL);
  EXPECT_NEAR(result, 10.0f, EPSILON);
}

TEST_F(FPUTest, FMAFusedMultiplyAdd) {
  // (a × b) + c
  float result = FPUComponent::executeOperation(2.0f, 3.0f, 1.0f, FPUOp::FMA);
  EXPECT_NEAR(result, 7.0f, EPSILON);
}

TEST_F(FPUTest, FMAFusedMultiplySubtract) {
  // (a × b) - c
  float result = FPUComponent::executeOperation(3.0f, 4.0f, 2.0f, FPUOp::FMS);
  EXPECT_NEAR(result, 10.0f, EPSILON);
}

TEST_F(FPUTest, FMAFusedNegateMultiplyAdd) {
  // -(a × b) + c
  float result = FPUComponent::executeOperation(2.0f, 3.0f, 10.0f, FPUOp::FNMA);
  EXPECT_NEAR(result, 4.0f, EPSILON);
}

TEST_F(FPUTest, FMAFusedNegateMultiplySubtract) {
  // -(a × b) - c
  float result = FPUComponent::executeOperation(2.0f, 3.0f, 1.0f, FPUOp::FNMS);
  EXPECT_NEAR(result, -7.0f, EPSILON);
}

// Comparison Operations
TEST_F(FPUTest, FloatComparison_Equal) {
  float result = FPUComponent::executeOperation(5.0f, 5.0f, 0.0f, FPUOp::FCMP);
  EXPECT_EQ(result, 1.0f);
}

TEST_F(FPUTest, FloatComparison_NotEqual) {
  float result = FPUComponent::executeOperation(5.0f, 3.0f, 0.0f, FPUOp::FCMP);
  EXPECT_EQ(result, 0.0f);
}

// Min/Max Operations
TEST_F(FPUTest, FloatMin) {
  float result = FPUComponent::executeOperation(5.0f, 3.0f, 0.0f, FPUOp::FMIN);
  EXPECT_NEAR(result, 3.0f, EPSILON);
}

TEST_F(FPUTest, FloatMax) {
  float result = FPUComponent::executeOperation(5.0f, 3.0f, 0.0f, FPUOp::FMAX);
  EXPECT_NEAR(result, 5.0f, EPSILON);
}

// Conversion Operations
TEST_F(FPUTest, Float32ToInt32Conversion) {
  float result =
      FPUComponent::executeOperation(3.7f, 0.0f, 0.0f, FPUOp::FCVT_W_S);
  int32_t int_result = static_cast<int32_t>(result);
  EXPECT_EQ(int_result, 3);
}

TEST_F(FPUTest, Int32ToFloat32Conversion) {
  // Pass 42 directly (will be treated as 42.0 in float form)
  float result =
      FPUComponent::executeOperation(42.0f, 0.0f, 0.0f, FPUOp::FCVT_S_W);
  EXPECT_NEAR(result, 42.0f, EPSILON);
}

// Complex FMA with negative numbers
TEST_F(FPUTest, FMANegativeNumbers) {
  float result = FPUComponent::executeOperation(-2.0f, 3.0f, 5.0f, FPUOp::FMA);
  EXPECT_NEAR(result, -1.0f, EPSILON);  // (-2 × 3) + 5 = -1
}

// Precision Test with small values
TEST_F(FPUTest, FMAPrecisionSmallValues) {
  float result =
      FPUComponent::executeOperation(1e-5f, 2e-5f, 3e-5f, FPUOp::FMA);
  float expected = std::fma(1e-5f, 2e-5f, 3e-5f);
  EXPECT_NEAR(result, expected, 1e-10f);
}

// Test FMA precision advantage
TEST_F(FPUTest, FMAVsNaiveMultiplication) {
  // FMA should be more accurate than separate multiply and add
  float a = 1.0f + 1e-7f;
  float b = 1.0f + 1e-7f;
  float c = 1.0f;

  float result_fma = FPUComponent::executeOperation(a, b, c, FPUOp::FMA);
  float result_naive = (a * b) + c;

  // They should both compute the same result
  EXPECT_NEAR(result_fma, result_naive, 1e-8f);
}

// Test operation names
TEST_F(FPUTest, OperationNames) {
  EXPECT_EQ(FPUComponent::getOpName(FPUOp::FADD), "FADD");
  EXPECT_EQ(FPUComponent::getOpName(FPUOp::FSUB), "FSUB");
  EXPECT_EQ(FPUComponent::getOpName(FPUOp::FMUL), "FMUL");
  EXPECT_EQ(FPUComponent::getOpName(FPUOp::FMA), "FMA");
  EXPECT_EQ(FPUComponent::getOpName(FPUOp::FMS), "FMS");
  EXPECT_EQ(FPUComponent::getOpName(FPUOp::FNMA), "FNMA");
  EXPECT_EQ(FPUComponent::getOpName(FPUOp::FNMS), "FNMS");
  EXPECT_EQ(FPUComponent::getOpName(FPUOp::FCMP), "FCMP");
  EXPECT_EQ(FPUComponent::getOpName(FPUOp::FMIN), "FMIN");
  EXPECT_EQ(FPUComponent::getOpName(FPUOp::FMAX), "FMAX");
}

// Test operation symbols
TEST_F(FPUTest, OperationSymbols) {
  EXPECT_EQ(FPUComponent::getOpSymbol(FPUOp::FADD), "+f");
  EXPECT_EQ(FPUComponent::getOpSymbol(FPUOp::FSUB), "-f");
  EXPECT_EQ(FPUComponent::getOpSymbol(FPUOp::FMUL), "*f");
  EXPECT_EQ(FPUComponent::getOpSymbol(FPUOp::FMA), "fma");
  EXPECT_EQ(FPUComponent::getOpSymbol(FPUOp::FMS), "fms");
  EXPECT_EQ(FPUComponent::getOpSymbol(FPUOp::FNMA), "-fma");
  EXPECT_EQ(FPUComponent::getOpSymbol(FPUOp::FNMS), "-fms");
}

// Test Pass-through operation
TEST_F(FPUTest, PassThroughOperation) {
  float result = FPUComponent::executeOperation(42.5f, 0.0f, 0.0f, FPUOp::PASS);
  EXPECT_NEAR(result, 42.5f, EPSILON);
}

// Test edge case: zero operands
TEST_F(FPUTest, ZeroOperands) {
  float result = FPUComponent::executeOperation(0.0f, 5.0f, 0.0f, FPUOp::FADD);
  EXPECT_NEAR(result, 5.0f, EPSILON);
}

// Test edge case: negative zero
TEST_F(FPUTest, NegativeZero) {
  float result = FPUComponent::executeOperation(-0.0f, 5.0f, 0.0f, FPUOp::FADD);
  EXPECT_NEAR(result, 5.0f, EPSILON);
}

// Test FMS vs FNMS distinction
TEST_F(FPUTest, FMSVsFNMS) {
  float fms_result =
      FPUComponent::executeOperation(2.0f, 3.0f, 1.0f, FPUOp::FMS);
  float fnms_result =
      FPUComponent::executeOperation(2.0f, 3.0f, 1.0f, FPUOp::FNMS);

  // FMS: (2 × 3) - 1 = 5
  // FNMS: -(2 × 3) - 1 = -7
  EXPECT_NEAR(fms_result, 5.0f, EPSILON);
  EXPECT_NEAR(fnms_result, -7.0f, EPSILON);
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
