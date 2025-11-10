#ifndef CONNECTION_H
#define CONNECTION_H

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "port.h"
#include "scheduler.h"

namespace Architecture {

/**
 * @brief Connection class
 *
 * Connects source ports to destination ports, managing data transfer
 */
class Connection {
 public:
  Connection(const std::string& name, EventDriven::EventScheduler& scheduler)
      : name_(name), scheduler_(scheduler), latency_(0) {}

  virtual ~Connection() = default;

  // Getters
  const std::string& getName() const { return name_; }
  uint64_t getLatency() const { return latency_; }
  void setLatency(uint64_t latency) { latency_ = latency; }

  // Port management
  void addSourcePort(std::shared_ptr<Port> port) {
    src_ports_.push_back(port);
    port->setConnection(this);
  }

  void addDestinationPort(std::shared_ptr<Port> port) {
    dst_ports_.push_back(port);
    port->setConnection(this);
  }

  const std::vector<std::shared_ptr<Port>>& getSourcePorts() const {
    return src_ports_;
  }

  const std::vector<std::shared_ptr<Port>>& getDestinationPorts() const {
    return dst_ports_;
  }

  // Data transfer (to be overridden by derived classes)
  virtual void propagate() {}

 protected:
  std::string name_;
  EventDriven::EventScheduler& scheduler_;
  std::vector<std::shared_ptr<Port>> src_ports_;
  std::vector<std::shared_ptr<Port>> dst_ports_;
  uint64_t latency_;  // Connection latency in cycles
};

}  // namespace Architecture

#endif  // CONNECTION_H
