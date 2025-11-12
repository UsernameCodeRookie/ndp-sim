#ifndef COMPONENT_H
#define COMPONENT_H

#include <memory>
#include <string>
#include <unordered_map>

#include "port.h"
#include "scheduler.h"

namespace Architecture {

/**
 * @brief Component base class
 *
 * Base class for all architectural components
 */
class Component {
 public:
  Component(const std::string& name, EventDriven::EventScheduler& scheduler)
      : name_(name), scheduler_(scheduler), enabled_(true) {}

  virtual ~Component() = default;

  // Getters
  const std::string& getName() const { return name_; }
  EventDriven::EventScheduler& getScheduler() { return scheduler_; }
  bool isEnabled() const { return enabled_; }
  void setEnabled(bool enabled) { enabled_ = enabled; }

  // Port management
  /**
   * @brief Create and add a port in one step
   * @param port_name Name of the port
   * @param direction Direction of the port (INPUT or OUTPUT)
   */
  void addPort(const std::string& port_name, PortDirection direction) {
    auto port = std::make_shared<Port>(port_name, direction, this);
    addPortInternal(port);
  }

  std::shared_ptr<Port> getPort(const std::string& name) {
    auto it = ports_.find(name);
    return (it != ports_.end()) ? it->second : nullptr;
  }

  const std::unordered_map<std::string, std::shared_ptr<Port>>& getPorts()
      const {
    return ports_;
  }

  // Component lifecycle methods (to be overridden by derived classes)
  virtual void initialize() {}
  virtual void reset() {}

 protected:
  std::string name_;
  EventDriven::EventScheduler& scheduler_;
  bool enabled_;
  std::unordered_map<std::string, std::shared_ptr<Port>> ports_;

  // Internal helper for adding a port object directly
  void addPortInternal(std::shared_ptr<Port> port) {
    ports_[port->getName()] = port;
  }
};

}  // namespace Architecture

#endif  // COMPONENT_H
