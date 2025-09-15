from bitstream.config.base import BaseConfigModule
from bitstream.index import NodeIndex
from typing import Optional

class DramLoopControlConfig(BaseConfigModule):
    """Represents a single DRAM loop configuration."""

    FIELD_MAP = [
        ("src_enable", 1),
        ("src_id", 8, lambda x: NodeIndex(x) if x else None),  # source node ID, resolved later
        ("outmost_loop", 1),
        ("start", 16),
        ("end", 16),
        ("stride", 8),
        ("last_index", 8),
    ]

    def __init__(self, idx : int):
        super().__init__()
        self.idx = idx
        self.id : Optional[NodeIndex] = None

    def set_empty(self):
        """Set all fields to None so that to_bits produces zeros."""
        for field_info in self.FIELD_MAP:
            name = field_info[0]
            self.values[name] = None  # None will encode as 0 in to_bits

    def from_json(self, cfg: dict):
        """
        Fill this loop control from JSON by picking the index-th entry
        that contains a 'stride' field.
        
        Args:
            cfg (dict): JSON dictionary containing loop configs.
            index (int): Index of the 'stride'-containing entry to pick.
        """
        cfg = cfg.get("dram_loop_configs", cfg)
        # Filter entries that contain 'stride'
        stride_entries = [(k, v) for k, v in sorted(cfg.items()) if "stride" in v]
        
        if self.idx < len(stride_entries):
            key, entry = stride_entries[self.idx]
            self.id = NodeIndex("DRAM_LC." + key)
            super().from_json(entry)
            
            self.values["src_enable"] = 1 if self.values.get("src_id") is not None else 0
        else:
            # No valid entry: treat as empty
            self.set_empty()
            
class LCPEConfig(BaseConfigModule):
    """Configuration for a PE connected to loop controls (LC_PE)."""

    FIELD_MAP = [
        ("inport", 24, lambda lst: [NodeIndex(x) if x else None for x in lst] if lst else None),
        ("inport_mode", 6, lambda lst: [LCPEConfig.inport_mode_map()[x] for x in lst] if lst else None),
        ("inport_last_index", 9),
        ("opcode", 4, lambda x: LCPEConfig.opcode_map()[x] if x is not None else 0),
        ("inport_enable", 3),
        ("constant_value", 24),
        ("constant_valid", 3),
    ]

    def __init__(self, idx: int):
        """
        Initialize PE config with a given index.
        The actual JSON entry will be picked in from_json() by index.
        """
        super().__init__()
        self.idx = idx
        self.id: Optional[NodeIndex] = None

    def set_empty(self):
        """Set all fields to None so that to_bits produces zeros."""
        for field_info in self.FIELD_MAP:
            name = field_info[0]
            self.values[name] = None  # None will encode as 0 in to_bits

    @staticmethod
    def opcode_map():
        """Map string opcode names to integers."""
        return {
            "add": 0,
            "mul": 1,
            "mac": 2,
        }
        
    @staticmethod
    def inport_mode_map():
        """Map string inport modes to integers."""
        return {
            None: 0,
            "buffer": 1,
            "keep": 2,
            "constant": 3,
        }

    def from_json(self, cfg: dict):
        """
        Fill this PE config from JSON by picking the index-th entry
        that contains 'opcode'.
        """
        cfg = cfg.get("dram_loop_configs", cfg)
        # Filter entries that contain 'opcode', sorted to maintain order
        opcode_entries = [(k, v) for k, v in sorted(cfg.items()) if "opcode" in v]

        if self.idx < len(opcode_entries):
            key, entry = opcode_entries[self.idx]
            # Assign NodeIndex
            self.id = NodeIndex(key)
            super().from_json(entry)
        else:
            # No valid entry: treat as empty
            self.set_empty()

class BufferLoopControlConfig(BaseConfigModule):
    """Represents a single buffer loop configuration (row or column)."""

    FIELD_MAP = [
        ("src_id", 8, lambda x: NodeIndex(x) if x else None),  # source node ID
        ("start", 16),
        ("end", 16),
        ("stride", 8),
        ("last_index", 8),
    ]

    def __init__(self, idx: int, lc_type: str):
        """
        Initialize loop control with index. The actual JSON entry
        is selected in from_json() based on index of 'stride'-containing entries.
        """
        super().__init__()
        self.idx = idx
        self.id: Optional[NodeIndex] = None
        self.lc_type = "ROW_LC" if lc_type == "row" else "COL_LC"

    def set_empty(self):
        """Set all fields to None so that to_bits produces zeros."""
        for field_info in self.FIELD_MAP:
            field_name = field_info[0]
            self.values[field_name] = None

    def from_json(self, cfg: dict):
        """
        Pick the idx-th entry with 'stride' field from ROW_LC or COL_LC.
        
        Args:
            cfg (dict): JSON dictionary containing buffer_loop_configs.
        """
        sub_cfg = cfg.get("buffer_loop_configs", {}).get(self.lc_type, {})
        stride_entries = [(k, v) for k, v in sorted(sub_cfg.items()) if "stride" in v]

        if self.idx < len(stride_entries):
            key, entry = stride_entries[self.idx]
            self.id = NodeIndex(f"{self.lc_type}.{key}")
            super().from_json(entry)
        else:
            # No valid entry: treat as empty
            self.set_empty()