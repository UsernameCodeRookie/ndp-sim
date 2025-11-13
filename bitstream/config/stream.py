from bitstream.config.base import BaseConfigModule
from typing import List, Optional
from bitstream.bit import Bit
from bitstream.index import Connect, NodeIndex
from math import log2


class ReadStreamEngineConfig(BaseConfigModule):
    """
    Read stream engine configuration with padding and tailing fields.
    This is a submodule of StreamConfig.
    """
    
    FIELD_MAP = [
        # Padding (4 bits, not used but needed for alignment)
        ("_padding", 4),  
        # Memory AG fields
        ("idx_mode", 6),                        # mse_mem_idx_keep_mode
        ("idx_keep_last_index", 9),              # mse_mem_idx_keep_last_index
        ("idx", 12, lambda self, x: ReadStreamEngineConfig._encode_idx(self, x)),  # mem_inport_src_id
        ("idx_constant", 24),                    # mse_mem_idx_constant
        # Buffer AG fields
        ("buf_idx_mode", 2),                     # mse_buf_idx_keep_mode
        ("buf_idx_keep_last_index", 6),          # mse_buf_idx_keep_last_index
        # Stream fields
        ("pingpong", 1),                         # mse_pingpong_enable
        ("pingpong_last_index", 3),              # mse_pingpong_last_index
        # Address and size fields
        ("base_addr", 29),                       # mse_stream_base_addr
        ("idx_size", 24),                        # mse_transaciton_layout_size
        ("idx_size_log", 9),                     # mse_transaciton_layout_size_log
        ("total_size", 8),                       # mse_transaciton_total_size
        ("dim_stride", 60),                      # mse_transaciton_mult
        # Remapping
        ("address_remapping", 64),               # mse_map_matrix_b
        # Padding fields
        ("padding_reg_value", 8),                # mse_padding_reg_value
        ("padding_enable", 3),                   # mse_padding_valid
        ("idx_padding_range_low_bound", 36),     # mse_padding_low_bound
        ("idx_padding_range_up_bound", 36),      # mse_padding_up_bound
        # Tailing (branch) fields
        ("tailing_enable", 3),                   # mse_branch_valid
        ("idx_tailing_range_low", 36),           # mse_branch_low_bound
        ("idx_tailing_range_up", 36),            # mse_branch_up_bound
        # Spatial fields
        ("spatial_stride", 80),                  # mse_buf_spatial_stride
        ("spatial_size", 5),                     # mse_buf_spatial_size
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
    
    @staticmethod
    def _encode_idx(self, x):
        """Encode idx field, converting string node names to Connect objects."""
        if isinstance(x, list):
            result = []
            for item in x:
                if isinstance(item, str):
                    result.append(Connect(item, self.id))
                elif item is None:
                    result.append(0)
                else:
                    result.append(item)
            return result
        return x
    
    def from_json(self, cfg: dict):
        """Load read stream configuration from JSON."""
        if not cfg.get("memory_AG", {}):
            self.set_empty()
            return
        
        self.id = NodeIndex(f"STREAM.{self.stream_key}", stream_type="read")
        cfg = cfg.get("memory_AG", cfg)
        super().from_json(cfg)

    def set_empty(self):
        """Set to empty configuration."""
        self.mark_empty()

class WriteStreamEngineConfig(BaseConfigModule):
    """
    Write stream engine configuration without padding and tailing fields.
    This is a submodule of StreamConfig.
    """
    
    FIELD_MAP = [
        # Memory AG fields
        ("idx_mode", 6),                         # mse_mem_idx_keep_mode
        ("idx_keep_last_index", 9),               # mse_mem_idx_keep_last_index
        ("idx", 12, lambda self, x: WriteStreamEngineConfig._encode_idx(self, x)),  # mem_inport_src_id
        ("mse_mem_idx_constant", 24),             # mse_mem_idx_constant
        # Buffer AG fields
        ("buf_idx_mode", 2),                      # mse_buf_idx_keep_mode
        ("buf_idx_keep_last_index", 6),           # mse_buf_idx_keep_last_index
        # Stream fields
        ("ping_pong", 1),                         # mse_pingpong_enable
        ("pingpong_last_index", 3),               # mse_pingpong_last_index
        # Address and size fields
        ("base_addr", 29),                        # mse_stream_base_addr
        ("idx_size", 24),                         # mse_transaciton_layout_size
        ("idx_size_log", 9),                      # mse_transaciton_layout_size_log
        ("total_size", 8),                        # mse_transaciton_total_size
        ("dim_stride", 60),                       # mse_transaciton_mult
        # Remapping
        ("address_remapping", 64),                # mse_map_matrix_b
        # Spatial fields
        ("spatial_stride", 80),                   # mse_buf_spatial_stride
        ("spatial_size", 5),                      # mse_buf_spatial_size
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
    
    @staticmethod
    def _encode_idx(self, x):
        """Encode idx field, converting string node names to Connect objects."""
        if isinstance(x, list):
            result = []
            for item in x:
                if isinstance(item, str):
                    result.append(Connect(item, self.id))
                elif item is None:
                    result.append(0)
                else:
                    result.append(item)
            return result
        return x
    
    def from_json(self, cfg: dict):
        """Load write stream configuration from JSON."""
        self.id = NodeIndex(f"STREAM.{self.stream_key}", stream_type="write")
        cfg = cfg.get("memory_AG", cfg)
        super().from_json(cfg)


class StreamConfig(BaseConfigModule):
    """
    Container for stream configurations.
    Similar to BufferLoopControlGroupConfig, this holds submodules.
    Determines read/write type from JSON and creates appropriate submodule.
    
    Can be initialized with either an index (int) or stream_key (str).
    """
    
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
        Determines type from memory_AG.mode and creates appropriate submodule.
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
        
        # Extract stream type from memory_AG.mode
        memory_ag = stream_cfg.get("memory_AG", {})
        
        if memory_ag is {}:
            self.set_empty()
        
        self.stream_type = memory_ag.get("mode", "read")
        
        # Create appropriate submodule based on stream type
        if self.stream_type == "write":
            submodule = WriteStreamEngineConfig(self.stream_key)
        else:
            submodule = ReadStreamEngineConfig(self.stream_key)
        
        # Load configuration into submodule
        submodule.from_json(stream_cfg)
        self.submodules = [submodule]
        
        # Add group connection
        group = stream_cfg.get('buffer_lc_group', None)
        if group is not None:
            Connect(f"{group}", submodule.id)
    
    def to_bits(self) -> List[Bit]:
        """Return bits from the submodule."""
        if self.submodules:
            return self.submodules[0].to_bits()
        return []



