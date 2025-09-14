from bitstream.config.base import BaseConfigModule
from typing import List
from bitstream.bit import Bit
from bitstream.config.index import NodeIndex

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
        ("idx", 15, lambda lst: [NodeIndex(x) if x else None for x in lst]),
        ("idx_enable", 3),
        ("idx_keep_mode", 3),
        ("idx_keep_last_index", 9),
    ]

    def from_json(self, cfg: dict):
        cfg = cfg.get("memory_AG", cfg)
        super().from_json(cfg)

class BufferAGConfig(BaseConfigModule):
    FIELD_MAP = [
        ("spatial_stride", 80),
        ("spatial_size", 5),
        ("idx_enable", 2),
        ("idx_keep_mode", 2),
        ("idx_keep_last_index", 6),
    ]

    def from_json(self, cfg: dict):
        cfg = cfg.get("buffer_AG", cfg)
        super().from_json(cfg)

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
            MemoryAGConfig1(),
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
