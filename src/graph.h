#ifndef GRAPH_H
#define GRAPH_H

#include "common.h"
#include "node.h"
#include "protocol.h"

class Graph {
 public:
  using NodeId = std::string;

  Graph() = default;
  ~Graph() = default;

  // Add a node with identifier
  template <typename NodeT, typename... Args>
  std::shared_ptr<NodeT> addNode(const NodeId& id, Args&&... args) {
    if (nodes.count(id)) {
      throw std::runtime_error("Node with id '" + id + "' already exists.");
    }
    auto node = std::make_shared<NodeT>(std::forward<Args>(args)...);
    nodes[id] = node;
    return node;
  }

  // Connect two ports using Channel architecture
  template <typename T>
  void connect(Port<T>& src, Port<T>& dst) {
    // Create a new channel for each connection with default protocol
    auto protocol =
        std::make_shared<ProtocolValidReady<T, CastMode::SingleCast>>();
    auto channel = std::make_shared<Channel<T>>(protocol);

    // Find and bind source port
    std::shared_ptr<Port<T>> srcPtr = findPortPtr(src);
    if (srcPtr) {
      channel->bindSrc(srcPtr);
    }

    // Find and bind destination port
    std::shared_ptr<Port<T>> dstPtr = findPortPtr(dst);
    if (dstPtr) {
      channel->bindDst(dstPtr);
    }

    channels.push_back(channel);
  }

  // Connect two ports using Channel architecture with custom protocol
  template <typename T, typename Protocol>
  void connect(Port<T>& src, Port<T>& dst, std::shared_ptr<Protocol> protocol) {
    // Create a new channel for each connection with custom protocol
    auto channel = std::make_shared<Channel<T, Protocol>>(protocol);

    // Find and bind source port
    std::shared_ptr<Port<T>> srcPtr = findPortPtr(src);
    if (srcPtr) {
      channel->bindSrc(srcPtr);
    }

    // Find and bind destination port
    std::shared_ptr<Port<T>> dstPtr = findPortPtr(dst);
    if (dstPtr) {
      channel->bindDst(dstPtr);
    }

    channels.push_back(channel);
  }

  // Tick all nodes and channels
  void tick(std::shared_ptr<Debugger> dbg = nullptr) {
    // Tick nodes first
    for (auto& [id, node] : nodes) {
      node->tick(dbg);
    }

    // Tick all channels to propagate data
    for (auto& channel : channels) {
      channel->tick();
    }
  }

 private:
  // Helper method to find shared_ptr for a port reference
  template <typename T>
  std::shared_ptr<Port<T>> findPortPtr(Port<T>& port) {
    for (auto& [id, node] : nodes) {
      for (size_t i = 0; i < node->getPortCount(); ++i) {
        auto portPtr = std::dynamic_pointer_cast<Port<T>>(node->getPort(i));
        if (portPtr && portPtr.get() == &port) {
          return portPtr;
        }
      }
    }
    return nullptr;
  }

  std::unordered_map<NodeId, std::shared_ptr<NodeBase>> nodes;
  std::vector<std::shared_ptr<ChannelBase>> channels;
};

#endif  // GRAPH_H
