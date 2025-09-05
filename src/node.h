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

  // Optional generic container of all ports for iteration
  std::vector<Port<T>*> ports() {
    return {&inPort0, &inPort1, &inPort2, &outPort};
  }
};

#endif  // NODE_H