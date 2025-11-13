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
    def __init__(self):
        # Predefined physical resource pools
        self.resource_pools = {
            "LC":         [f"LC{i}" for i in range(8)],           # 8 LC resources
            "GROUP":      [f"GROUP{i}" for i in range(4)],        # 4 GROUP resources
            "AG":         [f"AG{i}" for i in range(4)],           # 4 AG (Address Generator) resources
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
        
    def get_type(self, node: str) -> Optional[str]:
        """Infer the resource type of a node based on its name prefix."""
        if node.startswith("DRAM_LC.LC"):
            return "LC"
        elif node.startswith("GROUP"):
            return "GROUP"
        elif node.startswith("STREAM"):
            # Stream type is determined during allocation based on JSON mode
            return "STREAM"  # Will be refined to READ_STREAM or WRITE_STREAM
        elif node.startswith("LC_PE"):
            return "PE"
        else:
            return None
        
    def get_type_from_resource(self, resource: str) -> Optional[str]:
        """Infer the resource type from a physical resource name."""
        if resource.startswith("LC"):
            return "LC"
        elif resource.startswith("GROUP"):
            return "GROUP"
        elif resource.startswith("READ_STREAM"):
            return "READ_STREAM"
        elif resource.startswith("WRITE_STREAM"):
            return "WRITE_STREAM"
        elif resource.startswith("PE"):
            return "PE"
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
            res_type = ''.join(ch for ch in resource if ch.isalpha())
            res_idx = int(''.join(ch for ch in resource if ch.isdigit()))
        
        return res_type, res_idx
    
    def extract_logical_index(self, node: str) -> Optional[int]:
        """Extract logical index from node name.
        
        Args:
            node: Node name like 'DRAM_LC.LC3', 'LC_PE.PE5', 'GROUP2', 'STREAM.stream1'
            
        Returns:
            Logical index as integer, or None if not found
        """
        import re
        # Match patterns like LC3, PE5, GROUP2, stream1
        if node.startswith("DRAM_LC.LC"):
            match = re.search(r'LC(\d+)$', node)
        elif node.startswith("LC_PE.PE"):
            match = re.search(r'PE(\d+)$', node)
        elif node.startswith("GROUP"):
            match = re.search(r'GROUP(\d+)', node)
        elif node.startswith("STREAM.stream"):
            match = re.search(r'stream(\d+)$', node)
        else:
            return None
        
        return int(match.group(1)) if match else None


    def allocate(self, node: str, stream_type: Optional[str] = None) -> str:
        """
        Allocate a physical resource for a node based on its type.
        If the node name indicates a special type (ROW_LC), it may reuse
        its parent GROUP's resource.
        
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

        # Special case: ROW_LC shares its parent GROUP's resource
        if node.endswith("ROW_LC") or node.endswith("COL_LC"):
            group_prefix = node.split(".")[0]  # e.g., GROUP0 from GROUP0.ROW_LC
            # Ensure the parent group is allocated and reuse its resource.
            parent_res = self.allocate(group_prefix)
            self.node_to_resource[node] = parent_res
            return parent_res
        
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
        """LC i → LC j constraint: j in [i-2, i-1, i+1, i+2]"""
        def check(self, src_type: str, src_idx: int, dst_type: str, dst_idx: int) -> bool:
            if src_type == "LC" and dst_type == "LC":
                return abs(dst_idx - src_idx) in [1, 2]
            return True

        def penalty(self, src_type: str, src_idx: int, dst_type: str, dst_idx: int) -> float:
            if src_type == "LC" and dst_type == "LC":
                d = abs(dst_idx - src_idx)
                if d in [1, 2]:
                    return 0.0
                # penalty proportional to distance beyond allowable window
                return float(max(0, d - 2))
            return 0.0
        
    class LCtoPEConstraint(Constraint):
        """LC i → PE j constraint: j in [i, i+1]"""
        def check(self, src_type: str, src_idx: int, dst_type: str, dst_idx: int) -> bool:
            if src_type == "LC" and dst_type == "PE":
                return abs(dst_idx - src_idx) in [0, 1]
            return True

        def penalty(self, src_type: str, src_idx: int, dst_type: str, dst_idx: int) -> float:
            if src_type == "LC" and dst_type == "PE":
                d = abs(dst_idx - src_idx)
                if d in [0, 1]:
                    return 0.0
                return float(d - 1)
            return 0.0

    class LCtoGROUPConstraint(Constraint):
        """LC i → GROUP j constraint: j in [i//2 - 1, i//2 + 1]"""
        def check(self, src_type: str, src_idx: int, dst_type: str, dst_idx: int) -> bool:
            if src_type == "LC" and dst_type == "GROUP":
                return abs(dst_idx - (src_idx // 2)) in [0, 1]
            return True

        def penalty(self, src_type: str, src_idx: int, dst_type: str, dst_idx: int) -> float:
            if src_type == "LC" and dst_type == "GROUP":
                d = abs(dst_idx - (src_idx // 2))
                if d in [0, 1]:
                    return 0.0
                return float(d - 1)
            return 0.0
        
    class LCtoSTREAMConstraint(Constraint):
        """LC i → STREAM j constraint: 
        Unified STREAM indexing: READ_STREAM0,1,2 → 0,1,2; WRITE_STREAM0 → 3
        """
        def check(self, src_type: str, src_idx: int, dst_type: str, dst_idx: int) -> bool:
            if src_type == "LC" and dst_type == "STREAM":
                # LC can connect to streams with reasonable topology constraints
                return abs(dst_idx - (src_idx // 2)) in [0, 1]
            return True

        def penalty(self, src_type: str, src_idx: int, dst_type: str, dst_idx: int) -> float:
            if src_type == "LC" and dst_type == "STREAM":
                d = abs(dst_idx - (src_idx // 2))
                if d in [0, 1]:
                    return 0.0
                return float(d - 1)
            return 0.0
    
    class PEtoPEConstraint(Constraint):
        """PE i → PE j constraint: j in [i-2, i-1, i+1, i+2]"""
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
    
    
    class PEtoSTREAMConstraint(Constraint):
        """PE i → STREAM j constraint:
        Unified STREAM indexing: READ_STREAM0,1,2 → 0,1,2; WRITE_STREAM0 → 3
        """
        def check(self, src_type: str, src_idx: int, dst_type: str, dst_idx: int) -> bool:
            if src_type == "PE" and dst_type == "STREAM":
                # PE can connect to streams with reasonable topology constraints
                return abs(dst_idx - (src_idx // 2)) in [0, 1]
            return True

        def penalty(self, src_type: str, src_idx: int, dst_type: str, dst_idx: int) -> float:
            if src_type == "PE" and dst_type == "STREAM":
                d = abs(dst_idx - (src_idx // 2))
                if d in [0, 1]:
                    return 0.0
                return float(d - 1)
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
    
    def heuristic_search(self, connections: List[Dict[str, str]], max_iterations: int = 5000, 
                        initial_temp: float = 100.0, cooling_rate: float = 0.995,
                        node_metadata: Optional[Dict[str, Dict]] = None,
                        repair_prob: float = 0.2) -> Optional[Dict[str, str]]:
        """
        Perform simulated annealing search to find a valid mapping for large graphs.
        Uses probabilistic optimization to escape local minima and find better solutions.
        
        Args:
            connections: List of connection dictionaries (e.g., {"src": ..., "dst": ...})
            max_iterations: Maximum number of optimization iterations (default: 5000)
            initial_temp: Initial temperature for simulated annealing (default: 100.0)
            cooling_rate: Temperature cooling rate per iteration (default: 0.995)
            node_metadata: Dictionary containing node metadata like stream_type (default: None)
        
        Returns:
            Dict[str, str]: Valid node→resource mapping if found; otherwise None
        """
        if node_metadata is None:
            node_metadata = {}
        import random
        import math
        
        print(f"[Simulated Annealing] Starting search with {len(connections)} connections")
        print(f"[Simulated Annealing] Parameters: iterations={max_iterations}, T0={initial_temp}, cooling={cooling_rate}, repair_prob={repair_prob}")
        
        # Define all structural constraints
        self.constraints: List[Mapper.Constraint] = [
            self.LCtoLCConstraint(),
            self.LCtoPEConstraint(),
            self.LCtoGROUPConstraint(),
            self.LCtoSTREAMConstraint(),
            self.PEtoPEConstraint(),
            self.PEtoSTREAMConstraint(),
        ]
        
        # Collect all nodes participating in connections
        nodes_in_connections = set()
        for c in connections:
            src, dst = c["src"], c["dst"]
            if src.endswith("ROW_LC") or src.endswith("COL_LC"):
                src = src.split(".")[0]
            if dst.endswith("ROW_LC") or dst.endswith("COL_LC"):
                dst = dst.split(".")[0]
            nodes_in_connections.add(src)
            nodes_in_connections.add(dst)
        
        unique_nodes = [node for node in self.nodes if node in nodes_in_connections]
        print(f"[Simulated Annealing] Processing {len(unique_nodes)} nodes")
        
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
        for node in unique_nodes:
            pkey = pool_key_for_node(node)
            if pkey:
                nodes_by_type[pkey].append(node)
        
        # Helper function to calculate cost (sum of constraint penalties)
        def calculate_cost(mapping: Dict[str, str]) -> float:
            cost = 0.0
            for c in connections:
                src, dst = c["src"], c["dst"]
                if src.endswith("ROW_LC") or src.endswith("COL_LC"):
                    src = src.split(".")[0]
                if dst.endswith("ROW_LC") or dst.endswith("COL_LC"):
                    dst = dst.split(".")[0]

                if src in mapping and dst in mapping:
                    src_res = mapping[src]
                    dst_res = mapping[dst]
                    src_type, src_idx = self.parse_resource(src_res)
                    dst_type, dst_idx = self.parse_resource(dst_res)

                    for constraint in self.constraints:
                        cost += constraint.penalty(src_type, src_idx, dst_type, dst_idx)
            return cost
        
        # Initialize with current allocation - allow randomized start to better explore
        # Start with a mapping that preserves node->resource uniqueness per type
        def random_initial_mapping():
            m = {}
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
                    # Shuffle pool and assign unique resources
                    shuffled = pool.copy()
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
                    if src not in mapping or dst not in mapping:
                        return 0.0
                    src_res = mapping[src]
                    dst_res = mapping[dst]
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
                    node = src_bad if random.random() < 0.5 else dst_bad
                    # Determine pool based on actual pool key for this node
                    pkey = pool_key_for_node(node)
                    pool = self.resource_pools.get(pkey, [])
                    # Try candidate resources and pick the one that minimizes total cost
                    best_local = None
                    best_local_cost = float('inf')
                    used = set(current_mapping.values())
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
                print(f"[Simulated Annealing] Final results: best_cost={best_cost}, "
              f"accepted={accepted_moves}, rejected={rejected_moves}")
        
        if best_cost == 0:
            print(f"[Simulated Annealing] Success: Found valid mapping with 0 violations")
        else:
            print(f"[Simulated Annealing] Warning: Best mapping has total penalty {best_cost:.2f}")
            # Print which connections are violated
            print(f"[Simulated Annealing] Constraint violations:")
            for c in connections:
                src, dst = c["src"], c["dst"]
                if src.endswith("ROW_LC") or src.endswith("COL_LC"):
                    src = src.split(".")[0]
                if dst.endswith("ROW_LC") or dst.endswith("COL_LC"):
                    dst = dst.split(".")[0]
                
                if src in best_mapping and dst in best_mapping:
                    src_res = best_mapping[src]
                    dst_res = best_mapping[dst]
                    src_type, src_idx = self.parse_resource(src_res)
                    dst_type, dst_idx = self.parse_resource(dst_res)
                    
                    for constraint in self.constraints:
                        if not constraint.check(src_type, src_idx, dst_type, dst_idx):
                            print(f"  ✗ {src} ({src_res}) -> {dst} ({dst_res}): "
                                  f"{src_type}{src_idx} -> {dst_type}{dst_idx} violates {constraint.__class__.__name__}")
                            break
        
        self.node_to_resource = best_mapping
        return best_mapping
    
    def search(self, connections: List[Dict[str, str]], timeout_seconds: int = 600) -> Optional[Dict[str, str]]:
        """
        Perform a backtracking search to find a valid mapping between nodes and resources
        that satisfies all architectural constraints.

        Args:
            connections (List[Dict[str, str]]): List of connection dictionaries (e.g., {"src": ..., "dst": ...})
            timeout_seconds (int): Maximum search duration in seconds (default: 600)

        Returns:
            Optional[Dict[str, str]]: Valid node→resource mapping if found; otherwise None
        """
        start_time = time.time()
        self._timeout = False

        # Define all structural constraints that must hold between mapped resources
        self.constraints: List[Mapper.Constraint] = [
            self.LCtoLCConstraint(),
            self.LCtoPEConstraint(),
            self.LCtoGROUPConstraint(),
            self.LCtoSTREAMConstraint(),
            self.PEtoPEConstraint(),
            self.PEtoSTREAMConstraint(),
        ]

        # === Collect all nodes participating in the connection graph ===
        nodes_in_connections = set()
        for c in connections:
            src, dst = c["src"], c["dst"]
            # Normalize LC suffixes (e.g., "NODE.ROW_LC" → "NODE")
            if src.endswith("ROW_LC") or src.endswith("COL_LC"):
                src = src.split(".")[0]
            if dst.endswith("ROW_LC") or dst.endswith("COL_LC"):
                dst = dst.split(".")[0]
            nodes_in_connections.add(src)
            nodes_in_connections.add(dst)

        # Debug: show which nodes are involved in constraints
        print(f"[Debug] Checking nodes in connections:")
        for node in sorted(nodes_in_connections):
            res = self.node_to_resource.get(node)
            res_type = self.get_type(node)
            print(f"  {node}: type={res_type}, mapped_to={res}")
        
        # Keep only nodes that are both known and connected
        unique_nodes = [node for node in self.nodes if node in nodes_in_connections]
        print(f"[Debug] Search has {len(unique_nodes)} nodes to map (from {len(nodes_in_connections)} nodes in connections)")
        print(f"[Debug] unique_nodes: {unique_nodes}")

        attempt_count = [0]  # mutable counter shared across recursion

        # === Recursive backtracking ===
        def backtrack(index: int, current_mapping: Dict[str, str], used_resources: Set[str]) -> Optional[Dict[str, str]]:
            attempt_count[0] += 1

            # Periodic progress logging
            if attempt_count[0] % 1000 == 0:
                print(f"[Debug] Search attempts: {attempt_count[0]}, at node index {index}/{len(unique_nodes)}")
            if attempt_count[0] == 1:
                print(f"[Debug] Starting backtrack with {len(unique_nodes)} nodes, initial used_resources: {used_resources}")
            
            # Check timeout condition
            if time.time() - start_time > timeout_seconds:
                if not self._timeout:
                    print(f"[Warning] Constraint search timeout after {timeout_seconds} seconds ({attempt_count[0]} attempts)")
                    self._timeout = True
                return None
            
            # === Base case: all nodes assigned ===
            if index >= len(unique_nodes):
                full_mapping = self.node_to_resource.copy()
                full_mapping.update(current_mapping)

                # Verify all constraints across connected pairs
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

                        # Check each constraint between these resources
                        failed = False
                        for constraint in self.constraints:
                            if not constraint.check(src_type, src_idx, dst_type, dst_idx):
                                failed = True
                                break
                        if failed:
                            return None  # violate constraint → backtrack

                # All constraints satisfied → success
                self.node_to_resource = full_mapping
                return full_mapping

            # === Recursive case: assign resource for next node ===
            node = unique_nodes[index]
            existing_res = self.node_to_resource.get(node)
            node_type = self.get_type(node) if not existing_res else self.get_type_from_resource(existing_res)
            
            if node_type is None:
                raise RuntimeError(f"[Error] Cannot allocate resource for node: {node}")
            
            pool = self.resource_pools.get(node_type, [])

            # Helper function to check if current assignment violates any constraints
            def check_current_assignment(test_node: str, test_res: str) -> bool:
                """Check if assigning test_res to test_node violates constraints with already assigned nodes."""
                full_mapping = self.node_to_resource.copy()
                full_mapping.update(current_mapping)
                full_mapping[test_node] = test_res
                
                # Get the set of nodes we've assigned so far (including test_node)
                assigned_nodes = set(unique_nodes[:index+1])
                
                for c in connections:
                    src, dst = c["src"], c["dst"]
                    if src.endswith("ROW_LC") or src.endswith("COL_LC"):
                        src = src.split(".")[0]
                    if dst.endswith("ROW_LC") or dst.endswith("COL_LC"):
                        dst = dst.split(".")[0]
                    
                    # Only check if both endpoints are in our search space and have been assigned
                    if src not in assigned_nodes or dst not in assigned_nodes:
                        continue
                    
                    if src not in full_mapping or dst not in full_mapping:
                        continue
                            
                    src_res = full_mapping[src]
                    dst_res = full_mapping[dst]
                    src_type, src_idx = self.parse_resource(src_res)
                    dst_type, dst_idx = self.parse_resource(dst_res)
                    
                    for constraint in self.constraints:
                        if not constraint.check(src_type, src_idx, dst_type, dst_idx):
                            return False
                return True
            
            # 1. Try existing resource (if defined and available)
            if existing_res and existing_res not in used_resources and existing_res in pool:
                if check_current_assignment(node, existing_res):
                    current_mapping[node] = existing_res
                    used_resources.add(existing_res)
                    solution = backtrack(index + 1, current_mapping, used_resources)
                    if solution:
                        return solution
                    used_resources.remove(existing_res)
                    del current_mapping[node]

            # 2. Try all alternative resources from the pool
            for res in pool:
                if res in used_resources or res == existing_res:
                    continue
                if check_current_assignment(node, res):
                    current_mapping[node] = res
                    used_resources.add(res)
                    solution = backtrack(index + 1, current_mapping, used_resources)
                    if solution:
                        return solution
                    used_resources.remove(res)
                    del current_mapping[node]

            # No valid assignment found for this path
            return None

        # Start the recursive search with empty used resources
        # All nodes should be in unique_nodes since we only allocated connected nodes
        return backtrack(0, {}, set())



    def get(self, node: str) -> Optional[str]:
        """Return the physical resource assigned to a given node."""
        return self.node_to_resource.get(node)
    
    def get_node_by_resource(self, resource: str) -> Optional[str]:
        """Reverse lookup: Return the logical node assigned to a physical resource."""
        for node, res in self.node_to_resource.items():
            if res == resource:
                return node
        return None
    
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
            if node.endswith("ROW_LC") or node.endswith("COL_LC"):
                continue
            print(f"{node:25s} -> {res}")
        print("===============================")


class NodeGraph:
    """
    Singleton class representing logical nodes and their directed connections.

    It uses ResourceMapping to map logical nodes to hardware resources
    (LC, GROUP, AG, PE, etc.) for later scheduling or simulation.
    """
    _instance: Optional["NodeGraph"] = None

    def __init__(self):
        self.nodes: List[str] = []
        self.connections: List[Dict[str, str]] = []
        self.mapping = Mapper()  # Replaces node_to_resource
        self.node_metadata: Dict[str, Dict] = {}  # Store metadata like stream_type

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

    def allocate_resources(self, only_connected_nodes=False):
        """Allocate physical resources for all registered nodes.
        
        Args:
            only_connected_nodes: If True, only allocate resources for nodes that
                                 appear in connections. Other nodes will be skipped.
        """
        # Collect nodes that appear in connections
        nodes_in_connections = set()
        if only_connected_nodes:
            for c in self.connections:
                src, dst = c["src"], c["dst"]
                # Normalize LC suffixes
                if src.endswith("ROW_LC") or src.endswith("COL_LC"):
                    src = src.split(".")[0]
                if dst.endswith("ROW_LC") or dst.endswith("COL_LC"):
                    dst = dst.split(".")[0]
                nodes_in_connections.add(src)
                nodes_in_connections.add(dst)
        
        # Determine which nodes to process
        nodes_to_process = nodes_in_connections if only_connected_nodes else self.nodes
        
        for node in nodes_to_process:
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
    
    def heuristic_search_mapping(self, max_iterations: int = 5000):
        """Use simulated annealing to find a valid mapping for large graphs."""
        print(f"[Heuristic Search] Using {len(self.connections)} connections")
        for c in self.connections:
            print(f"  Connection: {c['src']} -> {c['dst']}")
        result = self.mapping.heuristic_search(self.connections, max_iterations=max_iterations, 
                                              node_metadata=self.node_metadata)
        if result is None or len(result) == 0:
            print("[Warning] Heuristic search failed, keeping initial allocation")
        else:
            print(f"[Success] Heuristic search completed with {len(result)} nodes")
            self.mapping.node_to_resource = result
    
    def search_mapping(self):
        """Search for a valid node→resource mapping satisfying constraints."""
        print(f"[Debug] Starting constraint search with {len(self.connections)} connections")
        for c in self.connections:
            print(f"  Connection: {c['src']} -> {c['dst']}")
        result = self.mapping.search(self.connections, timeout_seconds=300)
        if result is None or len(result) == 0:
            print("[Warning] Constraint search failed or timed out, keeping initial allocation")
        else:
            print(f"[Success] Found valid mapping with {len(result)} nodes")
            self.mapping.node_to_resource = result

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
    Visualize the physical layout of hardware resources (LC, GROUP, PE, STREAM)
    and draw mapped logical connections between them.

    Each row represents a physical resource layer:
        Row 3: LC0-LC7
        Row 2: GROUP0-GROUP3
        Row 1: PE0-PE7
        Row 0: READ_STREAM0-2, WRITE_STREAM0

    The function automatically adjusts connection lines to avoid overlapping,
    especially for same-row (e.g., LC→LC) connections, by drawing smooth curves.

    Output is saved to 'data/placement.png'.
    """
    dir_path = os.path.dirname(save_path)
    if dir_path:  # Only create directory if path is not empty
        os.makedirs(dir_path, exist_ok=True)
    fig, ax = plt.subplots(figsize=(10, 6))

    # Define fixed layout positions
    layout = {
        "LC":           [(i, 3) for i in range(8)],               # LC0–LC7
        "GROUP":        [(i * 2 + 0.5, 2) for i in range(4)],     # between every 2 LCs
        "PE":           [(i, 1) for i in range(8)],               # aligned with LC
        "READ_STREAM":  [(i * 2 + 0.5, 0) for i in range(3)],     # READ_STREAM0-2
        "WRITE_STREAM": [(7, 0)],                                  # WRITE_STREAM0
    }

    # Draw resource nodes
    for res_type, positions in layout.items():
        for i, (x, y) in enumerate(positions):
            res_name = f"{res_type}{i}"

            # Reverse lookup: find which node is mapped to this resource
            node_name = next(
                (node for node, res in mapper.node_to_resource.items() if res == res_name),
                ""
            )

            # Use different colors for READ and WRITE streams
            if res_type == "READ_STREAM":
                facecolor = "lightblue"
                label_text = "READ_STREAM" if i == 0 else ""
            elif res_type == "WRITE_STREAM":
                facecolor = "lightcoral"
                label_text = "WRITE_STREAM"
            else:
                facecolor = "lightgray"
                label_text = res_type if i == 0 else ""

            ax.scatter(
                x, y, s=500, marker="s",
                label=label_text,
                edgecolor="black", facecolor=facecolor, zorder=3
            )
            
            # Logical node name (e.g., DRAM_LC.LC7)
            if node_name:
                ax.text(
                    x, y - 0.25,  # shift text slightly downward
                    node_name,
                    ha="center", va="center",
                    fontsize=9, style="normal",
                    color="darkgreen"
                )
            else:
                ax.text(
                    x, y - 0.25,
                    "(unused)",
                    ha="center", va="center",
                    fontsize=9, style="normal",
                    color="gray"
                )

    # Helper: draw straight or curved connection
    def draw_connection(x1, y1, x2, y2, color="black"):
        if y1 == y2:
            # Same-row connection → use a small curve above the row
            mid_x = (x1 + x2) / 2
            offset = 0.2 + 0.1 * abs(x2 - x1)  # curvature depends on distance
            verts = [(x1, y1), (mid_x, y1 + offset), (x2, y2)]
            codes = [Path.MOVETO, Path.CURVE3, Path.CURVE3]
            path = Path(verts, codes)
            patch = FancyArrowPatch(
                path=path, arrowstyle="-|>", color=color,
                lw=1.2, alpha=0.6, mutation_scale=10, zorder=4
            )
            ax.add_patch(patch)
        else:
            # Different-row connection → draw a straight arrow
            patch = FancyArrowPatch(
                (x1, y1), (x2, y2), arrowstyle="-|>",
                color=color, lw=1.2, alpha=0.6, mutation_scale=10, zorder=4
            )
            ax.add_patch(patch)

    # Draw all mapped connections
    for c in connections:
        src_node, dst_node = c["src"], c["dst"]
        
        if src_node.endswith("ROW_LC") or src_node.endswith("COL_LC"):
            src_node = src_node.split(".")[0]
        if dst_node.endswith("ROW_LC") or dst_node.endswith("COL_LC"):
            dst_node = dst_node.split(".")[0]
        
        src_res = mapper.node_to_resource.get(src_node)
        dst_res = mapper.node_to_resource.get(dst_node)
        
        if not src_res or not dst_res:
            continue
        
        if src_res is dst_res:
            continue  # skip self-loops

        # Handle READ_STREAM and WRITE_STREAM specially
        if src_res.startswith("READ_STREAM"):
            src_type = "READ_STREAM"
            src_idx = int(src_res[len("READ_STREAM"):])
        elif src_res.startswith("WRITE_STREAM"):
            src_type = "WRITE_STREAM"
            src_idx = int(src_res[len("WRITE_STREAM"):])
        else:
            src_type = ''.join(ch for ch in src_res if ch.isalpha())
            src_idx = int(''.join(ch for ch in src_res if ch.isdigit()))
        
        if dst_res.startswith("READ_STREAM"):
            dst_type = "READ_STREAM"
            dst_idx = int(dst_res[len("READ_STREAM"):])
        elif dst_res.startswith("WRITE_STREAM"):
            dst_type = "WRITE_STREAM"
            dst_idx = int(dst_res[len("WRITE_STREAM"):])
        else:
            dst_type = ''.join(ch for ch in dst_res if ch.isalpha())
            dst_idx = int(''.join(ch for ch in dst_res if ch.isdigit()))

        if src_type not in layout or dst_type not in layout:
            continue
        if src_idx >= len(layout[src_type]) or dst_idx >= len(layout[dst_type]):
            continue

        x1, y1 = layout[src_type][src_idx]
        x2, y2 = layout[dst_type][dst_idx]
        draw_connection(x1, y1, x2, y2)

    # Configure axes and legend
    ax.set_title("Physical Resource Placement Visualization", fontsize=14, weight="bold")
    ax.set_xlim(-1, 8)
    ax.set_ylim(-1, 4)
    ax.set_xticks([])
    ax.set_yticks([])
    plt.legend(loc="upper right", fontsize=9, frameon=True, markerscale=0.5, labelspacing=0.5)
    ax.grid(False)

    plt.tight_layout()
    plt.savefig(save_path, dpi=200)
    print(f"[Saved] Placement visualization written to: {save_path}")
