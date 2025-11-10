#ifndef MEMORY_H
#define MEMORY_H

#include <cstdint>
#include <iostream>
#include <string>

#include "../event_base.h"
#include "../scheduler.h"

/**
 * @brief Custom event example: Memory access event
 */
class MemoryAccessEvent : public EventDriven::Event {
 public:
  MemoryAccessEvent(uint64_t time, uint64_t address, bool is_read)
      : EventDriven::Event(time, 0, EventDriven::EventType::MEMORY_ACCESS,
                           is_read ? "MemoryRead" : "MemoryWrite"),
        address_(address),
        is_read_(is_read) {}

  void execute(EventDriven::EventScheduler&) override {
    std::cout << "  " << (is_read_ ? "Reading from" : "Writing to")
              << " address 0x" << std::hex << address_ << std::dec << std::endl;
  }

 private:
  uint64_t address_;
  bool is_read_;
};

#endif  // MEMORY_H
