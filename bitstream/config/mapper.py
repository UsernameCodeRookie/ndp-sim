from typing import Dict, List, Optional, Set, Tuple
from collections import defaultdict
import os
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
            "LC":   [f"LC{i}" for i in range(8)],     # 8 LC resources
            "GROUP":[f"GROUP{i}" for i in range(4)],  # 4 GROUP resources
            "AG":   [f"AG{i}" for i in range(4)],     # 4 AG (Address Generator) resources
            "PE":   [f"PE{i}" for i in range(8)],     # 8 Processing Elements
        }

        # Track how many of each resource type have been used
        self.resource_counters = defaultdict(int)

        # Mapping: node name → assigned physical resource
        self.node_to_resource: Dict[str, str] = {}
        
        # List of all nodes registered for allocation
        self.nodes: List[str] = []
        
    def get_type(self, node: str) -> Optional[str]:
        """Infer the resource type of a node based on its name prefix."""
        if node.startswith("DRAM_LC.LC"):
            return "LC"
        elif node.startswith("GROUP"):
            return "GROUP"
        elif node.startswith("STREAM"):
            return "AG"
        elif node.startswith("LC_PE"):
            return "PE"
        else:
            return None

    def allocate(self, node: str) -> str:
        """
        Allocate a physical resource for a node based on its type.
        If the node name indicates a special type (ROW_LC), it may reuse
        its parent GROUP’s resource.
        """        
        # Return existing allocation if present
        if node in self.node_to_resource:
            return self.node_to_resource[node]
        
        res_type = self.get_type(node)

        # Non-hardware or unrecognized node type
        if res_type is None:
            self.node_to_resource[node] = "GENERIC"
            return "GENERIC"

        # Special case: ROW_LC shares its parent GROUP’s resource
        if node.endswith("ROW_LC") or node.endswith("COL_LC"):
            group_prefix = node.split(".")[0]  # e.g., GROUP0 from GROUP0.ROW_LC
            # If the parent group isn’t allocated yet, allocate it first
            return self.allocate(group_prefix)

        # Normal allocation — pick the next available resource from the pool
        idx = self.resource_counters[res_type]
        pool = self.resource_pools[res_type]
        self.nodes.append(node)

        # Pool exhausted check
        if idx >= len(pool):
            raise RuntimeError(f"[Error] {res_type} pool exhausted! (node: {node})")

        # Assign resource
        resource_name = pool[idx]
        self.node_to_resource[node] = resource_name
        self.resource_counters[res_type] += 1
        return resource_name
    
    class Constraint:
        """Base class for a connection-based constraint on physical resources."""
        def check(self, src_type: str, src_idx: int, dst_type: str, dst_idx: int) -> bool:
            """Return True if the connection satisfies the constraint."""
            raise NotImplementedError


    class LCtoLCConstraint(Constraint):
        """LC i → LC j constraint: j in [i-2, i-1, i+1, i+2]"""
        def check(self, src_type: str, src_idx: int, dst_type: str, dst_idx: int) -> bool:
            if src_type == "LC" and dst_type == "LC":
                return abs(dst_idx - src_idx) in [1, 2]
            return True  # not applicable


    class LCtoGROUPConstraint(Constraint):
        """LC i → GROUP j constraint: j = i / 4"""
        def check(self, src_type: str, src_idx: int, dst_type: str, dst_idx: int) -> bool:
            if src_type == "LC" and dst_type == "GROUP":
                return dst_idx == src_idx // 2
            return True


    class GROUPtoAGConstraint(Constraint):
        """GROUP i → AG j constraint: j = i"""
        def check(self, src_type: str, src_idx: int, dst_type: str, dst_idx: int) -> bool:
            if src_type == "GROUP" and dst_type == "AG":
                return dst_idx == src_idx
            return True

    
    def search(self, connections: List[Dict[str, str]]) -> Optional[Dict[str, str]]:
        """Backtracking search for a valid node→resource mapping."""
        self.constraints: List[Mapper.Constraint] = [
            self.LCtoLCConstraint(),
            self.LCtoGROUPConstraint(),
            self.GROUPtoAGConstraint(),
        ]

        def backtrack(index: int, current_mapping: Dict[str, str], used_resources: Set[str]) -> Optional[Dict[str, str]]:
            if index >= len(self.nodes):
                for c in connections:
                    src, dst = c["src"], c["dst"]
                    
                    if src.endswith("ROW_LC") or src.endswith("COL_LC"):
                        src = src.split(".")[0]
                    if dst.endswith("ROW_LC") or dst.endswith("COL_LC"):
                        dst = dst.split(".")[0]
                    
                    if src in current_mapping and dst in current_mapping:
                        src_type, src_idx = self.get_type(src), int(current_mapping[src][len(self.get_type(src)):])
                        dst_type, dst_idx = self.get_type(dst), int(current_mapping[dst][len(self.get_type(dst)):])
                        if not all(constraint.check(src_type, src_idx, dst_type, dst_idx) for constraint in self.constraints):
                            return None
                self.node_to_resource = current_mapping
                return current_mapping

            node = self.nodes[index]
            node_type = self.get_type(node)
            
            if node_type is None:
                raise RuntimeError(f"[Error] Cannot allocate resource for node: {node}")
            
            pool = self.resource_pools.get(node_type, [])

            for res in pool:
                if res in used_resources:
                    continue
                current_mapping[node] = res
                used_resources.add(res)
                solution = backtrack(index + 1, current_mapping, used_resources)
                if solution:
                    return solution
                used_resources.remove(res)
                del current_mapping[node]
            return None

        return backtrack(0, {}, set())


    def get(self, node: str) -> Optional[str]:
        """Return the physical resource assigned to a given node."""
        return self.node_to_resource.get(node)

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

    def __init__(self):
        self.nodes: List[str] = []
        self.connections: List[Dict[str, str]] = []
        self.mapping = Mapper()  # Replaces node_to_resource

    @staticmethod
    def get() -> "NodeGraph":
        """Return the singleton instance of NodeGraph."""
        if NodeGraph._instance is None:
            NodeGraph._instance = NodeGraph()
        return NodeGraph._instance

    def add_node(self, name: str):
        """Add a node to the graph if it does not already exist."""
        if name not in self.nodes:
            self.nodes.append(name)

    def connect(self, src: str, dst: str):
        """Add a directed connection (src → dst) if it doesn't already exist."""
        self.add_node(src)
        self.add_node(dst)
        connection = {"src": src, "dst": dst}
        if connection not in self.connections:
            self.connections.append(connection)

    def allocate_resources(self):
        """Allocate physical resources for all registered nodes."""
        for node in self.nodes:
            self.mapping.allocate(node)
            
    def search_mapping(self):
        """Search for a valid node→resource mapping satisfying constraints."""
        result = self.mapping.search(self.connections)
        if result is None or len(result) == 0:
            print("[Error] No valid resource mapping found!")
        # self.mapping.node_to_resource = result

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
    Visualize the physical layout of hardware resources (LC, GROUP, PE, AG)
    and draw mapped logical connections between them.

    Each row represents a physical resource layer:
        Row 3: LC0-LC7
        Row 2: GROUP0-GROUP3
        Row 1: PE0-PE7
        Row 0: AG0-AG3

    The function automatically adjusts connection lines to avoid overlapping,
    especially for same-row (e.g., LC→LC) connections, by drawing smooth curves.

    Output is saved to 'data/placement.png'.
    """
    os.makedirs(os.path.dirname(save_path), exist_ok=True)
    fig, ax = plt.subplots(figsize=(10, 6))

    # Define fixed layout positions
    layout = {
        "LC":    [(i, 3) for i in range(8)],               # LC0–LC7
        "GROUP": [(i * 2 + 0.5, 2) for i in range(4)],     # between every 2 LCs
        "PE":    [(i, 1) for i in range(8)],               # aligned with LC
        "AG":    [(i * 2 + 0.5, 0) for i in range(4)],     # aligned with GROUP
    }

    # Draw resource nodes
    for res_type, positions in layout.items():
        for i, (x, y) in enumerate(positions):
            res_name = f"{res_type}{i}"
            ax.scatter(x, y, s=500, marker="s", label=res_type if i == 0 else "",
                       edgecolor="black", facecolor="lightgray", zorder=3)
            ax.text(x, y, res_name, ha="center", va="center", fontsize=10, weight="bold")

    # Helper: draw straight or curved connection
    def draw_connection(x1, y1, x2, y2, color="steelblue"):
        if y1 == y2:
            # Same-row connection → use a small curve above the row
            mid_x = (x1 + x2) / 2
            offset = 0.2 + 0.1 * abs(x2 - x1)  # curvature depends on distance
            verts = [(x1, y1), (mid_x, y1 + offset), (x2, y2)]
            codes = [Path.MOVETO, Path.CURVE3, Path.CURVE3]
            path = Path(verts, codes)
            patch = FancyArrowPatch(
                path=path, arrowstyle="-|>", color=color,
                lw=1.2, alpha=0.6, mutation_scale=10, zorder=1
            )
            ax.add_patch(patch)
        else:
            # Different-row connection → draw a straight arrow
            patch = FancyArrowPatch(
                (x1, y1), (x2, y2), arrowstyle="-|>",
                color=color, lw=1.2, alpha=0.6, mutation_scale=10, zorder=1
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

        src_type = ''.join(ch for ch in src_res if ch.isalpha())
        dst_type = ''.join(ch for ch in dst_res if ch.isalpha())
        src_idx = int(''.join(ch for ch in src_res if ch.isdigit()))
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
    ax.legend(loc="upper right", fontsize=9)
    ax.grid(False)

    plt.tight_layout()
    plt.savefig(save_path, dpi=200)
    print(f"[Saved] Placement visualization written to: {save_path}")
