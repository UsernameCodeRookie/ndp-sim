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

  // Connect two ports using references and specify strategy
  template <typename T>
  void connect(Port<T>& src, Port<T>& dst,
               StrategyType strategy = StrategyType::Broadcast) {
    // Check if a connection from this src already exists
    auto it = std::find_if(connections.begin(), connections.end(),
                           [&src](const std::unique_ptr<ConnectionBase>& c) {
                             auto p = dynamic_cast<Connection<T>*>(c.get());
                             return p && &(p->getSrc()) == &src;
                           });

    if (it != connections.end()) {
      // src already has a connection, just add dst
      auto p = dynamic_cast<Connection<T>*>(it->get());
      if (p) {
        p->addDst(dst);
        return;
      }
    }

    // Create a new connection
    connections.emplace_back(
        std::make_unique<Connection<T>>(src, dst, strategy));
  }

  // Tick all nodes and propagate data
  void tick(std::shared_ptr<Debugger> dbg = nullptr) {
    // Tick nodes first
    for (auto& [id, node] : nodes) {
      node->tick(dbg);
    }

    // Propagate data along connections
    for (auto& c : connections) {
      c->propagate(dbg);
    }
  }

 private:
  std::unordered_map<NodeId, std::shared_ptr<NodeBase>> nodes;
  std::vector<std::unique_ptr<ConnectionBase>> connections;
};

#endif  // GRAPH_H
