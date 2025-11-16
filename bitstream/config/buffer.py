from bitstream.config.base import BaseConfigModule

class BufferConfig(BaseConfigModule):
    """Buffer manager config with index (buffer0, buffer1, ...)."""

    # Based on component_config/buffer_manager_cluster.py:
    # buf_wr_src_id(1) + buffer_life_time(2) + buffer_mode(1) + buffer_mask(8) = 12 bits
    # Note: buffer_enable is NOT part of the config bits, it's determined by whether
    # the config is empty or not (handled by the bitstream generation logic)
    FIELD_MAP = [
        ("dst_port", 1),  # buf_wr_src_id in hardware: 0=SpecArray, 1=GeneArray
        ("buffer_life_time", 2, lambda x : x-1),
        ("mode", 1),
        # "mask": List[int] -> Bit integer
        ("mask", 8, lambda x: int("".join(str(v) for v in x), 2) if isinstance(x, list) else x),
    ]

    def __init__(self, idx: int):
        super().__init__()
        self.idx = idx  # buffer index
        self.enable = 1  # Track enable separately for empty check

    def from_json(self, cfg: dict):
        cfg = cfg.get("buffer_config", cfg)
        key = f"buffer{self.idx}"
        if key in cfg and isinstance(cfg[key], dict):
            buffer_cfg = cfg[key]
            self.enable = buffer_cfg.get("enable", 1)
            super().from_json(buffer_cfg)
    
    def to_bits(self):
        """Override to return empty if disabled."""
        if not self.enable:
            return []  # Empty config
        return super().to_bits()
