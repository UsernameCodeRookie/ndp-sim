from typing import Dict, List, Optional, Set, Tuple
from collections import defaultdict
import os
import time
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.patches import FancyArrowPatch
from matplotlib.path import Path

class Mapper:
    """
    Manage the mapping between logical nodes and physical hardware resources.

    Each resource type (LC, GROUP, AG, PE) has a fixed number of available
    physical instances. This class allocates them on demand and maintains
    a node → resource mapping.
    """
    def __init__(self, seed: Optional[int] = None):
        # Predefined physical resource pools
        # New architecture: 2 rows of 8 LCs, 4 ROW_LC, 4 COL_LC, 8 PE, 4 AG
        self.resource_pools = {
            "LC":         [f"LC{i}" for i in range(16)],          # 16 LC resources (2 rows of 8)
            "ROW_LC":     [f"ROW_LC{i}" for i in range(4)],       # 4 ROW_LC resources
            "COL_LC":     [f"COL_LC{i}" for i in range(4)],       # 4 COL_LC resources
            "PE":         [f"PE{i}" for i in range(8)],           # 8 Processing Elements
            "READ_STREAM": [f"READ_STREAM{i}" for i in range(3)], # 3 Read Streams (physical)
            "WRITE_STREAM":[f"WRITE_STREAM{i}" for i in range(1)],# 1 Write Stream (physical)
        }

        # Track how many of each resource type have been used
        self.resource_counters = defaultdict(int)

        # Mapping: node name → assigned physical resource
        self.node_to_resource: Dict[str, str] = {}
        
        # Mapping: physical resource → module object (for direct access)
        self.resource_to_module: Dict[str, any] = {}
        
        # List of all nodes registered for allocation
        self.nodes: List[str] = []
        
        # Direct mapping mode flag
        self.use_direct_mapping: bool = False
        
        # Assigned node
        self.assigned_node: Dict[str, str] = {}
        
        # Random seed for reproducibility
        # self.seed = seed
        # if seed is not None:
        #     import random
        #     random.seed(seed)
        
    def get_type(self, node: str) -> Optional[str]:
        """Infer the resource type of a node based on its name prefix.
        
        Node naming patterns:
        - LCs (2 rows): "DRAM_LC.LC{row}{col}" where row is 0-1, col is 0-7
        - ROW_LC: "ROW_LC.ROW_LC{row}{col}" or "GROUP{n}.ROW_LC"
        - COL_LC: "COL_LC.COL_LC{row}{col}" or "GROUP{n}.COL_LC"
        - PE: "PE.PE{idx}" where idx is 0-7
        - STREAM: "STREAM.stream{idx}"
        """
        if "DRAM_LC" in node and ".LC" in node:
            return "LC"
        elif "ROW_LC" in node:
            # Support both "ROW_LC.ROW_LC{idx}" and "GROUP{n}.ROW_LC"
            return "ROW_LC"
        elif "COL_LC" in node:
            # Support both "COL_LC.COL_LC{idx}" and "GROUP{n}.COL_LC"
            return "COL_LC"
        elif "PE" in node and ".PE" in node:
            return "PE"
        elif node.startswith("STREAM"):
            # Stream type is determined during allocation based on JSON mode
            return "STREAM"  # Will be refined to READ_STREAM or WRITE_STREAM
        else:
            return None
        
    def get_type_from_resource(self, resource: str) -> Optional[str]:
        """Infer the resource type from a physical resource name."""
        if resource.startswith("LC"):
            return "LC"
        elif resource.startswith("ROW_LC"):
            return "ROW_LC"
        elif resource.startswith("COL_LC"):
            return "COL_LC"
        elif resource.startswith("PE"):
            return "PE"
        elif resource.startswith("READ_STREAM"):
            return "READ_STREAM"
        elif resource.startswith("WRITE_STREAM"):
            return "WRITE_STREAM"
        else:
            return None

    def parse_resource(self, resource: str):
        """Parse resource string to extract its type and index.
        
        Args:
            resource: The resource string, e.g., 'READ_STREAM0', 'WRITE_STREAM1', 'LC0', etc.
            
        Returns:
            A tuple (res_type, res_idx), where res_type is the type (e.g., 'STREAM', 'LC', etc.)
            and res_idx is the integer index extracted from the resource string.
        """
        if resource.startswith("READ_STREAM"):
            res_type = "STREAM"
            res_idx = int(resource[len("READ_STREAM"):])
        elif resource.startswith("WRITE_STREAM"):
            res_type = "STREAM"
            res_idx = 3 + int(resource[len("WRITE_STREAM"):])  # Offset for WRITE_STREAM resources
        else:
            # Extract alphabetical prefix as resource type and digits as index if present
            res_type = ''.join(ch for ch in resource if ch.isalpha())
            digits = ''.join(ch for ch in resource if ch.isdigit())
            res_idx = int(digits) if digits != '' else 0
        
        return res_type, res_idx
    
    def extract_logical_index(self, node: str) -> Optional[int]:
        """Extract logical index from node name for direct mapping.
        
        Node name patterns:
        - LC: 'DRAM_LC.LC{row}{col}' -> linearize to single index
        - ROW_LC: 'ROW_LC.ROW_LC{row}{col}' -> linearize
        - COL_LC: 'COL_LC.COL_LC{row}{col}' -> linearize
        - PE: 'PE.PE{idx}' -> idx
        - STREAM: 'STREAM.stream{idx}' -> idx
            
        Returns:
            Logical index as integer, or None if not found
        """
        import re
        
        if "DRAM_LC" in node and ".LC" in node:
            match = re.search(r'LC(\d+)(\d)$', node)  # LCrc where r=row, c=col
            if match:
                row = int(match.group(1)[0]) if len(match.group(1)) > 0 else 0
                col = int(match.group(2)) if match.group(2) else 0
                return row * 8 + col  # Linearize: first row 0-7, second row 8-15
        elif "ROW_LC" in node:
            # Support both "GROUP{n}.ROW_LC" and "ROW_LC.ROW_LC{idx}"
            match = re.search(r'GROUP(\d+)\.ROW_LC', node)
            if match:
                return int(match.group(1))
            match = re.search(r'ROW_LC(\d+)$', node)
            if match:
                return int(match.group(1))
        elif "COL_LC" in node:
            # Support both "GROUP{n}.COL_LC" and "COL_LC.COL_LC{idx}"
            match = re.search(r'GROUP(\d+)\.COL_LC', node)
            if match:
                return int(match.group(1))
            match = re.search(r'COL_LC(\d+)$', node)
            if match:
                return int(match.group(1))
        elif "PE" in node and ".PE" in node:
            match = re.search(r'PE(\d+)$', node)
            if match:
                return int(match.group(1))
        elif node.startswith("STREAM.stream"):
            match = re.search(r'stream(\d+)$', node)
            if match:
                return int(match.group(1))
        
        return None


    def allocate(self, node: str, stream_type: Optional[str] = None) -> str:
        """
        Allocate a physical resource for a node based on its type.
        
        Args:
            node: Node name to allocate
            stream_type: For STREAM nodes, specify "read" or "write" to determine pool
        """        
        # Return existing allocation if present
        if node in self.node_to_resource:
            return self.node_to_resource[node]
        
        # Add node to the list if not already present
        if node not in self.nodes:
            self.nodes.append(node)
        
        res_type = self.get_type(node)

        # Non-hardware or unrecognized node type
        if res_type is None:
            self.node_to_resource[node] = "GENERIC"
            return "GENERIC"
        
        # Initialize idx to None (will be set later based on mapping mode)
        idx = None
        
        # Special case: STREAM nodes need to know read vs write
        if res_type == "STREAM":
            # In direct mapping mode, determine read/write based on logical index
            if self.use_direct_mapping:
                logical_idx = self.extract_logical_index(node)
                if logical_idx is not None:
                    # streams 0-2 are READ_STREAMs, stream 3+ are WRITE_STREAMs
                    num_read_streams = len(self.resource_pools["READ_STREAM"])
                    if logical_idx < num_read_streams:
                        res_type = "READ_STREAM"
                        idx = logical_idx
                    else:
                        res_type = "WRITE_STREAM"
                        idx = logical_idx - num_read_streams
                else:
                    # Fallback if we can't extract index
                    if stream_type == "read":
                        res_type = "READ_STREAM"
                    elif stream_type == "write":
                        res_type = "WRITE_STREAM"
                    else:
                        res_type = "READ_STREAM"
                    idx = None  # Will be set by counter below
            else:
                # Normal mode: use stream_type parameter
                if stream_type == "read":
                    res_type = "READ_STREAM"
                elif stream_type == "write":
                    res_type = "WRITE_STREAM"
                else:
                    # Default to READ_STREAM if not specified
                    res_type = "READ_STREAM"
                idx = None  # Will be set by counter below

        pool = self.resource_pools.get(res_type)
        if pool is None:
            raise RuntimeError(f"[Error] Unknown resource type: {res_type} for node {node})")
        
        # Determine index based on mapping mode
        if self.use_direct_mapping:
            # Direct mapping: extract logical index from node name
            if idx is None:  # idx might already be set for STREAM nodes above
                logical_idx = self.extract_logical_index(node)
                if logical_idx is not None:
                    idx = logical_idx
                    # Validate index is within pool range
                    if idx >= len(pool):
                        raise RuntimeError(f"[Error] Direct mapping failed: {node} has index {idx}, but {res_type} pool only has {len(pool)} resources")
                else:
                    # Fallback to counter if we can't extract index
                    idx = self.resource_counters[res_type]
                    self.resource_counters[res_type] += 1
            else:
                # idx was already set (e.g., for STREAM nodes), validate it
                if idx >= len(pool):
                    raise RuntimeError(f"[Error] Direct mapping failed: {node} has index {idx}, but {res_type} pool only has {len(pool)} resources")
        else:
            # Normal allocation — pick the next available resource from the pool
            idx = self.resource_counters[res_type]
            self.resource_counters[res_type] += 1
        
        # Pool exhausted check (for counter-based allocation)
        if idx >= len(pool):
            raise RuntimeError(f"[Error] {res_type} pool exhausted! (node: {node})")

        # Assign resource
        resource_name = pool[idx]
        self.node_to_resource[node] = resource_name
        return resource_name
    
    class Constraint:
        """Base class for a connection-based constraint on physical resources."""
        def check(self, src_type: str, src_idx: int, dst_type: str, dst_idx: int) -> bool:
            """Return True if the connection satisfies the constraint."""
            raise NotImplementedError

        def penalty(self, src_type: str, src_idx: int, dst_type: str, dst_idx: int) -> float:
            """Return a non-negative penalty value indicating how badly the constraint is violated.
            0 -> no violation; larger values -> worse violation.
            Default: return 0 if check is True else 1 (coarse)
            """
            return 0.0 if self.check(src_type, src_idx, dst_type, dst_idx) else 1.0

    class LCtoLCConstraint(Constraint):
        """LC i → LC j constraint: 
        Within same row: j in [i-2, i-1, i+1, i+2] (distance 1 or 2)
        Between rows: connect to corresponding LC or ±2 positions in adjacent row
        """
        def check(self, src_type: str, src_idx: int, dst_type: str, dst_idx: int) -> bool:
            if src_type == "LC" and dst_type == "LC":
                src_row, src_col = divmod(src_idx, 8)
                dst_row, dst_col = divmod(dst_idx, 8)
                
                # LC in row 1 cannot connect to row 0 LCs
                if dst_row == 0 and src_row == 1:
                    # LC in row 0 cannot connect to row 1 LCs
                    return False
                
                # Same row: allowed distance is 1 or 2
                if src_row == dst_row:
                    return abs(dst_col - src_col) in [1, 2]
                # Different rows: src can connect to dst row's positions: col, col-2, col-1, col+1, col+2
                elif abs(src_row - dst_row) == 1:
                    return abs(dst_col - src_col) <= 2
                return False
            return True

        def penalty(self, src_type: str, src_idx: int, dst_type: str, dst_idx: int) -> float:
            if src_type == "LC" and dst_type == "LC":
                if self.check(src_type, src_idx, dst_type, dst_idx):
                    return 0.0
                src_row, src_col = divmod(src_idx, 8)
                dst_row, dst_col = divmod(dst_idx, 8)
                
                # if dst_row == 0 and src_row == 1:
                #     # LC in row 0 cannot connect to row 1 LCs
                #     return 100.0  # Extreme penalty for invalid connection
                
                # Penalty based on distance
                distance = abs(src_row - dst_row) * 8 + abs(src_col - dst_col)
                return float(distance)
            return 0.0
        
    class LCtoROWLCConstraint(Constraint):
        """LC i → ROW_LC j constraint:
        First row (LC 0-7) connects to ROW_LC 0-3 (corresponding upper positions)
        Second row (LC 8-15) connects to ROW_LC 4-7 (corresponding lower positions)
        Each ROW_LC connects to 6 LC nodes (2 above left, corresponding, 2 above right, same from below)
        """
        def check(self, src_type: str, src_idx: int, dst_type: str, dst_idx: int) -> bool:
            if src_type == "LC" and dst_type == "ROW_LC":
                src_row, src_col = divmod(src_idx, 8)
                # For ROW_LC, it connects to specific LCs from both rows
                # Mapping is flexible for now - just check reasonable distance
                return abs(dst_idx - (src_col // 2)) <= 2
            return True

        def penalty(self, src_type: str, src_idx: int, dst_type: str, dst_idx: int) -> float:
            if src_type == "LC" and dst_type == "ROW_LC":
                src_row, src_col = divmod(src_idx, 8)
                # Penalize based on distance
                d = abs(dst_idx - (src_col // 2))
                return float(d)
            return 0.0

    class LCtoStreamConstraint(Constraint):
        """LC i → AG j constraint:
        First row LC (0-7) connects to AG's first 6 targets
        Second row LC (8-15) connects to AG's second 6 targets
        """
        def check(self, src_type: str, src_idx: int, dst_type: str, dst_idx: int) -> bool:
            if src_type == "LC" and dst_type == "STREAM":
                src_row, src_col = divmod(src_idx, 8)
                # LC from row 0: can map to AG targets 0-5
                # LC from row 1: can map to AG targets 6-11
                # Each AG has 12 logical LC targets (6 from each row)
                return True  # Flexible constraint
            return True

        def penalty(self, src_type: str, src_idx: int, dst_type: str, dst_idx: int) -> float:
            if src_type == "LC" and dst_type == "STREAM":
                src_row, src_col = divmod(src_idx, 8)
                # Penalize based on distance from expected position
                expected_ag = src_col // 2
                d = abs(dst_idx - expected_ag)
                return float(d)
            return 0.0

    class PEtoLCConstraint(Constraint):
        """PE i ↔ LC j constraint with strict enforcement:
        
        Each PE i can only connect to row 0 LCs within column distance ≤1:
        - LC at column i
        - LC at column i-1 (left)
        - LC at column i+1 (right)
        
        PE MUST ONLY connect to first row (row 0) LCs. No connections to second row allowed.
        
        Handles both PE→LC and LC→PE directions.
        """
        def check(self, src_type: str, src_idx: int, dst_type: str, dst_idx: int) -> bool:
            # Handle PE → LC direction
            if src_type == "PE" and dst_type == "LC":
                dst_row, dst_col = divmod(dst_idx, 8)
                # PE can ONLY connect to first row LCs
                if dst_row != 0:
                    return False
                # Must be within distance 1 of the PE column
                return abs(dst_col - src_idx) <= 1
            # Handle LC → PE direction (reverse: src=LC, dst=PE)
            elif src_type == "LC" and dst_type == "PE":
                src_row, src_col = divmod(src_idx, 8)
                # LC in row 1 cannot connect to any PE
                if src_row != 0:
                    return False
                # Must be within distance 1 of the PE column (PE index = column for now)
                return abs(src_col - dst_idx) <= 1
            return True

        def penalty(self, src_type: str, src_idx: int, dst_type: str, dst_idx: int) -> float:
            # Handle PE → LC direction
            if src_type == "PE" and dst_type == "LC":
                dst_row, dst_col = divmod(dst_idx, 8)
                # VERY HIGH penalty for second row (should never happen)
                if dst_row != 0:
                    return 100.0  # Extreme penalty to disallow second row connections
                # Penalty for distance within same row
                d = abs(dst_col - src_idx)
                if d <= 1:
                    return 0.0
                return float(d)
            # Handle LC → PE direction (reverse: src=LC, dst=PE)
            elif src_type == "LC" and dst_type == "PE":
                src_row, src_col = divmod(src_idx, 8)
                # VERY HIGH penalty for second row LC
                if src_row != 0:
                    return 100.0  # Extreme penalty to disallow second row connections
                # Penalty for distance within row 0
                d = abs(src_col - dst_idx)
                if d <= 1:
                    return 0.0
                return float(d)
            return 0.0

    class PEtoPEConstraint(Constraint):
        """PE i → PE j constraint: j in [i-2, i-1, i+1, i+2] (distance 1 or 2)"""
        def check(self, src_type: str, src_idx: int, dst_type: str, dst_idx: int) -> bool:
            if src_type == "PE" and dst_type == "PE":
                return abs(dst_idx - src_idx) in [1, 2]
            return True

        def penalty(self, src_type: str, src_idx: int, dst_type: str, dst_idx: int) -> float:
            if src_type == "PE" and dst_type == "PE":
                d = abs(dst_idx - src_idx)
                if d in [1, 2]:
                    return 0.0
                return float(max(0, d - 2))
            return 0.0

    class PEtoStreamConstraint(Constraint):
        """PE i → AG/STREAM j constraint:
        PE connects to AG/STREAM targets (3 positions: above, left-2, right+2)
        Since AG and STREAM are equivalent, this also applies to STREAM connections.
        """
        def check(self, src_type: str, src_idx: int, dst_type: str, dst_idx: int) -> bool:
            if src_type == "PE" and dst_type == "STREAM":
                d = abs(dst_idx - (src_idx // 2))  # AG index is src_idx // 2
                if d in [0, 1]:
                    return True  # Flexible for now
                # Disallow other connections
                return False
            return True

        def penalty(self, src_type: str, src_idx: int, dst_type: str, dst_idx: int) -> float:
            if src_type == "PE" and dst_type == "STREAM":
                expected_ag = src_idx // 2
                d = abs(dst_idx - expected_ag)
                if d in [0, 1]:
                    return 0.0
                return float(d - 1)  # Penalize for distance beyond 1
            return 0.0

    class LCtoStreamConstraint(Constraint):
        """LC i → STREAM j constraint: 
        Unified STREAM indexing: READ_STREAM0,1,2 → 0,1,2; WRITE_STREAM0 → 3
        """
        def check(self, src_type: str, src_idx: int, dst_type: str, dst_idx: int) -> bool:
            if src_type == "LC" and dst_type == "STREAM":
                # LC can connect to streams with reasonable topology constraints
                src_row, src_col = divmod(src_idx, 8)
                return abs(dst_idx - (src_col // 2)) in [0, 1]
            return True

        def penalty(self, src_type: str, src_idx: int, dst_type: str, dst_idx: int) -> float:
            if src_type == "LC" and dst_type == "STREAM":
                src_row, src_col = divmod(src_idx, 8)
                d = abs(dst_idx - (src_col // 2))
                if d in [0, 1]:
                    return 0.0
                return float(d - 1)
            return 0.0

    class ROWLCtoColLCConstraint(Constraint):
        """ROW_LC i → COL_LC i constraint: hard-wired connection
        
        Each ROW_LC i connects to its corresponding COL_LC i (hard-wired, same index).
        When ROW_LC i maps to physical ROW_LC j, then COL_LC i MUST map to physical COL_LC j.
        """
        def check(self, src_type: str, src_idx: int, dst_type: str, dst_idx: int) -> bool:
            # Only check ROW_LC→COL_LC connections in the actual connections
            # The mapping constraint is enforced via penalty in cost calculation
            if src_type == "ROW_LC" and dst_type == "COL_LC":
                return src_idx == dst_idx  # ROW_LC i must connect to COL_LC i
            elif dst_type == "COL_LC" and src_type != "ROW_LC":
                return False  # COL_LC should only receive from ROW_LC
            return True

        def penalty(self, src_type: str, src_idx: int, dst_type: str, dst_idx: int) -> float:
            if src_type == "ROW_LC" and dst_type == "COL_LC":
                if src_idx == dst_idx:
                    return 0.0
                return 10000.0  # Extremely heavy penalty
            elif dst_type == "COL_LC" and src_type != "ROW_LC":
                return 10000.0  # Extremely heavy penalty
            return 0.0
    

    def direct_mapping(self) -> Dict[str, str]:
        """
        Directly map each node's logical index to its corresponding physical index.
        No constraint search is performed - simply uses the allocation order.
        
        Returns:
            Dict[str, str]: The current node→resource mapping (already set by allocate())
        """
        print(f"[Direct Mapping] Using direct logical→physical mapping (no constraint search)")
        print(f"[Direct Mapping] Mapped {len(self.node_to_resource)} nodes")
        return self.node_to_resource
    
    def search(self, connections: List[Dict[str, str]], max_iterations: int = 5000, 
                        initial_temp: float = 100.0, cooling_rate: float = 0.995,
                        node_metadata: Optional[Dict[str, Dict]] = None,
                        repair_prob: float = 0.2, seed: Optional[int] = None) -> Optional[Dict[str, str]]:
        """
        Perform simulated annealing search to find a valid mapping for large graphs.
        Uses probabilistic optimization to escape local minima and find better solutions.
        
        Args:
            connections: List of connection dictionaries (e.g., {"src": ..., "dst": ...})
            max_iterations: Maximum number of optimization iterations (default: 5000)
            initial_temp: Initial temperature for simulated annealing (default: 100.0)
            cooling_rate: Temperature cooling rate per iteration (default: 0.995)
            node_metadata: Dictionary containing node metadata like stream_type (default: None)
            repair_prob: Probability of repair moves during search (default: 0.2)
            seed: Random seed for reproducibility (default: None - uses current random state)
        
        Returns:
            Dict[str, str]: Valid node→resource mapping if found; otherwise None
        """
        if node_metadata is None:
            node_metadata = {}
        import random
        import math
        
        # Set random seed if provided
        # if seed is not None:
        #     random.seed(seed)
        # elif self.seed is not None:
        #     random.seed(self.seed)
        
        print(f"[Simulated Annealing] Starting search with {len(connections)} connections")
        print(f"[Simulated Annealing] Parameters: iterations={max_iterations}, T0={initial_temp}, cooling={cooling_rate}, repair_prob={repair_prob}")
        
        # Define all structural constraints
        self.constraints: List[Mapper.Constraint] = [
            self.LCtoLCConstraint(),
            self.LCtoROWLCConstraint(),
            self.LCtoStreamConstraint(),
            self.PEtoLCConstraint(),
            self.PEtoPEConstraint(),
            self.PEtoStreamConstraint(),
            self.LCtoStreamConstraint(),
            self.ROWLCtoColLCConstraint()
        ]
        
        # Collect all nodes participating in connections
        nodes_in_connections = set()
        for c in connections:
            # Keep full node names including .ROW_LC and .COL_LC
            src, dst = c["src"], c["dst"]
            nodes_in_connections.add(src)
            nodes_in_connections.add(dst)
        
        unique_nodes = [node for node in self.nodes if node in nodes_in_connections]
        print(f"[Simulated Annealing] Processing {len(unique_nodes)} nodes")
        # Skip nodes that were explicitly assigned via assign_node() from the search, but keep them for cost checks
        assigned_nodes = [n for n in unique_nodes if n in set(self.assigned_node.keys())]
        if assigned_nodes:
            print(f"[Simulated Annealing] Skipping {len(assigned_nodes)} pre-assigned nodes: {assigned_nodes}")
        unique_search_nodes = [n for n in unique_nodes if n not in set(self.assigned_node.keys())]
        print(f"[Simulated Annealing] Nodes being searched: {len(unique_search_nodes)}")
        
        # Group nodes by the pool key (based on EXISTING resource if present, else logical type)
        # This ensures that node operations (swap/reassign/repair) are only done within the same pool
        def pool_key_for_node(n: str) -> Optional[str]:
            # If node already has an allocated resource, prefer that
            existing_res = self.node_to_resource.get(n)
            if existing_res:
                pkey = self.get_type_from_resource(existing_res)
                # Some old code might return None for unknown; guard
                return pkey
            # Otherwise, derive logically
            node_type = self.get_type(n)
            if node_type == 'STREAM':
                metadata = node_metadata.get(n, {})
                stream_type = metadata.get('stream_type', 'read')
                return 'READ_STREAM' if stream_type == 'read' else 'WRITE_STREAM'
            return node_type

        nodes_by_type = defaultdict(list)
        for node in unique_search_nodes:
            pkey = pool_key_for_node(node)
            if pkey:
                nodes_by_type[pkey].append(node)
        
        # Helper function to calculate cost (sum of constraint penalties)
        def calculate_cost(mapping: Dict[str, str]) -> float:
            cost = 0.0
            # Ensure cost checks include pre-assigned nodes as well
            full_mapping = self.node_to_resource.copy()
            full_mapping.update(mapping)
            for c in connections:
                src, dst = c["src"], c["dst"]
                if src.endswith("ROW_LC") or src.endswith("COL_LC"):
                    src = src.split(".")[0]
                if dst.endswith("ROW_LC") or dst.endswith("COL_LC"):
                    dst = dst.split(".")[0]

                if src in full_mapping and dst in full_mapping:
                    src_res = full_mapping[src]
                    dst_res = full_mapping[dst]
                    src_type, src_idx = self.parse_resource(src_res)
                    dst_type, dst_idx = self.parse_resource(dst_res)

                    for constraint in self.constraints:
                        cost += constraint.penalty(src_type, src_idx, dst_type, dst_idx)
            return cost
        
        # Initialize with current allocation - allow randomized start to better explore
        # Start with a mapping that preserves node->resource uniqueness per type
        def random_initial_mapping():
            # Start with pre-assigned nodes already in the mapping and mark their resources used
            m = {n: self.node_to_resource[n] for n in assigned_nodes}
            pre_used_res = set(m.values())
            for t, nodes in nodes_by_type.items():
                pool = self.resource_pools.get(t, [])
                if len(nodes) > len(pool):
                    # Not enough physical resources -> just use deterministic existing map
                    for n in nodes:
                        if n in self.node_to_resource:
                            m[n] = self.node_to_resource[n]
                        else:
                            # Fallback: assign by cycling through pool
                            idx = len(m) % len(pool)
                            m[n] = pool[idx]
                else:
                    # Shuffle pool and assign unique resources, avoiding any resources already used by pre-assigned nodes
                    shuffled = [r for r in pool if r not in pre_used_res]
                    random.shuffle(shuffled)
                    for i, n in enumerate(nodes):
                        # If node already mapped and there's a valid mapping, prefer keeping it
                        if n in self.node_to_resource and self.node_to_resource[n] in shuffled:
                            # Use existing mapping, remove resource from shuffled to avoid duplicates
                            existing_res = self.node_to_resource[n]
                            if existing_res in shuffled:
                                shuffled.remove(existing_res)
                            m[n] = existing_res
                        else:
                            m[n] = shuffled.pop(0)
            return m

        # Use a randomized initial mapping to overcome initial bias of sequential allocation
        current_mapping = random_initial_mapping()
        current_cost = calculate_cost(current_mapping)
        
        best_mapping = current_mapping.copy()
        best_cost = current_cost
        # Track stagnation and random restarts
        stagnation = 0
        
        print(f"[Simulated Annealing] Initial cost: {current_cost:.2f} (penalty sum)")
        
        # Simulated annealing main loop
        temperature = initial_temp
        accepted_moves = 0
        rejected_moves = 0
        
        for iteration in range(max_iterations):
            # Progress reporting
            if iteration % 500 == 0 and iteration > 0:
                print(f"[Simulated Annealing] Iteration {iteration}/{max_iterations}, "
                      f"T={temperature:.2f}, cost={current_cost}, best={best_cost}, "
                      f"accepted={accepted_moves}, rejected={rejected_moves}")
            
            # If there are no unassigned nodes to operate on, exit loop early
            if not nodes_by_type:
                break
            # Select a random resource type and perform either a swap or a reassign
            res_type = random.choice(list(nodes_by_type.keys()))
            type_nodes = nodes_by_type[res_type]

            if len(type_nodes) == 0:
                continue

            pool = self.resource_pools.get(res_type, [])

            # With some probability, choose between reassigning to unused resources, doing a repair move,
            # or swapping two nodes of the same type. repair_prob biases towards targeted repairs.
            r = random.random()
            if r < 0.4:
                node = random.choice(type_nodes)
                # Find resources not yet used in the current mapping
                used_resources = set(current_mapping.values())
                unused_resources = [r for r in pool if r not in used_resources]

                # If we have unused resources, assign node to a random unused resource
                if len(unused_resources) > 0:
                    new_res = random.choice(unused_resources)
                    new_mapping = current_mapping.copy()
                    new_mapping[node] = new_res
                    # Occasional progress print to show we're exploring unused resources
                    if iteration % 500 == 0:
                        print(f"[Simulated Annealing] Reassigning {node} -> {new_res} (unused resource) at iter {iteration}")
                else:
                    # Fallback to swap when no unused resources are available
                    if len(type_nodes) < 2:
                        continue
                    node1, node2 = random.sample(type_nodes, 2)
                    new_mapping = current_mapping.copy()
                    new_mapping[node1], new_mapping[node2] = new_mapping[node2], new_mapping[node1]
            elif r < 0.4 + repair_prob:
                # Repair move: pick a violated connection and try to reduce its penalty by changing an endpoint
                # Collect violated connections and select the worst offender
                def _conn_penalty(src, dst, mapping):
                    # Combine pre-assigned mappings with the current mapping to evaluate penalties
                    full_map = self.node_to_resource.copy()
                    full_map.update(mapping)
                    if src not in full_map or dst not in full_map:
                        return 0.0
                    src_res = full_map[src]
                    dst_res = full_map[dst]
                    src_type, src_idx = self.parse_resource(src_res)
                    dst_type, dst_idx = self.parse_resource(dst_res)
                    total = 0.0
                    for cst in self.constraints:
                        total += cst.penalty(src_type, src_idx, dst_type, dst_idx)
                    return total

                violated = []
                for conn in connections:
                    s, d = conn['src'], conn['dst']
                    if s.endswith('ROW_LC') or s.endswith('COL_LC'):
                        s = s.split('.')[0]
                    if d.endswith('ROW_LC') or d.endswith('COL_LC'):
                        d = d.split('.')[0]
                    p = _conn_penalty(s, d, current_mapping)
                    if p > 0:
                        violated.append((p, s, d))

                if violated:
                    # Sort by penalty and select the worst connection
                    violated.sort(reverse=True)
                    _, src_bad, dst_bad = violated[0]
                    # Prefer selecting an unassigned endpoint for repair
                    if src_bad in unique_search_nodes and dst_bad in unique_search_nodes:
                        node = src_bad if random.random() < 0.5 else dst_bad
                    elif src_bad in unique_search_nodes:
                        node = src_bad
                    elif dst_bad in unique_search_nodes:
                        node = dst_bad
                    else:
                        # Both endpoints are pre-assigned; skip targeted repair
                        node = None
                    # Determine pool based on actual pool key for this node (only if node is not None)
                    if node is not None:
                        pkey = pool_key_for_node(node)
                        pool = self.resource_pools.get(pkey, [])
                    else:
                        pool = []
                    # Try candidate resources and pick the one that minimizes total cost
                    best_local = None
                    best_local_cost = float('inf')
                    used = set(current_mapping.values())
                    if node is None:
                        # fallback to swapping if targeted repair isn't possible
                        if len(type_nodes) < 2:
                            continue
                        node1, node2 = random.sample(type_nodes, 2)
                        new_mapping = current_mapping.copy()
                        new_mapping[node1], new_mapping[node2] = new_mapping[node2], new_mapping[node1]
                        # proceed to evaluation below
                    else:
                        for cand in pool:
                            if cand in used and cand != current_mapping.get(node):
                                continue
                            trial = current_mapping.copy()
                            trial[node] = cand
                            cst = calculate_cost(trial)
                            if cst < best_local_cost:
                                best_local_cost = cst
                                best_local = trial
                    if best_local is not None:
                        new_mapping = best_local
                    else:
                        # fallback to swapping if repair fails
                        if len(type_nodes) < 2:
                            continue
                        node1, node2 = random.sample(type_nodes, 2)
                        new_mapping = current_mapping.copy()
                        new_mapping[node1], new_mapping[node2] = new_mapping[node2], new_mapping[node1]
                else:
                    # No violated connections found; perform a swap
                    if len(type_nodes) < 2:
                        continue
                    node1, node2 = random.sample(type_nodes, 2)
                    new_mapping = current_mapping.copy()
                    new_mapping[node1], new_mapping[node2] = new_mapping[node2], new_mapping[node1]
            else:
                # Swap two nodes of the same type
                if len(type_nodes) < 2:
                    continue
                node1, node2 = random.sample(type_nodes, 2)
                # Create new mapping by swapping
                new_mapping = current_mapping.copy()
                new_mapping[node1], new_mapping[node2] = new_mapping[node2], new_mapping[node1]
            
            # Ensure new mapping maintains uniqueness of resources per type
            # (i.e., no two nodes assigned to the same physical resource)
            def is_valid_unique_mapping(mapping):
                assigned = list(mapping.values())
                if len(assigned) != len(set(assigned)):
                    return False
                # Also ensure type/pool consistency: a node must be assigned only resources
                # from its intended pool key (either existing resource's pool or logical type).
                def intended_pool_key(n: str) -> Optional[str]:
                    existing_res = self.node_to_resource.get(n)
                    if existing_res:
                        return self.get_type_from_resource(existing_res)
                    node_type = self.get_type(n)
                    if node_type == 'STREAM':
                        meta = node_metadata.get(n, {})
                        return 'READ_STREAM' if meta.get('stream_type','read') == 'read' else 'WRITE_STREAM'
                    return node_type

                for n, r in mapping.items():
                    rtype = self.get_type_from_resource(r)
                    ipk = intended_pool_key(n)
                    if ipk and rtype != ipk:
                        return False
                return True

            if not is_valid_unique_mapping(new_mapping):
                # Try to repair by converting to a swap if possible
                # Find a node currently assigned to the desired resource and swap
                conflicts = {}
                for n, r in new_mapping.items():
                    conflicts.setdefault(r, []).append(n)
                # If any resource has >1 node, try to swap with a random node to restore uniqueness
                for r, ns in conflicts.items():
                    if len(ns) > 1:
                        # Swap the other nodes' resources with some other resource in the pool
                        # Pick one to keep, and swap others with a free resource if available
                        keep = ns[0]
                        for other in ns[1:]:
                            # Find a free resource in the pool for this node type
                            rtype = self.get_type(other)
                            pool_for_type = self.resource_pools.get(rtype, [])
                            used = set(new_mapping.values())
                            free_res = [rr for rr in pool_for_type if rr not in used]
                            if free_res:
                                new_mapping[other] = free_res[0]
                            else:
                                # fallback: swap with keep (meaning no change)
                                new_mapping[other] = new_mapping[other]
                if not is_valid_unique_mapping(new_mapping):
                    # Skip invalid mapping
                    continue

            # Calculate new cost
            new_cost = calculate_cost(new_mapping)
            cost_delta = new_cost - current_cost
            
            # Accept or reject the move
            if cost_delta < 0:
                # Better solution - always accept
                current_mapping = new_mapping
                current_cost = new_cost
                accepted_moves += 1
                stagnation = 0
                # Update best solution
                if current_cost < best_cost:
                    best_mapping = current_mapping.copy()
                    best_cost = current_cost
                    if best_cost == 0:
                        print(f"[Simulated Annealing] Found optimal solution at iteration {iteration}")
                        break
            else:
                # Worse solution - accept with probability based on temperature
                acceptance_prob = math.exp(-cost_delta / temperature) if temperature > 0 else 0
                if random.random() < acceptance_prob:
                    current_mapping = new_mapping
                    current_cost = new_cost
                    accepted_moves += 1
                    stagnation = 0
                else:
                    rejected_moves += 1
            
            # Cool down temperature
            temperature *= cooling_rate

            # Random restart if we've stagnated for many iterations
            if stagnation > 2000 and iteration % 500 == 0:
                current_mapping = random_initial_mapping()
                current_cost = calculate_cost(current_mapping)
                stagnation = 0
            else:
                # if nothing accepted this iter, increment stagnation
                stagnation += 1
                # print(f"[Simulated Annealing] Final results: best_cost={best_cost}, "
            #   f"accepted={accepted_moves}, rejected={rejected_moves}")
        
        if best_cost == 0:
            print(f"[Simulated Annealing] Success: Found valid mapping with 0 violations")
        else:
            print(f"[Simulated Annealing] Warning: Best mapping has total penalty {best_cost:.2f}")
            # Print which connections are violated
            print(f"[Simulated Annealing] Constraint violations:")
            for c in connections:
                src_orig, dst_orig = c["src"], c["dst"]
                
                # Try to find the mapping for the original node names first (with suffix)
                # If not found, try without suffix for ROW_LC/COL_LC nodes
                src = src_orig
                dst = dst_orig
                
                if src not in best_mapping and (src.endswith("ROW_LC") or src.endswith("COL_LC")):
                    src = src.split(".")[0]
                if dst not in best_mapping and (dst.endswith("ROW_LC") or dst.endswith("COL_LC")):
                    dst = dst.split(".")[0]
                
                if src in best_mapping and dst in best_mapping:
                    src_res = best_mapping[src]
                    dst_res = best_mapping[dst]
                    src_type, src_idx = self.parse_resource(src_res)
                    dst_type, dst_idx = self.parse_resource(dst_res)
                    
                    for constraint in self.constraints:
                        if not constraint.check(src_type, src_idx, dst_type, dst_idx):
                            print(f"  ✗ {src_orig} ({src_res}) -> {dst_orig} ({dst_res}): "
                                  f"{src_type}{src_idx} -> {dst_type}{dst_idx} violates {constraint.__class__.__name__}")
                            break
        
        # Post-processing: Enforce ROW_LC ↔ COL_LC correspondence
        # When GROUP i.ROW_LC maps to ROW_LC j, force GROUP i.COL_LC to map to COL_LC j
        for node in best_mapping:
            if ".ROW_LC" in node:
                group_prefix = node.split(".")[0]
                col_lc_node = f"{group_prefix}.COL_LC"
                row_lc_resource = best_mapping[node]  # e.g., "ROW_LC2"
                if row_lc_resource.startswith("ROW_LC"):
                    row_lc_idx = int(row_lc_resource[6:])
                    col_lc_resource = f"COL_LC{row_lc_idx}"
                    if col_lc_node in best_mapping:
                        if best_mapping[col_lc_node] != col_lc_resource:
                            print(f"[Post-processing] Fixing {col_lc_node}: {best_mapping[col_lc_node]} → {col_lc_resource}")
                            best_mapping[col_lc_node] = col_lc_resource
        
        self.node_to_resource = best_mapping
        self.last_mapping_cost = best_cost  # Store cost for later access
        return best_mapping

    def get(self, node: str) -> Optional[str]:
        """Return the physical resource assigned to a given node."""
        return self.node_to_resource.get(node)
    
    def get_last_mapping_cost(self) -> float:
        """Return the cost (penalty) of the last mapping search."""
        return getattr(self, 'last_mapping_cost', float('inf'))
    
    def register_module(self, resource: str, module: any):
        """Register a module object for a physical resource."""
        self.resource_to_module[resource] = module
    
    def get_module(self, resource: str) -> Optional[any]:
        """Get the module object for a physical resource."""
        return self.resource_to_module.get(resource)

    def summary(self):
        """Print all node-to-resource mappings."""
        print("=== Resource Mapping Summary ===")
        for node, res in self.node_to_resource.items():
            print(f"{node:25s} -> {res}")
        print("===============================")


class NodeGraph:
    """
    Singleton class representing logical nodes and their directed connections.

    It uses ResourceMapping to map logical nodes to hardware resources
    (LC, GROUP, AG, PE, etc.) for later scheduling or simulation.
    """
    _instance: Optional["NodeGraph"] = None

    def __init__(self, seed: Optional[int] = None):
        self.nodes: List[str] = []
        self.connections: List[Dict[str, str]] = []
        self.mapping = Mapper(seed=seed)  # Replaces node_to_resource
        self.node_metadata: Dict[str, Dict] = {}  # Store metadata like stream_type
        self.seed = seed

    @staticmethod
    def get() -> "NodeGraph":
        """Return the singleton instance of NodeGraph."""
        if NodeGraph._instance is None:
            NodeGraph._instance = NodeGraph()
        return NodeGraph._instance

    def add_node(self, name: str, **metadata):
        """Add a node to the graph with optional metadata (e.g., stream_type)."""
        if name not in self.nodes:
            self.nodes.append(name)
        if metadata:
            self.node_metadata[name] = metadata

    def connect(self, src: str, dst: str):
        """Add a directed connection (src → dst) if it doesn't already exist."""
        self.add_node(src)
        self.add_node(dst)
        connection = {"src": src, "dst": dst}
        if connection not in self.connections:
            self.connections.append(connection)
            
    def assign_node(self, node: str, resource: str):
        """Assign a specific physical resource to a logical node."""
        # Directly assign the resource without modification
        # (resource should already be in correct format like GROUP0, ROW_LC0, READ_STREAM0, etc.)
        self.mapping.node_to_resource[node] = resource
        self.mapping.assigned_node[node] = resource  # Mark as pre-assigned
        if node not in self.mapping.nodes:
            self.mapping.nodes.append(node)

    def allocate_resources(self, only_connected_nodes=False):
        """Allocate physical resources for all registered nodes.
        
        Args:
            only_connected_nodes: If True, only allocate resources for nodes that
                                 appear in connections. Other nodes will be skipped.
        """
        # Collect nodes that appear in connections (keep full node names)
        nodes_in_connections = set()
        if only_connected_nodes:
            for c in self.connections:
                src, dst = c["src"], c["dst"]
                # Keep full node names (including .ROW_LC, .COL_LC suffixes)
                nodes_in_connections.add(src)
                nodes_in_connections.add(dst)
        
        # Determine which nodes to process
        nodes_to_process = nodes_in_connections if only_connected_nodes else self.nodes
        
        for node in nodes_to_process:
            # Skip nodes that are already assigned (GROUP, ROW_LC, COL_LC, STREAM with target-based binding)
            if node in self.mapping.node_to_resource:
                continue
            
            # Get metadata for this node (e.g., stream_type)
            metadata = self.node_metadata.get(node, {})
            stream_type = metadata.get('stream_type')
            
            # Allocate with stream_type if available
            if stream_type:
                self.mapping.allocate(node, stream_type=stream_type)
            else:
                self.mapping.allocate(node)
            
    def direct_mapping(self):
        """Use direct logical→physical mapping without constraint search."""
        print(f"[Direct Mapping] Using {len(self.connections)} connections")
        for c in self.connections:
            print(f"  Connection: {c['src']} -> {c['dst']}")
        # Enable direct mapping mode in the mapper
        self.mapping.use_direct_mapping = True
        result = self.mapping.direct_mapping()
        print(f"[Success] Direct mapping completed with {len(result)} nodes")
    
    def heuristic_search_mapping(self, max_iterations: int = 5000, seed: Optional[int] = None):
        """Use simulated annealing to find a valid mapping for large graphs.
        
        Args:
            max_iterations: Maximum iterations for simulated annealing
            seed: Random seed for reproducibility (optional)
        
        Returns:
            float: The cost (penalty) of the best mapping found, or inf if search failed.
        """
        print(f"[Heuristic Search] Using {len(self.connections)} connections")
        for c in self.connections:
            print(f"  Connection: {c['src']} -> {c['dst']}")
        result = self.mapping.search(self.connections, max_iterations=max_iterations, 
                                              node_metadata=self.node_metadata, seed=seed)
        if result is None or len(result) == 0:
            print("[Warning] Heuristic search failed, keeping initial allocation")
            return float('inf')
        else:
            print(f"[Success] Heuristic search completed with {len(result)} nodes")
            self.mapping.node_to_resource = result
            # Return the cost of the best mapping found
            return self.mapping.get_last_mapping_cost()

    def summary(self):
        """Print nodes, connections, and their corresponding physical resources."""
        print("=== NodeGraph Summary ===")
        for c in self.connections:
            print(f"{c['src']} -> {c['dst']}")

        print(f"Total nodes: {len(self.nodes)}")
        print(f"Total connections: {len(self.connections)}\n")

        # Print resource allocation details
        self.allocate_resources()
        self.search_mapping()
        self.mapping.summary()
        visualize_mapping(self.mapping, self.connections)

def visualize_mapping(mapper, connections, save_path="data/placement.png"):
    """
    Visualize the physical layout of hardware resources with new 5-layer architecture:
    
    Row 4: LC (Row 0): LC0-LC7
    Row 3: LC (Row 1): LC8-LC15
    Row 2: ROW_LC (left): ROW_LC0-ROW_LC3, COL_LC (right): COL_LC0-COL_LC3
    Row 1: PE0-PE7
    Row 0: AG0-AG3

    The function draws mapped logical connections between resources.
    Output is saved to 'data/placement.png'.
    """
    import matplotlib
    matplotlib.use('Agg')  # Use non-interactive backend
    import matplotlib.pyplot as plt
    from matplotlib.patches import FancyArrowPatch
    from matplotlib.path import Path
    import os
    
    dir_path = os.path.dirname(save_path)
    if dir_path:  # Only create directory if path is not empty
        os.makedirs(dir_path, exist_ok=True)
    fig, ax = plt.subplots(figsize=(14, 8))

    # Define fixed layout positions for new 5-row architecture with 4 ROW_LC and 4 COL_LC
    # AG row now contains STREAM resources (READ_STREAM and WRITE_STREAM)
    layout = {
        "LC":           [(i * 2, 4) for i in range(8)] + [(i * 2, 3) for i in range(8)],   # Row 0: LC0-7, Row 1: LC8-15
        "ROW_LC":       [(i * 4, 2) for i in range(4)],                            # Row 2 left: ROW_LC0-3 (spaced out)
        "COL_LC":       [(i * 4 + 2, 2) for i in range(4)],                        # Row 2 right: COL_LC0-3 (spaced out)
        "PE":           [(i * 2, 1) for i in range(8)],                           # Row 1: PE0-7
        "READ_STREAM":  [(i * 4 + 1, 0) for i in range(3)],                       # Row 0: READ_STREAM0-2 (positions 1, 5, 9)
        "WRITE_STREAM": [(13, 0)],                                               # Row 0: WRITE_STREAM0 (position 13)
    }

    # Draw resource nodes
    for res_type, positions in layout.items():
        for i, (x, y) in enumerate(positions):
            if res_type == "LC":
                res_name = f"LC{i}"
            elif res_type == "ROW_LC":
                res_name = f"ROW_LC{i}"
            elif res_type == "COL_LC":
                res_name = f"COL_LC{i}"
            elif res_type == "PE":
                res_name = f"PE{i}"
            elif res_type == "READ_STREAM":
                res_name = f"READ_STREAM{i}"
            elif res_type == "WRITE_STREAM":
                res_name = f"WRITE_STREAM{i}"
            else:
                res_name = f"{res_type}{i}"

            # Reverse lookup: find which node is mapped to this resource
            node_name = next(
                (node for node, res in mapper.node_to_resource.items() if res == res_name),
                ""
            )

            # Use different colors for each resource type
            color_map = {
                "LC": "lightgreen",
                "ROW_LC": "lightyellow",
                "COL_LC": "lightyellow",
                "PE": "lightblue",
                "READ_STREAM": "lightcoral",
                "WRITE_STREAM": "salmon"
            }
            facecolor = color_map.get(res_type, "lightgray")
            label_text = res_type if i == 0 else ""

            ax.scatter(
                x, y, s=400, marker="s",
                label=label_text,
                edgecolor="black", facecolor=facecolor, zorder=3
            )
            
            # Logical node name
            if node_name:
                ax.text(
                    x, y - 0.2,
                    node_name,
                    ha="center", va="center",
                    fontsize=6, style="normal",
                    color="darkgreen"
                )
            else:
                ax.text(
                    x, y - 0.2,
                    "(unused)",
                    ha="center", va="center",
                    fontsize=6, style="normal",
                    color="gray"
                )

    # Helper: draw straight or curved connection
    def draw_connection(x1, y1, x2, y2, color="black", style="solid"):
        if y1 == y2:
            # Same-row connection → use a small curve
            mid_x = (x1 + x2) / 2
            offset = 0.15 + 0.08 * abs(x2 - x1)
            verts = [(x1, y1), (mid_x, y1 + offset), (x2, y2)]
            codes = [Path.MOVETO, Path.CURVE3, Path.CURVE3]
            path = Path(verts, codes)
            patch = FancyArrowPatch(
                path=path, arrowstyle="-|>", color=color,
                lw=1.0, alpha=0.6, mutation_scale=8, zorder=4
            )
            ax.add_patch(patch)
        else:
            # Different-row connection
            patch = FancyArrowPatch(
                (x1, y1), (x2, y2), arrowstyle="-|>",
                color=color, lw=1.0, alpha=0.6, mutation_scale=8, zorder=4
            )
            ax.add_patch(patch)

    # Draw all mapped connections
    for c in connections:
        src_node, dst_node = c["src"], c["dst"]
        
        # Keep full node names (including .ROW_LC, .COL_LC) to find their mappings
        src_res = mapper.node_to_resource.get(src_node)
        dst_res = mapper.node_to_resource.get(dst_node)
        
        if not src_res or not dst_res:
            continue
        
        if src_res == dst_res:
            continue  # skip self-loops

        # Parse resource type and index
        def parse_res_name(res):
            if res.startswith("LC"):
                idx = int(res[2:])
                return "LC", idx
            elif res.startswith("ROW_LC"):
                idx = int(res[6:])
                return "ROW_LC", idx
            elif res.startswith("COL_LC"):
                idx = int(res[6:])
                return "COL_LC", idx
            elif res.startswith("READ_STREAM"):
                idx = int(res[11:])
                return "READ_STREAM", idx
            elif res.startswith("WRITE_STREAM"):
                idx = int(res[12:])
                return "WRITE_STREAM", idx
            elif res.startswith("PE"):
                idx = int(res[2:])
                return "PE", idx
            else:
                return None, None

        src_type, src_idx = parse_res_name(src_res)
        dst_type, dst_idx = parse_res_name(dst_res)

        if src_type not in layout or dst_type not in layout:
            continue

        # Calculate position based on current layout
        if src_type == "LC":
            src_row, src_col = divmod(src_idx, 8)
            x1 = src_col * 2  # LC spacing: i * 2
            y1 = 4 - src_row  # Row 0 at y=4, Row 1 at y=3
        elif src_type in ["ROW_LC", "COL_LC"]:
            if src_type == "ROW_LC":
                x1 = src_idx * 4  # ROW_LC spacing: i * 4
            else:  # COL_LC
                x1 = src_idx * 4 + 2  # COL_LC spacing: i * 4 + 2
            y1 = 2
        else:
            x1, y1 = layout[src_type][src_idx]

        if dst_type == "LC":
            dst_row, dst_col = divmod(dst_idx, 8)
            x2 = dst_col * 2  # LC spacing: i * 2
            y2 = 4 - dst_row  # Row 0 at y=4, Row 1 at y=3
        elif dst_type in ["ROW_LC", "COL_LC"]:
            if dst_type == "ROW_LC":
                x2 = dst_idx * 4  # ROW_LC spacing: i * 4
            else:  # COL_LC
                x2 = dst_idx * 4 + 2  # COL_LC spacing: i * 4 + 2
            y2 = 2
        else:
            x2, y2 = layout[dst_type][dst_idx]

        draw_connection(x1, y1, x2, y2)

    # Configure axes and legend
    ax.set_title("Physical Resource Placement Visualization (5-Layer Architecture)", 
                 fontsize=14, weight="bold")
    ax.set_xlim(-1, 16)  # LC spacing goes from 0 to 14 (8 LCs * 2), ROW/COL_LC up to 14
    ax.set_ylim(-1, 5)
    ax.set_xticks([])
    ax.set_yticks([])
    plt.legend(loc="upper right", fontsize=9, frameon=True, markerscale=0.5, labelspacing=0.5)
    ax.grid(False)

    plt.tight_layout()
    plt.savefig(save_path, dpi=200)
    print(f"[Saved] Placement visualization written to: {save_path}")
