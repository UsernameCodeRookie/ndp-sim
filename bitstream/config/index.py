from typing import List, Optional, Dict

class NodeIndex:
    """Represents a placeholder for a node index, resolved later in batch."""

    _queue: List["NodeIndex"] = []
    _registry: Dict[str, "NodeIndex"] = {}
    _counter: int = 0

    def __new__(cls, node_name: str):
        # If a NodeIndex with the same name exists, return it
        if node_name in cls._registry:
            return cls._registry[node_name]
        instance = super().__new__(cls)
        return instance

    def __init__(self, node_name: str):
        # Avoid re-initialization if already registered
        if hasattr(self, "_initialized"):
            return
        self.node_name = node_name
        self._index: Optional[int] = None
        NodeIndex._queue.append(self)
        NodeIndex._registry[node_name] = self
        self._initialized = True

    @classmethod
    def resolve_all(cls):
        """Register all queued node names and assign unique indices."""
        for future in cls._queue:
            if future._index is None:
                future._index = cls._counter
                cls._counter += 1
        cls._queue.clear()

    @property
    def index(self) -> int:
        """Get the resolved node index, triggering resolution if needed."""
        if self._index is None:
            NodeIndex.resolve_all()
        return self._index

    def __int__(self):
        return self.index

    def __repr__(self):
        return f"<NodeIndex {self.node_name} -> {self._index}>"
