from bitstream.config.base import BaseConfigModule

from bitstream.config.base import BaseConfigModule
from typing import List
from bitstream.bit import Bit

class SpecialConfig(BaseConfigModule):
    FIELD_MAP = [
        ("data_type", 2, lambda x: 0 if x == "fp16" else 1),
        ("index_end", 3),
    ]

class InportConfig(BaseConfigModule):
    FIELD_MAP = [
        ("enable", 1),
        ("pingpong_en", 1),
        ("pingpong_last_index", 3),
        ("src_ping_id", 3),
        ("src_pong_id", 3),
    ]
    
    def __init__(self, idx: int):
        super().__init__()
        self.idx = idx  # buffer index
        
    def from_json(self, cfg: dict):
        key = f"inport{self.idx}"
        cfg = cfg.get(key, cfg)
        super().from_json(cfg)

class OutportConfig(BaseConfigModule):
    FIELD_MAP = [
        ("enable", 1),
        ("mode", 1, lambda x: 0 if x == "col" else 1),
        ("fp32to16", 1, lambda x: 1 if str(x).lower() == "true" else 0),
    ]
    
    def from_json(self, cfg: dict):
        cfg = cfg.get("outport", cfg)
        super().from_json(cfg)
        
class SpecialArrayConfig(BaseConfigModule):
    """Special array composed of PE, multiple inports, and one outport."""

    def __init__(self):
        super().__init__()
        # submodules: (json_key, module_instance)
        self.submodules = [SpecialConfig()] + [InportConfig(i) for i in range(3)] + [OutportConfig()]

    def from_json(self, cfg: dict):
        cfg = cfg.get("special_array", cfg)
        for submodule in self.submodules:
            submodule.from_json(cfg)

    def to_bits(self) -> List[Bit]:
        """Concatenate all sub-config bitstreams in fixed order."""
        return sum((sub.to_bits() for sub in self.submodules), [])