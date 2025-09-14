from bitstream.config.base import BaseConfigModule
from typing import List
from bitstream.bit import Bit

class MemoryAGConfig(BaseConfigModule):
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
        ("ping_buffer", 3),
        ("pong_buffer", 3),
    ]


class StreamCtrlConfig(BaseConfigModule):
    FIELD_MAP = [
        ("ping_pong", 1),
        ("pingpong_last_index", 3),
    ]


class StreamConfig(BaseConfigModule):
    """A stream is composed of multiple sub-configs: memory_AG, buffer_AG, stream control."""

    def __init__(self, idx: int):
        super().__init__()
        self.idx = idx
        self.memory_ag = MemoryAGConfig()
        self.buffer_ag = BufferAGConfig()
        self.ctrl = StreamCtrlConfig()

    def from_json(self, cfg: dict):
        """
        Expected JSON structure (order may vary):
        {
            "stream0": {
                "memory_AG": {...},
                "buffer_AG": {...},
                "stream": {...}
            },
            "stream1": {...}
        }
        """
        
        cfg = cfg.get("stream_engine", cfg)
        key = f"stream{self.idx}"
        if key not in cfg:
            return
        stream_cfg = cfg[key]

        if "memory_AG" in stream_cfg:
            self.memory_ag.from_json(stream_cfg["memory_AG"])
        if "buffer_AG" in stream_cfg:
            self.buffer_ag.from_json(stream_cfg["buffer_AG"])
        if "stream" in stream_cfg:
            self.ctrl.from_json(stream_cfg["stream"])

    def to_bits(self) -> List[Bit]:
        """Concatenate all sub-config bitstreams."""
        return self.memory_ag.to_bits() + self.buffer_ag.to_bits() + self.ctrl.to_bits()