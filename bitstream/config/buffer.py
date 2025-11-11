from bitstream.config.base import BaseConfigModule

class BufferConfig(BaseConfigModule):
    """Buffer manager config with index (buffer0, buffer1, ...)."""

    # Based on register_map:
    # enable(1) + dst_port(1) + buffer_life_time(2) + mode(1) + mask(8) = 13 bits
    FIELD_MAP = [
        ("enable", 1),
        ("dst_port", 1),  # buf_wr_src_id in hardware: 0=SpecArray, 1=GeneArray
        ("buffer_life_time", 2),
        ("mode", 1),
        # "mask": List[int] -> Bit integer
        ("mask", 8, lambda x: int("".join(str(v) for v in x), 2) if isinstance(x, list) else x),
    ]

    def __init__(self, idx: int):
        super().__init__()
        self.idx = idx  # buffer index

    def from_json(self, cfg: dict):
        cfg = cfg.get("buffer_config", cfg)
        key = f"buffer{self.idx}"
        if key in cfg and isinstance(cfg[key], dict):
            super().from_json(cfg[key])
