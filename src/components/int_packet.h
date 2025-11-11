#ifndef INT_PACKET_H
#define INT_PACKET_H

#include <memory>

#include "../packet.h"

/**
 * @brief Simple integer data packet
 */
class IntDataPacket : public Architecture::DataPacket {
 public:
  IntDataPacket(int value) : value_(value) {}

  int getValue() const { return value_; }
  void setValue(int value) { value_ = value; }

  std::shared_ptr<Architecture::DataPacket> clone() const override {
    auto cloned = std::make_shared<IntDataPacket>(value_);
    cloned->setTimestamp(timestamp_);
    cloned->setValid(valid_);
    return cloned;
  }

 private:
  int value_;
};

#endif  // INT_PACKET_H
