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
  DataPacket() : timestamp(0), valid(true) {}
  virtual ~DataPacket() = default;

  // Clone method for copying data
  virtual std::shared_ptr<DataPacket> clone() const = 0;

  // Helper method for implementing clone in derived classes using variadic
  // templates
  template <typename DerivedType, typename... Args>
  std::shared_ptr<DataPacket> cloneImpl(Args&&... args) const {
    auto cloned = std::make_shared<DerivedType>(std::forward<Args>(args)...);
    cloned->timestamp = timestamp;
    cloned->valid = valid;
    return cloned;
  }

  // Helper method for classes with vector members that need copying
  // Usage: return cloneWithVectors<DerivedType>(constructor_args...,
  //        [](DerivedType* clone) { clone->vec_member = vec_member; });
  template <typename DerivedType, typename CopyFunc, typename... Args>
  std::shared_ptr<DataPacket> cloneWithVectors(CopyFunc copy_func,
                                               Args&&... args) const {
    auto cloned = std::make_shared<DerivedType>(std::forward<Args>(args)...);
    cloned->timestamp = timestamp;
    cloned->valid = valid;
    copy_func(cloned.get());
    return cloned;
  }

  uint64_t timestamp;  // Data creation timestamp
  bool valid;          // Data validity flag
};

/**
 * @brief Boolean data packet (for control signals like ready/valid)
 */
class BoolDataPacket : public DataPacket {
 public:
  explicit BoolDataPacket(bool val = false) : value(val) {}

  std::shared_ptr<DataPacket> clone() const override {
    return cloneImpl<BoolDataPacket>(value);
  }

  bool value;
};

/**
 * @brief Simple integer data packet
 */
class IntDataPacket : public DataPacket {
 public:
  explicit IntDataPacket(int val = 0) : value(val) {}

  std::shared_ptr<DataPacket> clone() const override {
    return cloneImpl<IntDataPacket>(value);
  }

  int value;
};

}  // namespace Architecture

#endif  // PACKET_H
