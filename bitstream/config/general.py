from bitstream.config.base import BaseConfigModule
from bitstream.index import NodeIndex, Connect
from typing import List, Optional
from bitstream.bit import Bit

class GAInportConfig(BaseConfigModule):
    """General Array inport configuration.
    
    Based on general_array.inport*.ga_inport_*:
    - enable(1) + src_id(3) + pingpong_en(1) + pingpong_last_index(3) + 
      fp16to32(1) + int32tofp(1) = 13 bits
    """
    FIELD_MAP = [
        ("enable", 8),  # ga_inport_enable
        ("src_id", 1, lambda self, x: Connect(x, self.id) if x else None),  # ga_inport_src_id (will be resolved)
        ("pingpong_en", 1),  # ga_inport_pingpong_en
        ("pingpong_last_index", 4),  # ga_inport_pingpong_last_index
        ("fp16to32", 1, lambda x: 1 if str(x).lower() == "true" else (0 if str(x).lower() == "false" else x)),  # ga_inport_fp16to32
        ("int32tofp", 1, lambda x: 1 if str(x).lower() == "true" else (0 if str(x).lower() == "false" else x)),  # ga_inport_int32tofp
    ]
    
    def __init__(self, idx: int):
        super().__init__()
        self.idx = idx
        self.id: Optional[NodeIndex] = None
    
    def from_json(self, cfg: dict):
        """Load from general_array.inport{idx}"""
        key = f"inport{self.idx}"
        if key in cfg:
            inport_cfg = cfg[key]
            # Create NodeIndex for src_id if present
            if "ga_inport_src_id" in inport_cfg:
                src_val = inport_cfg["ga_inport_src_id"]
                if src_val is not None and src_val != 0:
                    # Assume src_id refers to a stream or PE
                    self.id = NodeIndex(f"AG_INPORT{self.idx}")
            super().from_json(inport_cfg)

class GAOutportConfig(BaseConfigModule):
    """General Array outport configuration.
    
    Based on general_array.outport.ga_outport_*:
    - enable(1) + src_id(3) + fp32to16(1) + int32to8(1) = 6 bits
    """
    FIELD_MAP = [
        ("enable", 8),  # ga_outport_enable
        ("src_id", 1),  # ga_outport_src_id (direct index, not a node)
        ("fp32to16", 1, lambda x: 1 if str(x).lower() == "true" else (0 if str(x).lower() == "false" else x)),  # ga_outport_fp32to16
        ("int32to8", 1, lambda x: 1 if str(x).lower() == "true" else (0 if str(x).lower() == "false" else x)),  # ga_outport_int32to8
    ]
    
    def from_json(self, cfg: dict):
        """Load from general_array.outport"""
        cfg = cfg.get("outport", cfg)
        super().from_json(cfg)

