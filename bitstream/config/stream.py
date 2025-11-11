from bitstream.config.base import BaseConfigModule
from typing import List, Optional
from bitstream.bit import Bit
from bitstream.index import Connect, NodeIndex
from math import log2

class MemoryAGConfig0(BaseConfigModule):
    FIELD_MAP = [
        ("mode", 1, lambda x: 0 if x == "read" else 1),
        ("base_addr", 29),
        ("idx_size", 24),
        ("dim_stride", 60),
        ("padding_reg_value", 8),
        ("idx_padding_low", 36),
        ("idx_padding_up", 36),
        ("idx_tailing_low", 36),
        ("idx_tailing_up", 36),
        ("address_remapping", 64),
    ]

    def from_json(self, cfg: dict):
        cfg = cfg.get("memory_AG", cfg)
        super().from_json(cfg)

class StreamCtrlConfig0(BaseConfigModule):
    FIELD_MAP = [
        ("ping_pong", 1),
        ("pingpong_last_index", 3),
    ]

    def from_json(self, cfg: dict):
        cfg = cfg.get("stream", cfg)
        super().from_json(cfg)

class MemoryAGConfig1(BaseConfigModule):
    FIELD_MAP = [
        ("transcation_spatial_size_log", 9),
        ("transcation_total_size", 10),
        ("idx", 15, lambda self, lst: [Connect(x, self.id) if x else None for x in lst] if lst else None),
        ("idx_enable", 3),
        ("idx_keep_mode", 3, lambda lst: [MemoryAGConfig1.idx_keep_mode_map()[x] for x in lst] if lst else None),
        ("idx_keep_last_index", 9),
    ]
    
    def __init__(self, idx: int):
        """Initialize the MemoryAGConfig1 instance."""
        super().__init__()
        self.id : Optional[NodeIndex] = NodeIndex(f"STREAM{idx}.MEM_AG")

    def from_json(self, cfg: dict):
        cfg = cfg.get("memory_AG", cfg)
        super().from_json(cfg)
        
        idx_size = cfg.get("idx_size", None)
        
        if idx_size is not None:
            # idx_size from JSON is size-1, so add 1 to get actual size
            # But if it's 0, it means that dimension is disabled (size=1)
            actual_size = [(s + 1) if s is not None and s > 0 else 1 for s in idx_size]
            
            self.values["transcation_spatial_size_log"] = [
                int(log2(actual_size[2])) if actual_size[2] > 0 else 0, 
                int(log2(actual_size[1] * actual_size[2])) if actual_size[1] * actual_size[2] > 0 else 0, 
                int(log2(actual_size[0] * actual_size[1] * actual_size[2])) if actual_size[0] * actual_size[1] * actual_size[2] > 0 else 0
            ]
            self.values["transcation_total_size"] = actual_size[0] * actual_size[1] * actual_size[2]
            
    @staticmethod
    def idx_keep_mode_map():
        '''Map string/int idx_keep_mode names to integers matching hardware encoding.'''
        return {
            None: 0,
            0: 0,  # BUFFER
            1: 1,  # KEEP
            2: 2,  # CONSTANT
            "buffer": 0,
            "keep": 1,
            "constant": 2,
        }

class BufferAGConfig(BaseConfigModule):
    FIELD_MAP = [
        ("spatial_stride", 80),
        ("spatial_size", 5),
        ("idx_enable", 2),
        ("idx_keep_mode", 2, lambda lst: [BufferAGConfig.idx_keep_mode_map()[x] for x in lst] if lst else None),
        ("idx_keep_last_index", 6),
    ]

    def from_json(self, cfg: dict):
        cfg = cfg.get("buffer_AG", cfg)
        super().from_json(cfg)
        
    @staticmethod
    def idx_keep_mode_map():
        '''Map string/int idx_keep_mode names to integers matching hardware encoding.'''
        return {
            None: 0,
            0: 0,  # BUFFER
            1: 1,  # KEEP
            "buffer": 0,
            "keep": 1,
        }

class StreamCtrlConfig1(BaseConfigModule):
    FIELD_MAP = [
        ("ping_buffer", 3),
        ("pong_buffer", 3),
    ]

    def from_json(self, cfg: dict):
        cfg = cfg.get("stream", cfg)
        super().from_json(cfg)

class StreamConfig(BaseConfigModule):
    """Stream composed of multiple sub-config instances."""

    def __init__(self, idx: int):
        super().__init__()
        self.idx = idx
        # Each submodule is a config instance, order matters for to_bits()
        self.submodules: List[BaseConfigModule] = [
            MemoryAGConfig0(),
            StreamCtrlConfig0(),
            MemoryAGConfig1(idx),
            BufferAGConfig(),
            StreamCtrlConfig1(),
        ]

    def from_json(self, cfg: dict):
        cfg = cfg.get("stream_engine", cfg)
        key = f"stream{self.idx}"
        cfg = cfg.get(key, cfg)
        
        for submodule in self.submodules:
            # Let each submodule handle its own JSON parsing
            submodule.from_json(cfg)

    def to_bits(self) -> List[Bit]:
        """Concatenate all sub-config bitstreams in fixed order."""
        return sum((sub.to_bits() for sub in self.submodules), [])
