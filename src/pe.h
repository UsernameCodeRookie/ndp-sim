#ifndef PE_H
#define PE_H

#include <alu.h>
#include <buffer.h>

template <typename T>
struct Port {
  T value{};
  bool valid = false;
};

// Processing Element: inPorts -> inBuffers -> ALU -> outBuffer -> outPort
class PE {
 public:
  PE(size_t inCap, size_t outCap, Op opcode, int latency) noexcept
      : inBuffer0(inCap),
        inBuffer1(inCap),
        outBuffer(outCap),
        alu(opcode, latency) {}

  // One simulation cycle
  void tick() {
    // Stage 4: update output port from buffer if free
    if (!outPort.valid && !outBuffer.empty()) {
      outBuffer.pop(outPort.value);
      outPort.valid = true;
    }

    // Stage 3: collect ALU result into output buffer
    AluOutReg r;
    if (alu.tick(r) && r.valid && !outBuffer.full()) {
      outBuffer.push(r.value);
    }

    // Stage 2: if both ports valid and ALU ready, send operand
    if (!inBuffer0.empty() && !inBuffer1.empty() && !alu.full()) {
      int32_t src0, src1;
      inBuffer0.pop(src0);
      inBuffer1.pop(src1);
      AluInReg op{src0, src1, true};
      alu.accept(op);
    }

    // Stage 1: pull from input ports into input buffers
    if (inPort0.valid && !inBuffer0.full()) {
      inBuffer0.push(inPort0.value);
      inPort0.valid = false;
    }
    if (inPort1.valid && !inBuffer1.full()) {
      inBuffer1.push(inPort1.value);
      inPort1.valid = false;
    }
  }

  // Write to inPort0
  void writeIn0(int32_t val) noexcept {
    if (!inPort0.valid) {
      inPort0.value = val;
      inPort0.valid = true;
    }
  }

  // Write to inPort1
  void writeIn1(int32_t val) noexcept {
    if (!inPort1.valid) {
      inPort1.value = val;
      inPort1.valid = true;
    }
  }

  // Read from outPort
  bool readOut(int32_t& outVal) noexcept {
    if (outPort.valid) {
      outVal = outPort.value;
      outPort.valid = false;
      return true;
    }
    return false;
  }

 private:
  Buffer<int32_t> inBuffer0;
  Buffer<int32_t> inBuffer1;
  Buffer<int32_t> outBuffer;

  Port<int32_t> inPort0;
  Port<int32_t> inPort1;
  Port<int32_t> outPort;
  ALU alu;
};

#endif  // PE_H