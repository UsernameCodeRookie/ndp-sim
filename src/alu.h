#ifndef ALU_H
#define ALU_H

#include <common.h>

// Simple fixed opcodes for ALU
enum class Op { Add, Sub, Mul, Div };

// Input register: holds one pair of AluInRegs
struct AluInReg {
  int32_t src0 = 0, src1 = 0;
  bool valid = false;
};

// Output register: holds one AluOutReg
struct AluOutReg {
  int32_t value = 0;
  bool valid = false;
};

// Represents one op flowing inside pipeline
class ALU {
  struct Stage {
    int32_t a;
    int32_t b;
    int remaining;
  };

 public:
  ALU(Op op, int latency) noexcept : opcode(op), latency(latency) {}

  // feed one op into pipeline (if space)
  bool accept(const AluInReg& op) noexcept {
    if ((int)pipeline.size() >= latency) return false;
    pipeline.push_back({op.src0, op.src1, latency});
    return true;
  }

  // advance pipeline, return one AluOutReg if ready
  bool tick(AluOutReg& result) noexcept {
    for (auto& st : pipeline) st.remaining--;

    if (!pipeline.empty() && pipeline.front().remaining <= 0) {
      result.value = compute(pipeline.front().a, pipeline.front().b);
      result.valid = true;
      pipeline.pop_front();
      return true;
    }

    return false;
  }

  bool full() const noexcept { return (int)pipeline.size() >= latency; }

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

  Op opcode;
  int latency;
  std::deque<Stage> pipeline;
};

#endif  // ALU_H
