#ifndef PE_H
#define PE_H

#include <alu.h>
#include <buffer.h>
#include <node.h>

struct Data {
  uint32_t value{};
  bool last = false;
};

// Processing Element: inPorts -> inBuffers -> ALU -> outBuffer -> outPort
class PE : public Node3x1IO<Data> {
  struct FeedbackState {
    Data data{};
    bool valid = false;
  };

 public:
  PE(size_t inCap, size_t outCap, Op op, bool transout = false) noexcept
      : inBuffer0(inCap),
        inBuffer1(inCap),
        inBuffer2(inCap),
        outBuffer(outCap),
        opcode(op),
        transout(transout),
        operand_mask(operandMaskFor(op)) {}

  // One simulation cycle
  void tick(std::shared_ptr<Debugger> dbg = nullptr) override final {
    // Stage 4: decide output
    if (!outPort.valid) {
      if (!transout) {
        // Normal PE: output whenever outBuffer has data
        if (!outBuffer.empty()) {
          Data temp{};
          outBuffer.pop(temp);
          outPort.write(temp);
          DEBUG_EVENT(dbg, "PE", EventType::DataTransfer, {temp.value},
                      "Normal output");
        }
      } else {
        // Transout: only output if last_flag is set
        if (last_flag) {
          Data temp{};
          if (feedback_state.valid) temp = feedback_state.data;
          temp.last = true;  // ensure last flag
          outPort.write(temp);
          last_flag = false;
          has_accum_started = false;  // reset for next accumulation
          DEBUG_EVENT(dbg, "PE", EventType::DataTransfer, {temp.value},
                      "Transout output");

          feedback_state.valid = false;
        }
      }
    }

    // Stage 3: collect ALU result into output buffer
    AluOutReg r;
    if (alu.tick(r) && r.valid && !outBuffer.full()) {
      outBuffer.push({r.value, r.last});
      DEBUG_EVENT(dbg, "PE", EventType::StateChange, {r.value},
                  "ALU result pushed to outBuffer");

      if (transout && r.last) {
        last_flag = true;  // mark that last output is produced
      }
    }

    // Stage 2: if all input buffers ready and ALU not full, send operands
    if (allOpReady() && !alu.full()) {
      Data src0{0, false}, src1{0, false}, src2{0, false};
      if (operand_mask[0]) inBuffer0.pop(src0);
      if (operand_mask[1]) inBuffer1.pop(src1);

      // In transout mode, use outBuffer as src2
      if (transout) {
        if (!has_accum_started) {
          src2 = {0, false};  // initial accumulation
        } else if (feedback_state.valid) {
          src2 = feedback_state.data;  // previous tick feedback
          feedback_state.valid = false;
        }
      } else if (operand_mask[2]) {
        inBuffer2.pop(src2);
      }

      bool transout_last = transout && src0.last && src1.last;

      AluInReg opReg{src0.value, src1.value, src2.value,
                     opcode,     true,       transout_last};
      alu.accept(opReg);
      if (transout && !has_accum_started) {
        has_accum_started = true;
      }

      auto _ = {src0.value, src1.value, src2.value};
      DEBUG_EVENT(dbg, "PE", EventType::StateChange, _,
                  "Operands accepted by ALU");
    }

    // Stage 1: pull from input ports into input buffers
    if (inPort0.valid && !inBuffer0.full()) {
      inBuffer0.push(inPort0.data);
      DEBUG_EVENT(dbg, "PE", EventType::DataTransfer, {inPort0.data.value},
                  "inPort0 -> inBuffer0");
      inPort0.valid = false;
    }
    if (inPort1.valid && !inBuffer1.full()) {
      inBuffer1.push(inPort1.data);
      DEBUG_EVENT(dbg, "PE", EventType::DataTransfer, {inPort1.data.value},
                  "inPort1 -> inBuffer1");
      inPort1.valid = false;
    }
    if (!transout && inPort2.valid && !inBuffer2.full()) {
      inBuffer2.push(inPort2.data);
      DEBUG_EVENT(dbg, "PE", EventType::DataTransfer, {inPort2.data.value},
                  "inPort2 -> inBuffer2");
      inPort2.valid = false;
    }

    // Virtual Stage: update feedback state from out buffer
    if (transout && !feedback_state.valid && !outBuffer.empty()) {
      Data temp{};
      outBuffer.pop(temp);  // peek without removing
      feedback_state.data = temp;
      feedback_state.valid = true;
      DEBUG_EVENT(dbg, "PE", EventType::StateChange, {temp.value},
                  "Feedback state updated");
    }
  }

  // Check if all operands are ready
  bool allOpReady() const noexcept {
    bool ready0 = !operand_mask[0] || !inBuffer0.empty();
    bool ready1 = !operand_mask[1] || !inBuffer1.empty();
    bool ready2 = true;  // in transout mode, port2 is fed by outBuffer
    if (!transout) {
      ready2 = !operand_mask[2] || !inBuffer2.empty();
    } else {
      if (!has_accum_started) {
        ready2 = true;  // allow first operation without src2
      } else {
        ready2 = feedback_state.valid;  // need previous output
      }
    }
    return ready0 && ready1 && ready2;
  }

 private:
  Buffer<Data> inBuffer0;
  Buffer<Data> inBuffer1;
  Buffer<Data> inBuffer2;
  Buffer<Data> outBuffer;

  ALU alu;
  Op opcode;                         // fixed opcode per PE
  bool transout = false;             // transout mode flag
  std::array<bool, 3> operand_mask;  // which operands are used

  // required for transout mode
  bool last_flag = false;
  bool has_accum_started = false;

  // simulator only
  FeedbackState feedback_state;
};

#endif  // PE_H
