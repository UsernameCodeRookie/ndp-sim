#ifndef NODE_H
#define NODE_H

#include <debug.h>

// Utility to convert various types to uint32_t for logging
template <typename T>
struct ToUint32 {
  static uint32_t convert(const T& val) { return static_cast<uint32_t>(val); }
};

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

  uint32_t toUint32() const noexcept { return ToUint32<T>::convert(data); }
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
  virtual void propagate(std::shared_ptr<Debugger> dbg = nullptr) noexcept = 0;
};

// Connection strategy types
enum class StrategyType { SingleCast, Broadcast };

// Single-cast strategy for connections
template <typename T>
struct SingleCastStrategy {
  static void propagate(Port<T>* src, std::vector<Port<T>*>& dsts,
                        std::shared_ptr<Debugger> dbg = nullptr) {
    if (!src || !src->valid) return;

    for (auto* dst : dsts) {
      if (dst->write(src->data)) {
        src->valid = false;
        DEBUG_EVENT(dbg, "->", EventType::DataTransfer, {0}, "singlecast");
        break;
      }
    }
  }
};

// Broadcast strategy for connections
template <typename T>
struct BroadcastStrategy {
  static void propagate(Port<T>* src, std::vector<Port<T>*>& dsts,
                        std::shared_ptr<Debugger> dbg = nullptr) {
    if (!src || !src->valid) return;

    for (auto* dst : dsts) {
      dst->write(src->data);
      DEBUG_EVENT(dbg, "->", EventType::DataTransfer, {0}, "broadcast");
    }

    src->valid = false;
  }
};

// Connection
template <typename T>
class Connection : public ConnectionBase {
 public:
  Connection(Port<T>& src, Port<T>& dst, StrategyType s)
      : src(&src), strategy(s) {
    dsts.push_back(&dst);
  }

  void addDst(Port<T>& dst) { dsts.push_back(&dst); }

  void propagate(std::shared_ptr<Debugger> dbg = nullptr) noexcept override {
    switch (strategy) {
      case StrategyType::SingleCast:
        SingleCastStrategy<T>::propagate(src, dsts, dbg);
        break;
      case StrategyType::Broadcast:
        BroadcastStrategy<T>::propagate(src, dsts, dbg);
        break;
    }
  }

  Port<T>& getSrc() { return *src; }

 private:
  Port<T>* src;
  std::vector<Port<T>*> dsts;
  StrategyType strategy;
};

#endif  // NODE_H