from bitstream.config.base import BaseConfigModule
from typing import List, Optional
from bitstream.bit import Bit
from bitstream.index import Connect, NodeIndex
from bitstream.config.mapper import NodeGraph
from math import log2


class ReadStreamEngineConfig(BaseConfigModule):
    """
    Read stream engine configuration with padding and tailing fields.
    This is a submodule of StreamConfig.
    FIELD_MAP uses JSON field names directly from stream_engine format.
    """
    
    FIELD_MAP = [
        # Memory AG fields
        ("mem_idx_mode", 6, lambda x: [StreamConfig.inport_mode_map().get(i, 0) for i in x] if isinstance(x, list) else x),
        ("mem_idx_keep_last_index", 12),
        ("idx", 15),
        ("mem_idx_constant", 24),
        # Buffer AG fields
        ("buf_idx_mode", 2, lambda x: [StreamConfig.buffer_mode_map().get(i, 0) for i in x] if isinstance(x, list) else x),
        ("buf_idx_keep_last_index", 8),
        # Stream fields
        ("ping_pong", 1),
        ("pingpong_last_index", 4),
        # Address and size fields
        ("base_addr", 29),
        ("idx_size", 24),
        ("idx_size_log", 9),
        ("total_size", 8),
        ("dim_stride", 60),
        # Remapping
        ("address_remapping", 64, lambda lst: lst[::-1] if isinstance(lst, list) else StreamConfig.address_remapping_default),
        # Padding fields
        ("padding_reg_value", 8),
        ("padding_enable", 3),
        ("idx_padding_range", 72),
        # Tailing (branch) fields
        ("tailing_enable", 3),
        ("idx_tailing_range", 72),
        # Spatial fields
        ("buf_spatial_stride", 80, lambda lst: lst[::-1] if isinstance(lst, list) else None),
        ("buf_spatial_size", 5),
        ("buf_full_last_index", 4),
    ]
    
    def __init__(self, stream_key: str):
        super().__init__()
        self.stream_key = stream_key
        self.id: Optional[NodeIndex] = None
    
    @property
    def physical_index(self) -> int:
        """Get physical index from NodeIndex.physical_id."""
        if self.id is not None:
            return self.id.physical_id
        return 0
    
    def from_json(self, cfg: dict):
        """Load read stream configuration from JSON."""
        self.id = NodeIndex(f"STREAM.{self.stream_key}", stream_type="read")
        
        # Pre-process nested dict fields into lists
        if "idx_padding_range" in cfg and isinstance(cfg["idx_padding_range"], dict):
            padding_range = cfg["idx_padding_range"]
            cfg = {**cfg, "idx_padding_range": padding_range.get("low_bound", []) + padding_range.get("up_bound", [])}
        
        if "idx_tailing_range" in cfg and isinstance(cfg["idx_tailing_range"], dict):
            tailing_range = cfg["idx_tailing_range"]
            cfg = {**cfg, "idx_tailing_range": tailing_range.get("low", []) + tailing_range.get("up", [])}
        
        # Pre-process idx field to convert string node names to Connect objects
        if "idx" in cfg and isinstance(cfg["idx"], list):
            idx_list = []
            for item in cfg["idx"]:
                if isinstance(item, str):
                    idx_list.append(Connect(item, self.id))
                else:
                    idx_list.append(item)
            cfg = {**cfg, "idx": idx_list}
        
        super().from_json(cfg)
        
        idx_size : list = cfg.get("idx_size", [])
        
        dim_size = [0, 0, 0]
        for i in range(3):
            dim_size[i] = idx_size[i] + 1 if idx_size[i] is not None else 1
        dim0 = dim_size[0]
        dim1 = dim_size[1] * dim0
        dim2 = dim_size[2] * dim1
        
        self.values["total_size"] = dim2
        self.values["idx_size_log"] = [int(log2(dim0)), int(log2(dim1)), 0]

    def set_empty(self):
        """Set to empty configuration."""
        self.mark_empty()

