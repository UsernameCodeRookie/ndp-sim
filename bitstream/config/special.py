from bitstream.config.base import BaseConfigModule

from bitstream.config.base import BaseConfigModule
from typing import List
from bitstream.bit import Bit

class SpecialConfig(BaseConfigModule):
    # Based on register_map:
    # data_type(2) + index_end(3) = 5 bits
    FIELD_MAP = [
        ("data_type", 2, lambda x: SpecialConfig.data_type_map()[x] if x is not None else 0),
        ("index_end", 3),  # sa_pe_config_last_index in hardware
    ]
    
    @classmethod
    def data_type_map(cls):
        return {
            "int8": 0,
            "fp16": 1,
        }

class InportConfig(BaseConfigModule):
    # Based on register_map:
    # enable(1) + pingpong_en(1) + pingpong_last_index(3) = 5 bits (per inport)
    FIELD_MAP = [
        ("enable", 1),  # sa_inport_enable
        ("pingpong_en", 1),  # sa_inport_pingpong_en
        ("pingpong_last_index", 3),  # sa_inport_pingpong_last_index
    ]
    
    def __init__(self, idx: int):
        super().__init__()
        self.idx = idx  # buffer index
        
    def from_json(self, cfg: dict):
        key = f"inport{self.idx}"
        cfg = cfg.get(key, cfg)
        super().from_json(cfg)

class OutportConfig(BaseConfigModule):
    # Based on register_map:
    # mode(1) + fp32to16(1) = 2 bits
    FIELD_MAP = [
        ("mode", 1, lambda x: 1 if x == "col" else 0),  # sa_outport_major: col=1, row=0
        ("fp32to16", 1, lambda x: 1 if str(x).lower() == "true" else 0),  # sa_outport_fp32to16
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