#ifndef PACKET_H
#define PACKET_H

#include <cstdint>
#include <memory>

namespace Architecture {

/**
 * @brief Data packet base class
 *
 * Represents data transmitted through ports and connections
 *
 * Subclasses can use the convenience method cloneImpl<DerivedType>(args...)
 * to implement clone() without boilerplate. Example:
 *
 * class MyPacket : public DataPacket {
 *   int value_;
 * public:
 *   MyPacket(int v) : value_(v) {}
 *   std::shared_ptr<DataPacket> clone() const override {
 *     return cloneImpl<MyPacket>(value_);
 *   }
 * };
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
  /**
   * @brief Helper method for implementing clone in derived classes
   *
   * Usage in derived class:
   *   std::shared_ptr<DataPacket> clone() const override {
   *     return cloneImpl<DerivedType>(arg1, arg2, ...);
   *   }
   *
   * @tparam DerivedType The derived class type
   * @tparam Args Constructor argument types
   * @param args Arguments to pass to derived class constructor
   * @return Cloned packet with timestamp and valid flag copied
   */
  template <typename DerivedType, typename... Args>
  std::shared_ptr<DataPacket> cloneImpl(Args&&... args) const {
    auto cloned = std::make_shared<DerivedType>(std::forward<Args>(args)...);
    cloned->setTimestamp(timestamp_);
    cloned->setValid(valid_);
    return cloned;
  }

  uint64_t timestamp_;  // Data creation timestamp
  bool valid_;          // Data validity flag
};

/**
 * @brief Boolean data packet (for control signals like ready/valid)
 */
class BoolDataPacket : public DataPacket {
 public:
  explicit BoolDataPacket(bool value) : value_(value) {}

  bool getValue() const { return value_; }
  void setValue(bool value) { value_ = value; }

  std::shared_ptr<DataPacket> clone() const override {
    return cloneImpl<BoolDataPacket>(value_);
  }

 private:
  bool value_;
};

/**
 * @brief Simple integer data packet
 */
class IntDataPacket : public DataPacket {
 public:
  IntDataPacket(int value) : value_(value) {}

  int getValue() const { return value_; }
  void setValue(int value) { value_ = value; }

  std::shared_ptr<DataPacket> clone() const override {
    return cloneImpl<IntDataPacket>(value_);
  }

 private:
  int value_;
};

}  // namespace Architecture

#endif  // PACKET_H
