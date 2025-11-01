from typing import Dict, List, Optional


class NodeGraph:
    """Singleton class representing the dataflow graph of all node connections."""

    _instance: Optional["NodeGraph"] = None  # internal singleton

    def __init__(self):
        """Initialize a new (normally hidden) NodeGraph instance."""
        self.nodes: List[str] = []
        self.connections: List[Dict[str, str]] = []

    @staticmethod
    def get() -> "NodeGraph":
        """Return the global singleton instance of NodeGraph."""
        if NodeGraph._instance is None:
            NodeGraph._instance = NodeGraph()
        return NodeGraph._instance

    def add_node(self, name: str):
        if name not in self.nodes:
            self.nodes.append(name)

    def connect(self, src: str, dst: str):
        self.add_node(src)
        self.add_node(dst)
        
        connection = {"src": src, "dst": dst}
        if connection not in self.connections:
            self.connections.append({"src": src, "dst": dst})

    def reset(self):
        """Clear all nodes and connections."""
        self.nodes.clear()
        self.connections.clear()

    def summary(self):
        print("=== NodeGraph Summary ===")
        for c in self.connections:
            print(f"{c['src']} -> {c['dst']}")
        print(f"Total nodes: {len(self.nodes)}")
        print(f"Total connections: {len(self.connections)}")
