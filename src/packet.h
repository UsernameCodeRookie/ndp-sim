#ifndef PACKET_H
#define PACKET_H

#include <cstdint>
#include <memory>

namespace Architecture {

/**
 * @brief Data packet base class
 *
 * Represents data transmitted through ports and connections
 */
class DataPacket {
 public:
  DataPacket() : timestamp_(0), valid_(true) {}
  virtual ~DataPacket() = default;

  uint64_t getTimestamp() const { return timestamp_; }
  void setTimestamp(uint64_t ts) { timestamp_ = ts; }

  bool isValid() const { return valid_; }
  void setValid(bool valid) { valid_ = valid; }

  // Clone method for copying data
  virtual std::shared_ptr<DataPacket> clone() const = 0;

 protected:
  uint64_t timestamp_;  // Data creation timestamp
  bool valid_;          // Data validity flag
};

}  // namespace Architecture

#endif  // PACKET_H
