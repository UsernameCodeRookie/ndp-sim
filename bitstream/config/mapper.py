from typing import Dict, List, Optional

class NodeGraph:
    """Singleton class representing nodes and their directed connections."""
    _instance: Optional["NodeGraph"] = None

    def __init__(self):
        self.nodes: List[str] = []
        self.connections: List[Dict[str, str]] = []

    @staticmethod
    def get() -> "NodeGraph":
        if NodeGraph._instance is None:
            NodeGraph._instance = NodeGraph()
        return NodeGraph._instance

    def add_node(self, name: str):
        if name not in self.nodes:
            self.nodes.append(name)

    def connect(self, src: str, dst: str):
        """Add a connection (src -> dst) if it does not already exist."""
        self.add_node(src)
        self.add_node(dst)
        connection = {"src": src, "dst": dst}
        if connection not in self.connections:
            self.connections.append(connection)

    def summary(self):
        """Print nodes, connections, and run mapper."""
        print("=== NodeGraph Summary ===")
        for c in self.connections:
            print(f"{c['src']} -> {c['dst']}")
        print(f"Total nodes: {len(self.nodes)}")
        print(f"Total connections: {len(self.connections)}")

