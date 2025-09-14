from bitstream.config.base import BaseConfigModule

class BufferConfig(BaseConfigModule):
    """Buffer manager config with index (buffer0, buffer1, ...)."""

    FIELD_MAP = [
        # rw: "read" -> 0, "write" -> 1
        ("rw", 1, lambda x: 0 if x == "read" else 1),
        ("dst_port", 2),
        ("enable", 1),
        ("buffer_life_time", 2),
        ("mode", 1),
        # "mask": "00001111" (binary string) -> 0b00001111 (int)
        ("mask", 8, lambda x: int(x, 2)),
    ]

    def __init__(self, idx: int):
        super().__init__()
        self.idx = idx  # buffer index

    def from_json(self, cfg: dict):
        cfg = cfg.get("buffer_config", cfg)
        key = f"buffer{self.idx}"
        if key in cfg and isinstance(cfg[key], dict):
            super().from_json(cfg[key])