class GAPEConfig(BaseConfigModule):
    """General Array PE configuration.
    
    Based on general_array.PE_array.PE**.ga_pe_*:
    - inport_enable[0:3](3) + src_id[0:3](9) + inport_mode[0:3](6) + 
      keep_last_index[0:3](9) + alu_opcode(2) + constant_value[0:3](36) + 
      constant_valid[0:3](3) = 68 bits
    """
    FIELD_MAP = [
        ("_padding", 2),  # Padding bits for alignment        
        # Source IDs (3 bits each, 9 bits total)
        ("inport2_src_id", 3),  # ga_pe_src_id[0]
        ("inport1_src_id", 3),  # ga_pe_src_id[1]
        ("inport0_src_id", 3),  # ga_pe_src_id[2]
        
        # Input modes (2 bits each, 6 bits total)
        ("inport2_mode", 2, lambda x: x if isinstance(x, int) else (GAPEConfig.inport_mode_map().get(x, 0) if x is not None else 0)),
        ("inport1_mode", 2, lambda x: x if isinstance(x, int) else (GAPEConfig.inport_mode_map().get(x, 0) if x is not None else 0)),
        ("inport0_mode", 2, lambda x: x if isinstance(x, int) else (GAPEConfig.inport_mode_map().get(x, 0) if x is not None else 0)),
        
        # Keep last indices (3 bits each, 9 bits total)
        ("inport2_keep_last_index", 4),  # ga_pe_keep_last_index[0]
        ("inport1_keep_last_index", 4),  # ga_pe_keep_last_index[1]
        ("inport0_keep_last_index", 4),  # ga_pe_keep_last_index[2]
        
        # ALU opcode (3 bits)
        ("alu_opcode", 3, lambda x: x if isinstance(x, int) else (GAPEConfig.opcode_map().get(x, 0) if x is not None else 0)),
        
        # Constants (12 bits each, 36 bits total)
        ("constant2", 32),  # ga_pe_constant_value[0]
        ("constant1", 32),  # ga_pe_constant_value[1]
        ("constant0", 32),  # ga_pe_constant_value[2]
    ]
    
    @classmethod
    def opcode_map(cls):
        """Map opcode names to integers"""
        return {
            "add": 0,
            "mul": 1,
            "mac": 2,
        }
    
    @classmethod
    def inport_mode_map(cls):
        """Map inport modes to integers"""
        return {
            "buffer": 0,
            "keep": 1,
            "constant": 2,
        }
    
    def __init__(self, pe_name: str):
        """Initialize with PE name (e.g., 'PE00', 'PE12')"""
        super().__init__()
        self.pe_name = pe_name
    
    def from_json(self, cfg: dict):
        """Load from general_array.PE_array.PE**
        
        Note: JSON arrays are in order [inport2, inport1, inport0] (high to low indices),
        so we need to reverse the mapping: json[0] -> inport2, json[1] -> inport1, json[2] -> inport0
        """
        if self.pe_name in cfg:
            pe_cfg = cfg[self.pe_name]
            
            # Unpack arrays into individual fields
            # Arrays in JSON are [2, 1, 0] order, so we reverse the mapping
            if "ga_pe_inport_enable" in pe_cfg:
                enables = pe_cfg["ga_pe_inport_enable"]
                if isinstance(enables, list):
                    self.values["inport2_enable"] = enables[0] if len(enables) > 0 else 0
                    self.values["inport1_enable"] = enables[1] if len(enables) > 1 else 0
                    self.values["inport0_enable"] = enables[2] if len(enables) > 2 else 0
            
            if "ga_pe_src_id" in pe_cfg:
                src_ids = pe_cfg["ga_pe_src_id"]
                if isinstance(src_ids, list):
                    self.values["inport2_src_id"] = src_ids[0] if len(src_ids) > 0 else 0
                    self.values["inport1_src_id"] = src_ids[1] if len(src_ids) > 1 else 0
                    self.values["inport0_src_id"] = src_ids[2] if len(src_ids) > 2 else 0
            
            if "ga_pe_inport_mode" in pe_cfg:
                modes = pe_cfg["ga_pe_inport_mode"]
                if isinstance(modes, list):
                    self.values["inport2_mode"] = modes[0] if len(modes) > 0 else None
                    self.values["inport1_mode"] = modes[1] if len(modes) > 1 else None
                    self.values["inport0_mode"] = modes[2] if len(modes) > 2 else None
            
            if "ga_pe_keep_last_index" in pe_cfg:
                indices = pe_cfg["ga_pe_keep_last_index"]
                if isinstance(indices, list):
                    self.values["inport2_keep_last_index"] = indices[0] if len(indices) > 0 else 0
                    self.values["inport1_keep_last_index"] = indices[1] if len(indices) > 1 else 0
                    self.values["inport0_keep_last_index"] = indices[2] if len(indices) > 2 else 0
            
            if "ga_pe_alu_opcode" in pe_cfg:
                self.values["alu_opcode"] = pe_cfg["ga_pe_alu_opcode"]
            
            if "ga_pe_constant_value" in pe_cfg:
                constants = pe_cfg["ga_pe_constant_value"]
                if isinstance(constants, list):
                    self.values["constant2"] = constants[0] if len(constants) > 0 else None
                    self.values["constant1"] = constants[1] if len(constants) > 1 else None
                    self.values["constant0"] = constants[2] if len(constants) > 2 else None
            
            if "ga_pe_constant_valid" in pe_cfg:
                valids = pe_cfg["ga_pe_constant_valid"]
                if isinstance(valids, list):
                    self.values["constant2_valid"] = valids[0] if len(valids) > 0 else 0
                    self.values["constant1_valid"] = valids[1] if len(valids) > 1 else 0
                    self.values["constant0_valid"] = valids[2] if len(valids) > 2 else 0