class WriteStreamEngineConfig(BaseConfigModule):
    """
    Write stream engine configuration without padding and tailing fields.
    This is a submodule of StreamConfig.
    FIELD_MAP uses JSON field names directly from stream_engine format.
    """
    
    FIELD_MAP = [
        ("_padding", 1),
        # Memory AG fields
        ("mem_idx_mode", 6, lambda x: [StreamConfig.inport_mode_map().get(i, 0) for i in x] if isinstance(x, list) else x),
        ("mem_idx_keep_last_index", 12),
        ("idx", 15),
        ("mem_idx_constant", 24),
        # Buffer AG fields
        ("buf_idx_mode", 2, lambda x: [StreamConfig.buffer_mode_map().get(i, 0) for i in x] if isinstance(x, list) else x),
        ("buf_idx_keep_last_index", 8),
        # Stream fields
        ("ping_pong", 1),
        ("pingpong_last_index", 4),
        # Address and size fields
        ("base_addr", 29),
        ("idx_size", 24),
        ("idx_size_log", 9),
        ("total_size", 8),
        ("dim_stride", 60),
        # Remapping
        ("address_remapping", 64, lambda lst: lst[::-1] if isinstance(lst, list) else StreamConfig.address_remapping_default),
        # Tailing (branch) fields
        ("tailing_enable", 3),
        ("idx_tailing_range", 72),
        # Spatial fields
        ("buf_spatial_stride", 80, lambda lst: lst[::-1] if isinstance(lst, list) else None),
        ("buf_spatial_size", 5),
    ]
    
    def __init__(self, stream_key: str):
        super().__init__()
        self.stream_key = stream_key
        self.id: Optional[NodeIndex] = None
    
    @property
    def physical_index(self) -> int:
        """Get physical index from NodeIndex.physical_id."""
        if self.id is not None:
            return self.id.physical_id
        return 0
    
    def from_json(self, cfg: dict):
        """Load write stream configuration from JSON."""
        self.id = NodeIndex(f"STREAM.{self.stream_key}", stream_type="write")
        
        if "idx_tailing_range" in cfg and isinstance(cfg["idx_tailing_range"], dict):
            tailing_range = cfg["idx_tailing_range"]
            cfg = {**cfg, "idx_tailing_range": tailing_range.get("low", []) + tailing_range.get("up", [])}
        
        # Pre-process idx field to convert string node names to Connect objects
        if "idx" in cfg and isinstance(cfg["idx"], list):
            idx_list = []
            for item in cfg["idx"]:
                if isinstance(item, str):
                    idx_list.append(Connect(item, self.id))
                else:
                    idx_list.append(item)
            cfg = {**cfg, "idx": idx_list}
        
        super().from_json(cfg)
        
        idx_size : list = cfg.get("idx_size", [])
        
        dim_size = [0, 0, 0]
        for i in range(3):
            dim_size[i] = idx_size[i] + 1 if idx_size[i] is not None else 1
        dim0 = dim_size[0]
        dim1 = dim_size[1] * dim0
        dim2 = dim_size[2] * dim1
        
        self.values["total_size"] = dim2
        self.values["idx_size_log"] = [int(log2(dim0)), int(log2(dim1)), 0]


