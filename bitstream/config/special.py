from bitstream.config.base import BaseConfigModule
from typing import List
from bitstream.bit import Bit

class InportConfig(BaseConfigModule):
    # Based on component_config/special_array.py:
    # enable(1) + pingpong_en(1) + pingpong_last_index(3) = 5 bits (per inport)
    FIELD_MAP = [
        ("mode", 1),
        ("enable", 1),  # sa_inport_enable
        ("pingpong_en", 1),  # sa_inport_pingpong_en
        ("pingpong_last_index", 4),  # sa_inport_pingpong_last_index
        ("nbr_enable", 1),
    ]
    
    def __init__(self, idx: int):
        super().__init__()
        self.idx = idx
        
    def from_json(self, cfg: dict):
        key = f"inport{self.idx}"
        cfg = cfg.get(key, cfg)
        super().from_json(cfg)

class PEConfig(BaseConfigModule):
    FIELD_MAP = [
        ("data_type", 2, lambda x: PEConfig.data_type_map().get(x, x) if isinstance(x, str) else (x if x is not None else 0)),  # sa_pe_data_type
        ("transout_last_index", 4),  # sa_pe_transout_last_index in hardware
        ("bias_enable", 1),  # sa_pe_bias_enable (default 0)
    ]
    
    @classmethod
    def data_type_map(cls):
        return {
            "int8": 0,
            "fp16": 2,
            "bf16": 3,
        }
    
    def from_json(self, cfg: dict):
        super().from_json(cfg)

class OutportConfig(BaseConfigModule):
    # Based on component_config/special_array.py:
    # outport_major(1) + fp32to16(1) = 2 bits
    FIELD_MAP = [
        ("mode", 1, lambda x: 0 if x == "col" else (1 if x == "row" else x)),  # sa_outport_major: col=0, row=1, or pass through int
        ("fp32to16", 1, lambda x: 1 if str(x).lower() == "true" else (0 if str(x).lower() == "false" else x)),  # sa_outport_fp32to16
        ("fp32tobf16", 1, lambda x: 1 if str(x).lower() == "true" else (0 if str(x).lower() == "false" else x)),  # sa_outport_fp32tobf16
    ]
    
    def from_json(self, cfg: dict):
        cfg = cfg.get("outport", cfg)
        super().from_json(cfg)
        
class SpecialArrayConfig(BaseConfigModule):
    """Special array composed of PE, multiple inports, and one outport.
    
    Bit order (high to low, matching component_config/special_array.py):
    - inport2 (5 bits)
    - inport1 (5 bits)
    - inport0 (5 bits)
    - inport neighbor (1 bit)
    - PE config (6 bits)
    - outport (2 bits)
    Total: 24 bits
    """

    def __init__(self):
        super().__init__()
        # Order matters! Must match component_config bit order (high to low)
        self.submodules = [
            InportConfig(2),  # inport2 first (high bits)
            InportConfig(1),  # inport1
            InportConfig(0),  # inport0
            PEConfig(),       # PE config
            OutportConfig(),  # outport last (low bits)
        ]

    def from_json(self, cfg: dict):
        cfg = cfg.get("special_array", cfg)
        for submodule in self.submodules:
            submodule.from_json(cfg)

    def to_bits(self) -> List[Bit]:
        """Concatenate all sub-config bitstreams in fixed order."""
        return sum((sub.to_bits() for sub in self.submodules), [])
