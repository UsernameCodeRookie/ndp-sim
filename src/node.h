#ifndef NODE_H
#define NODE_H

#include <debug.h>

#include <array>

template <typename T>
struct Port {
  T data{};
  bool valid = false;

  bool write(const T& d) noexcept {
    if (!valid) {
      data = d;
      valid = true;
      return true;
    }
    return false;
  }

  bool read(T& d) noexcept {
    if (!valid) return false;
    d = data;
    valid = false;
    return true;
  }

  bool peek(T& d) const noexcept {
    if (!valid) return false;
    d = data;
    return true;
  }
};

// Base class for all nodes
class NodeBase {
 public:
  virtual ~NodeBase() = default;

  // Tick for one simulation cycle
  virtual void tick(std::shared_ptr<Debugger> dbg = nullptr) = 0;
};

template <size_t N, typename I>
class NodeNI : virtual public NodeBase {
 public:
  NodeNI() = default;
  std::array<Port<I>, N> inPorts;
};

template <size_t M, typename O>
class NodeMO : virtual public NodeBase {
 public:
  NodeMO() = default;
  std::array<Port<O>, M> outPorts;
};

template <typename T>
class Node3x1IO : public NodeNI<3, T>, public NodeMO<1, T> {
 public:
  Node3x1IO() = default;

  Port<T>& inPort0 = this->inPorts[0];
  Port<T>& inPort1 = this->inPorts[1];
  Port<T>& inPort2 = this->inPorts[2];
  Port<T>& outPort = this->outPorts[0];
};

// Base class for all connections
class ConnectionBase {
 public:
  virtual ~ConnectionBase() = default;

  // propagate data from source to destination
  virtual void propagate(std::shared_ptr<Debugger> dbg = nullptr) noexcept = 0;
};

// Typed connection between two ports (reference version)
template <typename T>
class Connection : public ConnectionBase {
 public:
  Connection(Port<T>& _src, Port<T>& _dst) noexcept : src(_src), dst(_dst) {}

  void propagate(std::shared_ptr<Debugger> dbg = nullptr) noexcept override {
    if (!src.valid) return;  // nothing to propagate

    // Try to push data downstream
    if (dst.write(src.data)) {
      DEBUG_EVENT(dbg, "Connection", EventType::DataTransfer, {src.data.value},
                  "propagated");
      src.valid = false;  // consume source
    }
  }

 private:
  Port<T>& src;
  Port<T>& dst;
};

#endif  // NODE_H