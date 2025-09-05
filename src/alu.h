#ifndef ALU_H
#define ALU_H

#include <common.h>

enum class Op {
  nop,
  add,
  sub,
  mul,
  mac,
  sum,
  eq,
  neq,
  max,
  sadd,
  ssub,
  smul,
  smac,
  ssum,
  joint_8,
  joint_16,
  mux,
  bit_and,
  bit_or,
  bit_not,
  bit_xor,
  ls,
  logic_rs,
  arith_rs,
  logic_and,
  logic_or,
  logic_not,
  lte,
  gte,
  lt,
  gt,

  // Floating point ops (bit-level float in uint32_t)
  fadd,
  fsub,
  fmul,
  fmac,
  fmas,
  fnmac,
  fnmas,
  feq,
  flte,
  flt,
  fcpys,
  fcpysn,
  fcpys_inv
};

#include <array>
#include <cstdint>

constexpr std::array<bool, 3> operandMaskFor(Op op) noexcept {
  switch (op) {
    // Two-input ops: src0, src1
    case Op::add:
    case Op::sub:
    case Op::mul:
    case Op::eq:
    case Op::neq:
    case Op::max:
    case Op::sadd:
    case Op::ssub:
    case Op::smul:
    case Op::joint_8:
    case Op::joint_16:
    case Op::bit_and:
    case Op::bit_or:
    case Op::bit_xor:
    case Op::ls:
    case Op::logic_rs:
    case Op::arith_rs:
    case Op::logic_and:
    case Op::logic_or:
    case Op::lte:
    case Op::gte:
    case Op::lt:
    case Op::gt:
    case Op::fadd:
    case Op::fsub:
    case Op::fmul:
    case Op::feq:
    case Op::flte:
    case Op::flt:
    case Op::fcpys:
    case Op::fcpysn:
    case Op::fcpys_inv:
    case Op::bit_not:
    case Op::logic_not:
    case Op::nop:
      return {true, true, false};  // true as operand used, false as unused

    // Three-input ops: src0, src1, src2
    case Op::mac:
    case Op::smac:
    case Op::sum:
    case Op::ssum:
    case Op::fmac:
    case Op::fmas:
    case Op::fnmac:
    case Op::fnmas:
    case Op::mux:
      return {true, true, true};

    default:
      return {false, false, false};
  }
}

struct AluInReg {
  uint32_t src0 = 0;
  uint32_t src1 = 0;
  uint32_t src2 = 0;
  Op opcode = Op::nop;
  bool valid = false;
  bool last = false;
};

struct AluOutReg {
  uint32_t value = 0;
  bool valid = false;
  bool last = false;
};

constexpr uint32_t getLatency(Op op) {
  switch (op) {
    case Op::add:
    case Op::sub:
    case Op::sadd:
    case Op::ssub:
    case Op::eq:
    case Op::neq:
    case Op::max:
    case Op::joint_8:
    case Op::joint_16:
    case Op::mux:
    case Op::bit_and:
    case Op::bit_or:
    case Op::bit_not:
    case Op::bit_xor:
    case Op::ls:
    case Op::logic_rs:
    case Op::arith_rs:
    case Op::logic_and:
    case Op::logic_or:
    case Op::logic_not:
    case Op::lte:
    case Op::gte:
    case Op::lt:
    case Op::gt:
    case Op::nop:
      return 1;

    case Op::mul:
    case Op::smul:
    case Op::mac:
    case Op::smac:
    case Op::fmac:
    case Op::fmas:
    case Op::fnmac:
    case Op::fnmas:
    case Op::fmul:
    case Op::fadd:
    case Op::fsub:
      return 2;

    case Op::sum:
    case Op::ssum:
    case Op::feq:
    case Op::flte:
    case Op::flt:
    case Op::fcpys:
    case Op::fcpysn:
    case Op::fcpys_inv:
      return 3;

    default:
      return 1;
  }
}

// Simple pipelined ALU
class ALU {
  struct Stage {
    uint32_t a, b, c;
    Op opcode;
    int remaining;
    bool last = false;
  };

 public:
  ALU() = default;

  // Accept one operation into the pipeline
  bool accept(const AluInReg& in) noexcept {
    if (!in.valid) return false;
    if ((int)pipeline.size() >= maxLatency) return false;
    pipeline.push_back({in.src0, in.src1, in.src2, in.opcode,
                        (int)getLatency(in.opcode), in.last});
    return true;
  }

