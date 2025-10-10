#ifndef NODE_H
#define NODE_H

#include <debug.h>

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

template <typename SrcT, typename DstT,
          typename Converter = std::function<DstT(const SrcT&)>>
class Channel : public ChannelBase {
 public:
  Channel(Converter cvt = nullptr) : convert(std::move(cvt)) {}

  // Bind ports explicitly
  void bindSrc(std::shared_ptr<Port<SrcT>> src) {
    srcs.push_back(std::move(src));
  }
  void bindDst(std::shared_ptr<Port<DstT>> dst) {
    dsts.push_back(std::move(dst));
  }

  // Access to sources for graph management
  const std::vector<std::shared_ptr<Port<SrcT>>>& getSources() const noexcept {
    return srcs;
  }

  // Access to destinations for graph management
  const std::vector<std::shared_ptr<Port<DstT>>>& getDestinations()
      const noexcept {
    return dsts;
  }

  // Simulation tick
  void tick() noexcept override {
    // Iterate all sources
    for (auto& s : srcs) {
      if (!s || !s->valid()) continue;

      SrcT sdata{};
      if (!s->read(sdata)) continue;

      DstT converted = convert ? convert(sdata) : implicitConvert(sdata);

      // Broadcast to all destinations
      for (auto& d : dsts) {
        if (d) d->write(converted);
      }
    }
  }

 private:
  static DstT implicitConvert(const SrcT& s) {
    if constexpr (std::is_convertible_v<SrcT, DstT>) {
      return static_cast<DstT>(s);
    } else {
      static_assert(sizeof(SrcT) == 0,
                    "No implicit conversion available between SrcT and DstT");
    }
  }

  std::vector<std::shared_ptr<Port<SrcT>>> srcs;
  std::vector<std::shared_ptr<Port<DstT>>> dsts;
  Converter convert;
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

// Backward compatibility aliases for reg.h
template <size_t N, typename I>
class NodeNI : public Node<N, I> {
 public:
  NodeNI() = default;
};

template <size_t M, typename O>
class NodeMO : public Node<M, O> {
 public:
  NodeMO() = default;
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