class StreamConfig(BaseConfigModule):
    """
    Container for stream configurations.
    Similar to BufferLoopControlGroupConfig, this holds submodules.
    Determines read/write type from JSON and creates appropriate submodule.
    
    Can be initialized with either an index (int) or stream_key (str).
    """
    
    address_remapping_default : list = [15, 14, 13, 12, 11, 10, 9, 8,
                                        7, 6, 5, 4, 3, 2, 1, 0]
    
    def __init__(self, idx_or_key):
        super().__init__()
        # Support both int index and string key for flexibility
        if isinstance(idx_or_key, int):
            self.idx = idx_or_key
            self.stream_key = None  # Will be set in from_json
        else:
            self.idx = None
            self.stream_key = idx_or_key
        
        self._stream_type: Optional[str] = None
        self.submodules: List[BaseConfigModule] = []
    
    @property
    def physical_index(self) -> int:
        """Get physical index from the submodule's NodeIndex."""
        if self.submodules and hasattr(self.submodules[0], 'physical_index'):
            return self.submodules[0].physical_index
        return 0
    
    @property
    def id(self) -> Optional[NodeIndex]:
        """Get NodeIndex from the submodule."""
        if self.submodules and hasattr(self.submodules[0], 'id'):
            return self.submodules[0].id
        return None
    
    @property
    def stream_type(self) -> Optional[str]:
        """Get stream type (read/write)."""
        return self._stream_type
    
    @stream_type.setter
    def stream_type(self, value: Optional[str]):
        """Set stream type."""
        self._stream_type = value
        
    def set_empty(self):
        """Set to empty configuration (no submodules)."""
        self.submodules = []
        self.mark_empty()
    
    def from_json(self, cfg: dict):
        """
        Load stream configuration from JSON.
        If initialized with index, finds the idx-th stream from stream_engine.
        Determines type from mode field and creates appropriate submodule.
        """
        # Get stream_engine from config
        stream_engine = cfg.get('stream_engine', cfg)
        
        # If initialized with index, find the corresponding stream key
        if self.stream_key is None:
            # Get all stream keys except n2n, sorted
            stream_keys = sorted([k for k in stream_engine.keys() if k != 'n2n'])
            if self.idx < len(stream_keys):
                self.stream_key = stream_keys[self.idx]
            else:
                # No valid stream at this index, create empty config
                self.submodules = []
                self.set_empty()
                return
        
        # Get the specific stream configuration
        if self.stream_key not in stream_engine:
            self.submodules = []
            self.set_empty()
            return
        
        stream_cfg = stream_engine[self.stream_key]
        
        # Determine stream type from 'mode' field in JSON
        mode = stream_cfg.get('mode', 'read')
        self._stream_type = mode
        
        # Create appropriate submodule based on stream type
        if mode == "write":
            submodule = WriteStreamEngineConfig(self.stream_key)
        else:
            submodule = ReadStreamEngineConfig(self.stream_key)
        
        # Load configuration into submodule - pass stream_cfg directly
        submodule.from_json(stream_cfg)
        self.submodules = [submodule]
        
        # Use target field to determine physical assignment (A B C D -> 0 1 2 3)
        # target 0,1,2 (A,B,C) map to READ_STREAM0, READ_STREAM1, READ_STREAM2
        # target 3 (D) maps to WRITE_STREAM0
        target = stream_cfg.get("target", None)
        if target is not None:
            try:
                target_idx = ord(target) - ord('A')
                self.idx = target_idx
                
                # Determine resource based on target
                if target_idx <= 2:
                    # targets A, B, C -> READ_STREAM 0, 1, 2
                    resource = f"READ_STREAM{target_idx}"
                else:
                    # target D (3) -> WRITE_STREAM0
                    resource = "WRITE_STREAM0"
                
                # Directly assign to fixed position based on target
                NodeGraph.get().assign_node(f"STREAM.{self.stream_key}", resource)
            except Exception:
                # If target is not a single character, skip assigning and leave idx as None
                pass
        
    
    def to_bits(self) -> List[Bit]:
        """Return bits from the submodule."""
        if self.submodules:
            return self.submodules[0].to_bits()
        return []
    
    @staticmethod
    def inport_mode_map():
        """Map string inport modes to integers. Also accepts integers directly."""
        return {
            None: 0,
            "buffer": 1,
            "keep": 2,
            "constant": 3,
        }
        
    @staticmethod
    def buffer_mode_map():
        """Map string buffer modes to integers. Also accepts integers directly."""
        return {
            "buffer": 0,
            "keep": 1,
        }



