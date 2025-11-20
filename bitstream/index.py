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

    def __new__(cls, name: str, **metadata):
        # If a NodeIndex with the same name exists, return it
        if name in cls._registry:
            return cls._registry[name]
        instance = super().__new__(cls)
        return instance

    def __init__(self, name: str, **metadata):
        # Avoid re-initialization if already registered
        if hasattr(self, "_initialized"):
            return
        self.node_name = name
        self._index: Optional[int] = None
        self._physical_id: Optional[int] = None  # Physical hardware resource ID from mapper
        NodeIndex._queue.append(self)
        NodeIndex._registry[name] = self
        self._initialized = True
        
        NodeGraph.get().add_node(name, **metadata)
    
    @classmethod
    def resolve_all(cls, modules=None):
        """
        Resolve all node indices using the mapper to get physical resource IDs.
        This must be called after NodeGraph.allocate_resources() or NodeGraph.search_mapping().
        
        Args:
            modules: Optional list of modules to register with the mapper after resolution.
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
                # Handle READ_STREAM and WRITE_STREAM specially
                if physical_resource.startswith("READ_STREAM"):
                    resource_id = int(physical_resource[len("READ_STREAM"):])
                elif physical_resource.startswith("WRITE_STREAM"):
                    resource_id = int(physical_resource[len("WRITE_STREAM"):])
                else:
                    # e.g., "LC2" -> 2, "PE5" -> 5, "GROUP1" -> 1
                    resource_type = ''.join(ch for ch in physical_resource if ch.isalpha())
                    resource_id = int(''.join(ch for ch in physical_resource if ch.isdigit()))
                node_idx._physical_id = resource_id
            else:
                # Fallback: use sequential index if no mapping found
                node_idx._physical_id = node_idx._index
        
        cls._resolved = True
        cls._queue.clear()
        
        # Auto-register modules to mapper if provided
        if modules:
            cls._register_modules(modules)
    
    @classmethod
    def _register_modules(cls, modules):
        """Register all modules with the mapper after resolution."""
        from bitstream.config.loop import BufferLoopControlGroupConfig
        
        mapper = NodeGraph.get().mapping
        
        for module in modules:
            # Register modules with direct node IDs
            if hasattr(module, 'id') and module.id:
                node_name = module.id.node_name
                resource = mapper.get(node_name)
                if resource:
                    mapper.register_module(resource, module)
            
            # Handle BufferLoopControlGroupConfig - register parent module
            if isinstance(module, BufferLoopControlGroupConfig):
                if hasattr(module, 'submodules') and len(module.submodules) > 0:
                    first_sub = module.submodules[0]
                    if hasattr(first_sub, 'id') and first_sub.id:
                        node_name = first_sub.id.node_name
                        resource = mapper.get(node_name)  # Gets GROUP resource
                        if resource:
                            mapper.register_module(resource, module)

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
    
    @staticmethod
    def _get_node_type(node_name: str) -> str:
        """Extract node type from node name."""
        if node_name.startswith("DRAM_LC.LC"):
            return "LC"
        elif node_name.startswith("LC_PE.PE"):
            return "PE"
        elif "ROW_LC" in node_name:
            return "ROW_LC"
        elif "COL_LC" in node_name:
            return "COL_LC"
        elif node_name.startswith("STREAM."):
            return "STREAM"
        elif node_name.startswith("AG"):
            return "AG"
        else:
            return "UNKNOWN"
    
    @staticmethod
    def _get_lc_row(node_name: str) -> int:
        """Get which row an LC belongs to (0=first row, 1=second row)."""
        if node_name.startswith("DRAM_LC.LC"):
            # Extract the number from "DRAM_LC.LC<num>"
            lc_num = int(node_name.split(".")[-1][2:])
            return 0 if lc_num < 8 else 1
        return -1
    
    def _calculate_relative_index(self) -> int:
        """
        Calculate relative index based on source and destination node types.
        
        Architecture:
        - Row 0: 8 LCs (LC0-LC7)
        - Row 1: 8 LCs (LC8-LC15)
        - Row 2: 4 GROUPs (GROUP0-GROUP3), each with ROW_LC and COL_LC
        - Row 3: 8 PEs (PE0-PE7)
        - Row 4: 4 AGs (AG0-AG3)
        
        Encoding Rules:
        1. LC → LC (same row):
           - Indices 0-4: 5 LCs from row above (corresponding + left 2, right 2)
           - Indices 5-8: 4 neighbors on same row (left 2, right 2)
           
        2. LC → ROW_LC: 0-11 (6 from row 0, 6 from row 1)
        3. COL_LC → ROW_LC: 12
        
        4. PE → LC (3 LCs): corresponding LC, left 1, right 1 = indices 0-5
        5. PE → PE (4 neighbors): left 2, right 2 = indices 6-9
        
        6. AG → LC row 0 (6 LCs): indices 0-5
           AG → LC row 1 (6 LCs): indices 6-11
           AG → PE (6 PEs): indices 12-17
           AG ↔ ROW_LC/COL_LC: hard-wired (not variable)
        """
        # Ensure physical IDs are resolved
        if self.src._physical_id is None or self.dst._physical_id is None:
            NodeIndex.resolve_all()
        
        src_phys_id = self.src._physical_id if self.src._physical_id is not None else 0
        dst_phys_id = self.dst._physical_id if self.dst._physical_id is not None else 0
        
        src_type = self._get_node_type(self.src.node_name)
        dst_type = self._get_node_type(self.dst.node_name)
        
        # ==================== LC → LC ====================
        # Same row: 5-8 for left 2, right 2 neighbors
        # Different row: 0-4 for corresponding + left 2, right 2 from other row
        if src_type == "LC" and dst_type == "LC":
            src_row = self._get_lc_row(self.src.node_name)
            dst_row = self._get_lc_row(self.dst.node_name)
            src_lc_idx = src_phys_id % 8
            dst_lc_idx = dst_phys_id % 8
            
            if src_row == dst_row:
                # Same row connections: left 2, right 2 → indices 5-8
                diff = src_lc_idx - dst_lc_idx
                if diff == -2:
                    return 5  # left 2
                elif diff == -1:
                    return 6  # left 1
                elif diff == 1:
                    return 7  # right 1
                elif diff == 2:
                    return 8  # right 2
                else:
                    return 0  # Invalid
            else:
                # Different row: corresponding LC and neighbors from other row → indices 0-4
                # Map: [i-2, i-1, i, i+1, i+2] → [0, 1, 2, 3, 4]
                diff = dst_lc_idx - src_lc_idx
                if diff == -2:
                    return 0
                elif diff == -1:
                    return 1
                elif diff == 0:
                    return 2  # Corresponding
                elif diff == 1:
                    return 3
                elif diff == 2:
                    return 4
                else:
                    return 0  # Invalid
        
        # ==================== LC → ROW_LC ====================
        # Each ROW_LC connects to 6 LCs from row 0 and 6 from row 1
        # Encoding: 0-5 for row 0, 6-11 for row 1
        elif src_type == "LC" and dst_type == "ROW_LC":
            group_id = dst_phys_id  # ROW_LC shares physical_id with its GROUP
            src_row = self._get_lc_row(self.src.node_name)
            src_lc_idx = src_phys_id % 8
            
            # Valid LC range: [group_id*2-1, group_id*2, group_id*2+1, group_id*2+2, group_id*2+3]
            # This gives left 2, corresponding 2, right 2 LCs
            lc_range_start = max(0, group_id * 2 - 1)
            lc_range = list(range(lc_range_start, min(8, group_id * 2 + 3)))
            
            if src_lc_idx in lc_range:
                relative_idx = lc_range.index(src_lc_idx)
                # Add offset based on which row
                return relative_idx if src_row == 0 else relative_idx + 6
            return 0  # Invalid
        
        # ==================== COL_LC → ROW_LC ====================
        elif src_type == "COL_LC" and dst_type == "ROW_LC":
            return 12
        
        # ==================== PE → LC ====================
        # Each PE connects to 3 LCs: corresponding LC, left 1, right 1 → indices 0-5
        # But since ROW_LC/COL_LC is above PE, this refers to row above
        elif src_type == "PE" and dst_type == "LC":
            src_pe_idx = src_phys_id
            dst_lc_idx = dst_phys_id % 8
            
            # PE connects to LCs from rows 0-1
            # Map: [i-1, i, i+1] → [0, 1, 2] for row 0, [3, 4, 5] for row 1
            dst_row = self._get_lc_row(self.dst.node_name)
            diff = dst_lc_idx - src_pe_idx
            
            if diff == -1:
                return 0 if dst_row == 0 else 3
            elif diff == 0:
                return 1 if dst_row == 0 else 4
            elif diff == 1:
                return 2 if dst_row == 0 else 5
            else:
                return 0  # Invalid
        
        # ==================== PE → PE ====================
        # Same row PE connections: left 2, right 2 → indices 6-9
        elif src_type == "PE" and dst_type == "PE":
            diff = src_phys_id - dst_phys_id
            if diff == -2:
                return 6  # left 2
            elif diff == -1:
                return 7  # left 1
            elif diff == 1:
                return 8  # right 1
            elif diff == 2:
                return 9  # right 2
            else:
                return 0  # Invalid
        
        # ==================== AG → LC ====================
        # AG connects to 6 LCs from row 0 (indices 0-5) and 6 from row 1 (indices 6-11)
        elif src_type == "AG" and dst_type == "LC":
            ag_idx = src_phys_id
            dst_lc_idx = dst_phys_id % 8
            dst_row = self._get_lc_row(self.dst.node_name)
            
            # Valid LC range for each AG: [ag_idx*2-1, ag_idx*2, ag_idx*2+1, ag_idx*2+2, ag_idx*2+3]
            lc_range_start = max(0, ag_idx * 2 - 1)
            lc_range = list(range(lc_range_start, min(8, ag_idx * 2 + 3)))
            
            if dst_lc_idx in lc_range:
                relative_idx = lc_range.index(dst_lc_idx)
                return relative_idx if dst_row == 0 else relative_idx + 6
            return 0  # Invalid
        
        # ==================== AG → PE ====================
        # AG connects to 6 PEs: corresponding PE and neighbors → indices 12-17
        elif src_type == "AG" and dst_type == "PE":
            ag_idx = src_phys_id
            pe_idx = dst_phys_id
            
            # Valid PE range: [ag_idx*2-1, ag_idx*2, ag_idx*2+1, ag_idx*2+2, ag_idx*2+3]
            pe_range_start = max(0, ag_idx * 2 - 1)
            pe_range = list(range(pe_range_start, min(8, ag_idx * 2 + 3)))
            
            if pe_idx in pe_range:
                relative_idx = pe_range.index(pe_idx)
                return 12 + relative_idx
            return 0  # Invalid
        
        # ==================== AG ↔ ROW_LC/COL_LC ====================
        # Hard-wired connections, not variable - return 0 or special indicator
        elif (src_type == "AG" and dst_type == "ROW_LC") or \
             (src_type == "AG" and dst_type == "COL_LC"):
            return 0  # Hard-wired, no variable index needed
        
        # ==================== ROW_LC/COL_LC → AG ====================
        elif (src_type == "ROW_LC" and dst_type == "AG") or \
             (src_type == "COL_LC" and dst_type == "AG"):
            return 0  # Hard-wired, no variable index needed
        
        # Default: return 0 for unmapped types
        return 0
        
    def __int__(self):
        # Ensure resolution before returning value
        if self.src._physical_id is None:
            NodeIndex.resolve_all()
        
        # Return relative index based on connection type
        return self._calculate_relative_index()
    
    def __repr__(self):
        return f"Connect({self.src.node_name} -> {self.dst.node_name})"