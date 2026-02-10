from bitstream.config.base import BaseConfigModule

class NeighborStreamConfig(BaseConfigModule):
    """Neighbor-to-neighbor stream config."""

    FIELD_MAP = [
        ("src_slice_sel", 1),
        ("dst_slice_sel", 1),
        ("ping_pong", 1),
        ("mem_loop", 4, lambda x : x-1 if isinstance(x, int) and x > 0 else x),
    ]
    
    def __init__(self, idx: int):
        super().__init__(idx)
        self.idx = idx
        
    def set_empty(self):
        """Set all fields to None so that to_bits produces zeros."""
        for field_info in self.FIELD_MAP:
            name = field_info[0]
            self.values[name] = None  # None will encode as 0 in to_bits
        self.mark_empty()

    def from_json(self, cfg: dict):
        # cfg = cfg.get("stream_engine", cfg)
        cfg = cfg.get("n2n", {})
        keys = list(cfg.keys())
        
        if self.idx < len(keys):
            n2n = cfg[keys[self.idx]]
            self.from_json(n2n)
        else:
            self.set_empty()