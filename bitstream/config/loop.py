from bitstream.config.base import BaseConfigModule
from bitstream.config.index import NodeIndex

class DramLoopControlConfig(BaseConfigModule):
    """Represents a single DRAM loop configuration."""

    FIELD_MAP = [
        ("src_id", 8, lambda x: NodeIndex(x) if x else None),  # source node ID, will be resolved later
        ("outmost_loop", 1),
        ("start", 16),
        ("end", 16),
        ("stride", 8),
        ("last_index", 8),
    ]
    
    def __init__(self, name: str):
        super().__init__()
        self.id = NodeIndex(name)  # Unique ID for this loop, will be resolved later
        
class DramLoopConfig(BaseConfigModule):
    def from_json(self, cfg):
        cfg = cfg.get("dram_loop_configs", cfg)
        self.submodules = []
        for key, value in cfg.items():
            loop_cfg = DramLoopControlConfig(key)
            loop_cfg.from_json(value)
            self.submodules.append(loop_cfg)
            
    def to_bits(self):
        """Concatenate all loop config bitstreams in fixed order."""
        return sum((loop.to_bits() for loop in self.submodules), [])