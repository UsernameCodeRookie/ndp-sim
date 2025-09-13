from abc import ABC, abstractmethod
from bitstream.bit import Bit
from typing import List, Union, Tuple, Callable


class ConfigModule(ABC):
    """Abstract base class for all config modules."""

    @abstractmethod
    def from_json(self, cfg: dict):
        pass

    @abstractmethod
    def to_bits(self) -> list[Bit]:
        pass


class BaseConfigModule(ConfigModule):
    """Base implementation using FIELD_MAP and inheritance."""
    
    # FIELD_MAP: list of tuples (field_name, bit_width[, mapper])
    # mapper: function that converts the field value to int
    FIELD_MAP: List[Union[Tuple[str, int], Tuple[str, int, Callable]]] = []

    def __init__(self):
        self.values = {}
        for entry in self.FIELD_MAP:
            name = entry[0]
            self.values[name] = 0

    def from_json(self, cfg: dict):
        for entry in self.FIELD_MAP:
            name = entry[0]
            if name in cfg:
                self.values[name] = cfg[name]

    def _to_int(self, val, mapper: Callable = None):
        """Convert field value to int, using mapper if provided."""
        if mapper:
            return int(mapper(val))
        if isinstance(val, int):
            return val
        if isinstance(val, bool):
            return int(val)
        raise TypeError(f"Cannot convert value {val} of type {type(val)} to Bit")

    def to_bits(self) -> List[Bit]:
        bits: List[Bit] = []
        for entry in self.FIELD_MAP:
            name = entry[0]
            width = entry[1]
            mapper = entry[2] if len(entry) > 2 else None
            bits.append(Bit(self._to_int(self.values[name], mapper), width))
        return bits

class ConfigRegister(BaseConfigModule):
    """Top-level CONFIG register (3 bits)."""

    FIELD_MAP = [
        ("CONFIG", 3),
    ]

    def from_json(self, cfg: dict):
        self.values["CONFIG"] = int(cfg.get("CONFIG", 0), 2) if isinstance(cfg.get("CONFIG"), str) else cfg.get("CONFIG", 0)


class NeighborStreamConfig(BaseConfigModule):
    """Neighbor-to-neighbor stream config."""

    FIELD_MAP = [
        ("mem_loop", 4),
        ("mode", 1),
        ("stream_id", 2),
        ("src_slice_sel", 1),
        ("dst_slice_sel", 1),
        ("src_buf_ping_idx", 3),
        ("src_buf_pong_idx", 3),
        ("dst_buf_ping_idx", 3),
        ("dst_buf_pong_idx", 3),
    ]

    def from_json(self, cfg: dict):
        cfg = cfg.get("stream_engine", cfg)
        n2n = cfg.get("n2n", {})
        super().from_json(n2n)


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


class BufferConfig(BaseConfigModule):
    """Buffer manager config with index (buffer0, buffer1, ...)."""

    FIELD_MAP = [
        # rw: "read" -> 0, "write" -> 1
        ("rw", 1, lambda x: 0 if x == "read" else 1),
        ("dst_port", 2),
        ("enable", 1),
        ("buffer_life_time", 2),
        ("mode", 1),
        # "mask": "00001111" (binary string) -> 0b00001111 (int)
        ("mask", 8, lambda x: int(x, 2)),
    ]

    def __init__(self, idx: int):
        super().__init__()
        self.idx = idx  # buffer index

    def from_json(self, cfg: dict):
        """
        Load buffer config from JSON dict.
        Expected format:
        {
            "buffer0": { "rw": 1, "dst_port": 2, ... },
            "buffer1": { ... }
        }
        """
        
        cfg = cfg.get("buffer_config", cfg)
        key = f"buffer{self.idx}"
        if key in cfg and isinstance(cfg[key], dict):
            super().from_json(cfg[key])


class SpecialArrayConfig(BaseConfigModule):
    """Special array (PE) config."""

    FIELD_MAP = [
        # data_type: "fp16" -> 0, "fp32" -> 1
        ("data_type", 2, lambda x: 0 if x == "fp16" else 1),
        ("index_end", 3),
        ("inport0_enbale", 1),
        ("inport1_enbale", 1),
        ("inport2_enbale", 1),
        ("outport_enbale", 1),
        # outport_mode: "col" -> 0, "row" -> 1
        ("outport_mode", 1, lambda x: 0 if x == "col" else 1),
        # outport_fp32to16: "true" -> 1, "false" -> 0
        ("outport_fp32to16", 1, lambda x: 1 if str(x).lower() == "true" else 0),
    ]

    def from_json(self, cfg: dict):
        special = cfg.get("special_array", {})
        super().from_json(special)
