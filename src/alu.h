#ifndef ALU_H
#define ALU_H

#include <common.h>

#include <deque>

// Simple fixed opcodes for ALU
enum class Op { Add, Sub, Mul, Div };

// Input register: holds one pair of operands
struct InReg {
  bool valid = false;
  int32_t src0 = 0, src1 = 0;
};

// Output register: holds one result
struct OutReg {
  bool valid = false;
  int32_t value = 0;
};

// Represents one op flowing inside pipeline
struct Stage {
  int32_t a;
  int32_t b;
  int remaining;
};

class ALU {
 public:
  ALU(InReg& in, OutReg& out, Op opcode, int latency) noexcept
      : in(in), out(out), opcode(opcode), latency(latency) {}

  void tick() noexcept {
    // Advance all pipeline ops
    for (auto& st : pipeline) {
      st.remaining--;
    }

    // Check if front op is done
    if (!pipeline.empty() && pipeline.front().remaining <= 0) {
      if (!out.valid) {
        auto done = pipeline.front();
        out.value = compute(done.a, done.b);
        out.valid = true;
        pipeline.pop_front();
      }
    }

    // Accept new op if input is valid
    if (in.valid) {
      if ((int)pipeline.size() < latency) {
        pipeline.push_back({in.src0, in.src1, latency});
        in.valid = false;
      }
    }
  }

 private:
  int32_t compute(int32_t a, int32_t b) const noexcept {
    switch (opcode) {
      case Op::Add:
        return a + b;
      case Op::Sub:
        return a - b;
      case Op::Mul:
        return a * b;
      case Op::Div:
        return b != 0 ? a / b : 0;
    }
    return 0;
  }

  InReg& in;
  OutReg& out;
  Op opcode;
  int latency;
  std::deque<Stage> pipeline;
};

#endif  // ALU_H
