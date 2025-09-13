#ifndef REG_H
#define REG_H

#include <node.h>

#define BANK_NUM 8
#define BANK_DEPTH 4

// address = [row_id(2 bits), bank_id(3 bits), offset(2 bits)]
struct Address {
  uint8_t value{0};
};

struct Data {
  uint32_t value{0};
};

class RegFile : public NodeNI<16, Address>, NodeMO<BANK_NUM, Data> {
 public:
  // One simulation cycle
  void tick(std::shared_ptr<Debugger> dbg = nullptr) override final {
    // Stage 3: read from registers
    for (size_t i = 0; i < BANK_NUM; ++i) {
      if (!outPorts[i].valid) {
      }
    }

    // Stage 2: write to registers

    // Stage 1: read from input ports
  }

  // Decode address into (row_id, bank_id, offset)
  std::tuple<uint8_t, uint8_t, uint8_t> decodeAddress(
      const Address &addr) const noexcept {
    uint8_t offset = addr.value & 0b11;           // low 2 bits
    uint8_t bank_id = (addr.value >> 2) & 0b111;  // next 3 bits
    uint8_t row_id = (addr.value >> 5) & 0b11;    // top 2 bits
    return {row_id, bank_id, offset};
  }

  // Read from register file
  Data read(const Address &addr) const noexcept {
    auto [row, bank, offset] = decodeAddress(addr);
    return registers[bank][row];
  }

  // Write into register file
  void write(const Address &addr, const Data &data) noexcept {
    auto [row, bank, offset] = decodeAddress(addr);
    registers[bank][row] = data;
  }

 private:
  std::array<std::array<Data, BANK_DEPTH>, BANK_NUM> registers;
};

#endif  // REG_H