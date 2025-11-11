from typing import List, Optional, Dict
from bitstream.config.mapper import NodeGraph

"""
TODO: Use mapper to place node instead of direct NodeIndex creation in config modules.
"""

class NodeIndex:
    """Represents a placeholder for a node index, resolved later in batch."""

    _queue: List["NodeIndex"] = []
    _registry: Dict[str, "NodeIndex"] = {}
    _counter: int = 0
    _resolved: bool = False

    def __new__(cls, name: str):
        # If a NodeIndex with the same name exists, return it
        if name in cls._registry:
            return cls._registry[name]
        instance = super().__new__(cls)
        return instance

    def __init__(self, name: str):
        # Avoid re-initialization if already registered
        if hasattr(self, "_initialized"):
            return
        self.node_name = name
        self._index: Optional[int] = None
        self._physical_id: Optional[int] = None  # Physical hardware resource ID from mapper
        NodeIndex._queue.append(self)
        NodeIndex._registry[name] = self
        self._initialized = True
        
        NodeGraph.get().add_node(name)
    
    @classmethod
    def resolve_all(cls):
        """
        Resolve all node indices using the mapper to get physical resource IDs.
        This must be called after NodeGraph.allocate_resources() or NodeGraph.search_mapping().
        """
        if cls._resolved:
            return
            
        graph = NodeGraph.get()
        mapper = graph.mapping
        
        for node_idx in cls._queue:
            # Assign sequential index for internal tracking
            if node_idx._index is None:
                node_idx._index = cls._counter
                cls._counter += 1
            
            # Get physical resource from mapper
            physical_resource = mapper.get(node_idx.node_name)
            
            if physical_resource and physical_resource != "GENERIC":
                # Extract numeric ID from physical resource name
                # e.g., "LC2" -> 2, "PE5" -> 5, "GROUP1" -> 1, "AG3" -> 3
                resource_type = ''.join(ch for ch in physical_resource if ch.isalpha())
                resource_id = int(''.join(ch for ch in physical_resource if ch.isdigit()))
                node_idx._physical_id = resource_id
            else:
                # Fallback: use sequential index if no mapping found
                node_idx._physical_id = node_idx._index
        
        cls._resolved = True
        cls._queue.clear()

    @property
    def index(self) -> int:
        """Get the resolved sequential index."""
        if self._index is None:
            NodeIndex.resolve_all()
        return self._index
    
    @property
    def physical_id(self) -> int:
        """Get the physical hardware resource ID (used for bitstream encoding)."""
        if self._physical_id is None:
            NodeIndex.resolve_all()
        return self._physical_id

    def __int__(self):
        """Return physical resource ID for encoding in bitstream."""
        return self.physical_id
    
    def __repr__(self):
        return f"NodeIndex({self.node_name}, phys_id={self._physical_id})"

class Connect:
    """Represents a connection between two nodes in the dataflow graph."""

    def __init__(self, src: str, dst: NodeIndex):
        self.src = NodeIndex(src)
        self.dst = dst
        
        NodeGraph.get().connect(src, dst.node_name)
        
    def __int__(self):
        # Ensure resolution before returning value
        if self.src._physical_id is None:
            NodeIndex.resolve_all()
        
        # Return physical ID or 0 if not available
        if self.src and self.src._physical_id is not None:
            return self.src._physical_id
        return 0
    
    def __repr__(self):
        return f"Connect({self.src.node_name} -> {self.dst.node_name})"