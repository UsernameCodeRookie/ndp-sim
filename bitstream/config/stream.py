from bitstream.config.base import BaseConfigModule
from typing import List
from bitstream.bit import Bit

class MemoryAGConfig0(BaseConfigModule):
    FIELD_MAP = [
        ("mode", 1),
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
    
class StreamCtrlConfig0(BaseConfigModule):
    FIELD_MAP = [
        ("ping_pong", 1),
        ("pingpong_last_index", 3),
    ]
    
class MemoryAGConfig1(BaseConfigModule):
    FIELD_MAP = [
        ("idx", 15),
        ("idx_enable", 3),
        ("idx_keep_mode", 3),
        ("idx_keep_last_index", 9),
    ]


class BufferAGConfig(BaseConfigModule):
    FIELD_MAP = [
        ("spatial_stride", 80),
        ("spatial_size", 5),
        ("idx_enable", 2),
        ("idx_keep_mode", 2),
        ("idx_keep_last_index", 6),
    ]


class StreamCtrlConfig1(BaseConfigModule):
    FIELD_MAP = [
        ("ping_buffer", 3),
        ("pong_buffer", 3),
    ]


class StreamConfig(BaseConfigModule):
    """A stream is composed of multiple sub-configs: memory_AG, buffer_AG, stream control."""

    def __init__(self, idx: int):
        super().__init__()
        self.idx = idx
        # submodules: (json_key, module_instance)
        self.submodules = [
            ("memory_AG", MemoryAGConfig0()),
            ("stream", StreamCtrlConfig0()),
            ("memory_AG", MemoryAGConfig1()),
            ("buffer_AG", BufferAGConfig()),
            ("stream", StreamCtrlConfig1()),
        ]

    def from_json(self, cfg: dict):
        cfg = cfg.get("stream_engine", cfg)
        key = f"stream{self.idx}"
        if key not in cfg:
            return
        stream_cfg = cfg[key]

        for json_key, sub in self.submodules:
            if json_key in stream_cfg:
                sub.from_json(stream_cfg[json_key])

    def to_bits(self) -> List[Bit]:
        """Concatenate all sub-config bitstreams in fixed order."""
        return sum((sub.to_bits() for _, sub in self.submodules), [])