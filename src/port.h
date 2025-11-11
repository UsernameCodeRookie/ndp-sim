#ifndef PORT_H
#define PORT_H

#include <memory>
#include <string>

#include "packet.h"

namespace Architecture {

// Forward declarations
class Component;
class Connection;

/**
 * @brief Port direction enumeration
 */
enum class PortDirection { INPUT, OUTPUT, BIDIRECTIONAL };

/**
 * @brief Port class
 *
 * Represents an interface point on a component for data input/output
 */
class Port {
 public:
  Port(const std::string& name, PortDirection direction, Component* owner)
      : name_(name),
        direction_(direction),
        owner_(owner),
        connection_(nullptr),
        data_(nullptr) {}

  virtual ~Port() = default;

  // Getters
  const std::string& getName() const { return name_; }
  PortDirection getDirection() const { return direction_; }
  Component* getOwner() const { return owner_; }
  Connection* getConnection() const { return connection_; }

  // Connection management
  void setConnection(Connection* conn) { connection_ = conn; }
  bool isConnected() const { return connection_ != nullptr; }

  // Data operations
  void setData(std::shared_ptr<DataPacket> data) { data_ = data; }
  std::shared_ptr<DataPacket> getData() const { return data_; }
  bool hasData() const { return data_ != nullptr && data_->isValid(); }
  void clearData() { data_ = nullptr; }

  // Read data (for input ports)
  virtual std::shared_ptr<DataPacket> read() {
    auto temp = data_;
    data_ = nullptr;  // Consume data after reading
    return temp;
  }

  // Write data (for output ports)
  virtual void write(std::shared_ptr<DataPacket> data) { data_ = data; }

 protected:
  std::string name_;
  PortDirection direction_;
  Component* owner_;
  Connection* connection_;
  std::shared_ptr<DataPacket> data_;
};

}  // namespace Architecture

#endif  // PORT_H
