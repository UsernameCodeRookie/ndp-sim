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
            
            # Handle BufferLoopControlGroupConfig - register parent module and submodules
            if isinstance(module, BufferLoopControlGroupConfig):
                if hasattr(module, 'submodules') and len(module.submodules) > 0:
                    # Register each submodule (ROW_LC and COL_LC) separately
                    for submodule in module.submodules:
                        if hasattr(submodule, 'id') and submodule.id:
                            node_name = submodule.id.node_name
                            resource = mapper.get(node_name)
                            if resource:
                                mapper.register_module(resource, submodule)

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
        elif node_name.startswith("GA_PE.PE"):
            return "PE"
        elif "ROW_LC" in node_name:
            return "ROW_LC"
        elif "COL_LC" in node_name:
            return "COL_LC"
        elif node_name.startswith("STREAM."):
            # Need to check if it's READ_STREAM or WRITE_STREAM from mapper
            # For now, return generic STREAM - will be resolved via mapper
            return "STREAM"
        else:
            return "UNKNOWN"
    
    @staticmethod
    def _get_stream_type(node_name: str) -> str:
        """Get the specific stream type (READ_STREAM or WRITE_STREAM) for a node."""
        from bitstream.config.mapper import NodeGraph
        graph = NodeGraph.get()
        mapper = graph.mapping
        
        # Get the physical resource assigned to this stream node
        physical_resource = mapper.get(node_name)
        
        if physical_resource:
            if physical_resource.startswith("READ_STREAM"):
                return "READ_STREAM"
            elif physical_resource.startswith("WRITE_STREAM"):
                return "WRITE_STREAM"
        
        return "STREAM"  # Fallback if not found
    
    @staticmethod
    def _get_lc_row(node_name: str) -> int:
        """Get which row an LC belongs to (0=first row, 1=second row)."""
        if node_name.startswith("DRAM_LC.LC"):
            # Extract the number from "DRAM_LC.LC<num>"
            graph = NodeGraph.get()
            mapper = graph.mapping
            physical_resource = mapper.get(node_name) # LCxx
            
            lc_num = physical_resource[len("LC"):] if physical_resource else "0"
            
            return 0 if int(lc_num) < 8 else 1
        return -1
    
    @staticmethod
    def _get_pe_position(node_name: str) -> tuple:
        """Extract GA_PE row and column from node name (e.g., 'GA_PE.PE12' -> (1, 2)).
        
        Only applies to GA_PE, not LC_PE.
        """
        if node_name.startswith("GA_PE.PE"):
            pe_suffix = node_name[len("GA_PE."):]  # "PE12"
            if pe_suffix.startswith("PE"):
                pe_nums = pe_suffix[2:]  # "12"
                try:
                    if len(pe_nums) == 2:
                        row = int(pe_nums[0])
                        col = int(pe_nums[1])
                        return (row, col)
                    elif len(pe_nums) == 4:
                        row = int(pe_nums[:2])
                        col = int(pe_nums[2:])
                        return (row, col)
                except ValueError:
                    pass
        return None
    
    def _calculate_pe_src_id(self, src_pe_pos: tuple, dst_pe_pos: tuple) -> int:
        """
        Calculate src_id for PE→PE connection based on relative position.
        
        The configuration specifies: current_pe.inport0 = src_pe_name
        This means: src_pe sends data to current_pe (which is dst_pe)
        
        We need to calculate src_id based on src_pe's position relative to dst_pe's position:
        For dst_pe at (i, j) receiving from src_pe:
        - src at (i-1, j-1) -> src_id 1
        - src at (i, j-1)   -> src_id 2
        - src at (i+1, j-1) -> src_id 3
        - src at (i-1, j)   -> src_id 4
        - src at (i+1, j)   -> src_id 5
        """
        if not src_pe_pos or not dst_pe_pos:
            return 0
        
        src_row, src_col = src_pe_pos
        dst_row, dst_col = dst_pe_pos
        
        # Calculate relative position: src relative to dst
        row_diff = src_row - dst_row
        col_diff = src_col - dst_col
        
        # Column j-1 (previous column)
        if col_diff == -1:
            if row_diff == -1:
                return 1  # (i-1, j-1)
            elif row_diff == 0:
                return 2  # (i, j-1)
            elif row_diff == 1:
                return 3  # (i+1, j-1)
        # Same column j
        elif col_diff == 0:
            if row_diff == -1:
                return 4  # (i-1, j)
            elif row_diff == 1:
                return 5  # (i+1, j)
        
        return 0  # Default for invalid positions
    
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
                diff = src_lc_idx - dst_lc_idx
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
            
            # Valid LC range: [group_id*2-2, group_id*2-1, group_id*2, group_id*2+1, group_id*2+2, group_id*2+3]
            # This gives left 2, corresponding 2, right 2 LCs
            lc_range_start = group_id * 2 - 2
            lc_range = list(range(lc_range_start, group_id * 2 + 4))
            
            if src_lc_idx in lc_range:
                relative_idx = lc_range.index(src_lc_idx)
                # Add offset based on which row
                return relative_idx if src_row == 0 else relative_idx + 6
            return 0  # Invalid
        
        # ==================== COL_LC → ROW_LC ====================
        elif src_type == "ROW_LC" and dst_type == "COL_LC":
            return 12
        
        # ==================== LC → PE ====================
        # Each LC connects to 3 PEs: corresponding PE, left 1, right 1 → indices 0-5
        # LCs from rows 0-1 connect to PEs below
        elif src_type == "LC" and dst_type == "PE":
            src_lc_idx = src_phys_id % 8
            dst_pe_idx = dst_phys_id
            
            # LC connects to PEs below
            # Map: [i-1, i, i+1] → [0, 1, 2] for row 0, [3, 4, 5] for row 1
            src_row = self._get_lc_row(self.src.node_name)
            diff = src_lc_idx - dst_pe_idx
            
            if diff == -1:
                return 0 if src_row == 0 else 3
            elif diff == 0:
                return 1 if src_row == 0 else 4
            elif diff == 1:
                return 2 if src_row == 0 else 5
            else:
                return 0  # Invalid
        
        # ==================== PE → PE ====================
        # GA_PE: 2D grid-based src_id calculation based on relative position
        # LC_PE: Same row PE connections: left 2, right 2 → indices 6-9
        elif src_type == "PE" and dst_type == "PE":
            # Check if this is GA_PE (2D grid) or LC_PE (1D row)
            if self.src.node_name.startswith("GA_PE.") and self.dst.node_name.startswith("GA_PE."):
                src_pe_pos = self._get_pe_position(self.src.node_name)
                dst_pe_pos = self._get_pe_position(self.dst.node_name)
                return self._calculate_pe_src_id(src_pe_pos, dst_pe_pos)
            else:
                # LC_PE: Use physical ID for 1D linear calculation
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
        
        # ==================== LC → STREAM (READ_STREAM or WRITE_STREAM) ====================
        # LC connects to streams via specific patterns → indices 0-5 for row 0, 6-11 for row 1
        elif src_type == "LC" and dst_type == "STREAM":
            src_lc_idx = src_phys_id % 8
            src_row = self._get_lc_row(self.src.node_name)
            stream_type = self._get_stream_type(self.dst.node_name)
            
            # Map stream resources to logical stream indices
            # READ_STREAM0-2 -> stream 0-2
            # WRITE_STREAM0 -> stream 3
            if stream_type == "READ_STREAM":
                stream_logical_idx = dst_phys_id
            elif stream_type == "WRITE_STREAM":
                stream_logical_idx = 3  # WRITE_STREAM0 -> stream 3
            else:
                raise ValueError(f"Unknown stream type: {stream_type}")
            
            # Valid LC range for each stream: [stream_idx*2-2, stream_idx*2-1, stream_idx*2, stream_idx*2+1, stream_idx*2+2, stream_idx*2+3]
            lc_range_start = stream_logical_idx * 2 - 2
            lc_range = list(range(lc_range_start, stream_logical_idx * 2 + 4))
            
            if src_lc_idx in lc_range:
                relative_idx = lc_range.index(src_lc_idx)
                return relative_idx if src_row == 0 else relative_idx + 6
            return 0  # Invalid
        
        # ==================== PE → STREAM (READ_STREAM or WRITE_STREAM) ====================
        # PE connects to streams via specific patterns → indices 12-17
        elif src_type == "PE" and dst_type == "STREAM":
            src_pe_idx = src_phys_id
            stream_type = self._get_stream_type(self.dst.node_name)
            
            # Map stream resources to logical stream indices
            # READ_STREAM0-2 -> stream 0-2
            # WRITE_STREAM0 -> stream 3
            if stream_type == "READ_STREAM":
                stream_logical_idx = dst_phys_id
            elif stream_type == "WRITE_STREAM":
                stream_logical_idx = 3  # WRITE_STREAM0 -> stream 3
            else:
                raise ValueError(f"Unknown stream type: {stream_type}")
            
            # Valid PE range: [stream_idx*2-2, stream_idx*2-1, stream_idx*2, stream_idx*2+1, stream_idx*2+2, stream_idx*2+3]
            pe_range_start = stream_logical_idx * 2 - 2
            pe_range = list(range(pe_range_start, stream_logical_idx * 2 + 4))
            
            if src_pe_idx in pe_range:
                relative_idx = pe_range.index(src_pe_idx)
                return 12 + relative_idx
            return 0  # Invalid
        
        # ==================== AG ↔ ROW_LC/COL_LC ====================
        # Hard-wired connections, not variable - return 0 or special indicator
        elif (src_type == "STREAM" and dst_type == "ROW_LC") or \
             (src_type == "STREAM" and dst_type == "COL_LC"):
            return 0  # Hard-wired, no variable index needed
        
        # ==================== ROW_LC/COL_LC → AG ====================
        elif (src_type == "ROW_LC" and dst_type == "STREAM") or \
             (src_type == "COL_LC" and dst_type == "STREAM"):
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