  // Advance pipeline, return result if ready
  bool tick(AluOutReg& result) noexcept {
    for (auto& st : pipeline) st.remaining--;

    if (!pipeline.empty() && pipeline.front().remaining <= 0) {
      result.value = compute(pipeline.front());
      result.valid = true;
      result.last = pipeline.front().last;
      pipeline.pop_front();
      return true;
    }
    result.valid = false;
    return false;
  }

  bool full() const noexcept { return (int)pipeline.size() >= maxLatency; }

 private:
  // Core computation
  uint32_t compute(const Stage& st) const noexcept {
    auto a = st.a, b = st.b, c = st.c;

    switch (st.opcode) {
      case Op::nop:
        return b;
      case Op::add:
        return a + b;
      case Op::sub:
        return a - b;
      case Op::mul:
        return a * b;
      case Op::mac:
        return a * b + c;
      case Op::sum: {
        uint32_t acc = c;
        for (int i = 0; i < 4; i++)
          acc += ((a >> (i * 8)) & 0xFF) * ((b >> (i * 8)) & 0xFF);
        return acc;
      }
      case Op::eq:
        return a == b;
      case Op::neq:
        return a != b;
      case Op::max:
        return std::max(a, b);
      case Op::sadd:
        return int32_t(a) + int32_t(b);
      case Op::ssub:
        return int32_t(a) - int32_t(b);
      case Op::smul:
        return int32_t(a) * int32_t(b);
      case Op::smac:
        return int32_t(a) * int32_t(b) + int32_t(c);
      case Op::ssum: {
        int32_t acc = int32_t(c);
        for (int i = 0; i < 4; i++)
          acc += int8_t((a >> (i * 8)) & 0xFF) * int8_t((b >> (i * 8)) & 0xFF);
        return uint32_t(acc);
      }
      case Op::joint_8:
        return (a & 0xFF) | ((b & 0xFF) << 8);
      case Op::joint_16:
        return (a & 0xFFFF) | ((b & 0xFFFF) << 16);
      case Op::mux:
        return (c == 0) ? b : a;
      case Op::bit_and:
        return a & b;
      case Op::bit_or:
        return a | b;
      case Op::bit_not:
        return ~b;
      case Op::bit_xor:
        return a ^ b;
      case Op::ls:
        return a << b;
      case Op::logic_rs:
        return a >> b;
      case Op::arith_rs:
        return uint32_t(int32_t(a) >> b);
      case Op::logic_and:
        return (a && b);
      case Op::logic_or:
        return (a || b);
      case Op::logic_not:
        return !b;
      case Op::lte:
        return a <= b;
      case Op::gte:
        return a >= b;
      case Op::lt:
        return a < b;
      case Op::gt:
        return a > b;

      // Floating point ops (reinterpret bits)
      case Op::fadd:
        return f32_to_u32(u32_to_f32(a) + u32_to_f32(b));
      case Op::fsub:
        return f32_to_u32(u32_to_f32(a) - u32_to_f32(b));
      case Op::fmul:
        return f32_to_u32(u32_to_f32(a) * u32_to_f32(b));
      case Op::fmac:
        return f32_to_u32(u32_to_f32(a) * u32_to_f32(b) + u32_to_f32(c));
      case Op::fmas:
        return f32_to_u32(u32_to_f32(a) * u32_to_f32(b) - u32_to_f32(c));
      case Op::fnmac:
        return f32_to_u32(-u32_to_f32(a) * u32_to_f32(b) + u32_to_f32(c));
      case Op::fnmas:
        return f32_to_u32(-u32_to_f32(a) * u32_to_f32(b) - u32_to_f32(c));
      case Op::feq:
        return u32_to_f32(a) == u32_to_f32(b);
      case Op::flte:
        return u32_to_f32(a) <= u32_to_f32(b);
      case Op::flt:
        return u32_to_f32(a) < u32_to_f32(b);
      case Op::fcpys:
        return (a & 0x80000000) | (b & 0x7FFFFFFF);
      case Op::fcpysn:
        return (a & 0xFF800000) | (b & 0x007FFFFF);
      case Op::fcpys_inv:
        return (~a & 0x80000000) | (b & 0x7FFFFFFF);
    }
    return 0;
  }

  static float u32_to_f32(uint32_t x) {
    float f;
    std::memcpy(&f, &x, sizeof(f));
    return f;
  }
  static uint32_t f32_to_u32(float f) {
    uint32_t x;
    std::memcpy(&x, &f, sizeof(x));
    return x;
  }

  std::deque<Stage> pipeline;
  static constexpr int maxLatency = 10;  // conservative max
};

#endif  // ALU_H
