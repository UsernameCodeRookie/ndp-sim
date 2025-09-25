#ifndef GRAPH_H
#define GRAPH_H

#include "node.h"

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

  // Connect two ports using references (no raw pointers)
  template <typename T>
  void connect(Port<T>& src, Port<T>& dst) {
    connections.emplace_back(std::make_unique<Connection<T>>(src, dst));
  }

  // Tick all nodes and propagate data
  void tick(std::shared_ptr<Debugger> dbg = nullptr) {
    for (auto& [id, node] : nodes) {
      node->tick(dbg);
    }

    for (auto& c : connections) {
      c->propagate(dbg);
    }
  }

 private:
  std::unordered_map<NodeId, std::shared_ptr<NodeBase>> nodes;
  std::vector<std::unique_ptr<ConnectionBase>> connections;
};

#endif  // GRAPH_H
