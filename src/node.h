#ifndef NODE_H
#define NODE_H

#include <debug.h>

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
};

class NodeBase {
 public:
  virtual ~NodeBase() = default;

  // Tick for one simulation cycle
  virtual void tick(std::shared_ptr<Debugger> dbg = nullptr) = 0;
};

template <typename T>
class Node3x1IO : public NodeBase {
 public:
  Node3x1IO() = default;

  Port<T> inPort0;
  Port<T> inPort1;
  Port<T> inPort2;
  Port<T> outPort;
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

#endif  // NODE_H