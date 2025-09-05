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
  PE(size_t inCap, size_t outCap, Op op) noexcept
      : inBuffer0(inCap),
        inBuffer1(inCap),
        inBuffer2(inCap),
        outBuffer(outCap),
        opcode(op),
        operand_mask(operandMaskFor(op)) {}

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

    // Stage 2: if all input buffers ready and ALU not full, send operands
    if (allAluOperandsReady() && !alu.full()) {
      uint32_t src0, src1, src2;
      if (operand_mask[0]) inBuffer0.pop(src0);
      if (operand_mask[1]) inBuffer1.pop(src1);
      if (operand_mask[2]) inBuffer2.pop(src2);
      AluInReg opReg{src0, src1, src2, opcode, true};
      alu.accept(opReg);
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
    if (inPort2.valid && !inBuffer2.full()) {
      inBuffer2.push(inPort2.value);
      inPort2.valid = false;
    }
  }

  bool allAluOperandsReady() const noexcept {
    return (!operand_mask[0] || !inBuffer0.empty()) &&
           (!operand_mask[1] || !inBuffer1.empty()) &&
           (!operand_mask[2] || !inBuffer2.empty());
  }

  // Write to inPort0
  void writeIn0(uint32_t val) noexcept {
    if (!inPort0.valid) {
      inPort0.value = val;
      inPort0.valid = true;
    }
  }

  // Write to inPort1
  void writeIn1(uint32_t val) noexcept {
    if (!inPort1.valid) {
      inPort1.value = val;
      inPort1.valid = true;
    }
  }

  // Write to inPort2
  void writeIn2(uint32_t val) noexcept {
    if (!inPort2.valid) {
      inPort2.value = val;
      inPort2.valid = true;
    }
  }

  // Read from outPort
  bool readOut(uint32_t& outVal) noexcept {
    if (outPort.valid) {
      outVal = outPort.value;
      outPort.valid = false;
      return true;
    }
    return false;
  }

 private:
  Buffer<uint32_t> inBuffer0;
  Buffer<uint32_t> inBuffer1;
  Buffer<uint32_t> inBuffer2;
  Buffer<uint32_t> outBuffer;

  Port<uint32_t> inPort0;
  Port<uint32_t> inPort1;
  Port<uint32_t> inPort2;
  Port<uint32_t> outPort;

  ALU alu;
  Op opcode;                         // fixed opcode per PE
  std::array<bool, 3> operand_mask;  // which operands are used
};

#endif  // PE_H
