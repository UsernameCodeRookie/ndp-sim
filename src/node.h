#ifndef NODE_H
#define NODE_H

#include <debug.h>
#include <protocol.h>

struct IPort {
  virtual ~IPort() = default;
};

template <class T>
struct Port : public IPort {
  Port() = default;

  // Copy constructor
  Port(const Port& other) noexcept
      : data(other.data), occupied(other.occupied) {}

  // Assignment operator
  Port& operator=(const Port& other) noexcept {
    if (this != &other) {
      data = other.data;
      occupied = other.occupied;
    }
    return *this;
  }

  bool write(const T& val) noexcept {
    if (occupied) return false;
    data = val;
    occupied = true;
    return true;
  }

  bool read(T& out) noexcept {
    if (!occupied) return false;
    out = data;
    occupied = false;
    return true;
  }

  const T& peek() const noexcept { return data; }

  bool valid() const noexcept { return occupied; }

  // Backward compatibility method
  bool peek(T& out) const noexcept {
    if (!occupied) return false;
    out = data;
    return true;
  }

  T data{};
  bool occupied = false;
};

class ChannelBase {
 public:
  virtual ~ChannelBase() = default;
  virtual void tick() noexcept = 0;
  void attach(std::shared_ptr<IPort> p) { ports.push_back(std::move(p)); }

 protected:
  std::vector<std::shared_ptr<IPort>> ports;
};

template <typename T, typename Protocol = IProtocol<T, CastMode::SingleCast>>
class Channel : public ChannelBase {
 public:
  explicit Channel(std::shared_ptr<Protocol> proto = nullptr)
      : protocol(std::move(proto)) {}

  void bindSrc(std::shared_ptr<Port<T>> src) { srcs.push_back(std::move(src)); }
  void bindDst(std::shared_ptr<Port<T>> dst) { dsts.push_back(std::move(dst)); }

  void tick() noexcept override {
    if (protocol) protocol->execute(srcs, dsts);
  }

 private:
  std::vector<std::shared_ptr<Port<T>>> srcs;
  std::vector<std::shared_ptr<Port<T>>> dsts;
  std::shared_ptr<Protocol> protocol;
};

class NodeBase {
 public:
  virtual ~NodeBase() = default;

  // Tick for one simulation cycle - keeping backward compatibility
  virtual void tick(std::shared_ptr<Debugger> dbg = nullptr) = 0;

  // Get total port count
  size_t getPortCount() const noexcept { return ports_.size(); }

  // Get port by index (returns nullptr if index out of bounds)
  std::shared_ptr<IPort> getPort(size_t index) const noexcept {
    return index < ports_.size() ? ports_[index] : nullptr;
  }

  // Get typed port by index
  template <typename T>
  std::shared_ptr<Port<T>> getPort(size_t index) const noexcept {
    return std::dynamic_pointer_cast<Port<T>>(getPort(index));
  }

  // Add a port to the node
  void addPort(std::shared_ptr<IPort> port) noexcept { ports_.push_back(port); }

  // Get all ports
  const std::vector<std::shared_ptr<IPort>>& getPorts() const noexcept {
    return ports_;
  }

 protected:
  std::vector<std::shared_ptr<IPort>> ports_;
};

// Generic node with N ports of type T
template <size_t N, typename T>
class Node : virtual public NodeBase {
 public:
  Node() {
    // Create N ports and store them in the container
    for (size_t i = 0; i < N; ++i) {
      auto port = std::make_shared<Port<T>>();
      addPort(port);
      portPtrs[i] = port;
    }
  }

  // Get port by index (returns the shared_ptr version)
  std::shared_ptr<Port<T>> getPortPtr(size_t index) const noexcept {
    return index < N ? portPtrs[index] : nullptr;
  }

  // Direct access to port references
  Port<T>& getPort(size_t index) noexcept { return *portPtrs[index]; }
  const Port<T>& getPort(size_t index) const noexcept {
    return *portPtrs[index];
  }

 protected:
  std::array<std::shared_ptr<Port<T>>, N> portPtrs;
};

// Node type for 3 input + 1 output configuration
template <typename T>
class Node3x1IO : public Node<4, T> {
 public:
  Node3x1IO()
      : inPort0(this->getPort(0)),
        inPort1(this->getPort(1)),
        inPort2(this->getPort(2)),
        outPort(this->getPort(3)) {}

  // Direct port references
  Port<T>& inPort0;
  Port<T>& inPort1;
  Port<T>& inPort2;
  Port<T>& outPort;
};

#endif  // NODE_H