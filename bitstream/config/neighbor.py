from bitstream.config.base import BaseConfigModule

class NeighborStreamConfig(BaseConfigModule):
    """Neighbor-to-neighbor stream config."""

    FIELD_MAP = [
        ("src_slice_sel", 1),
        ("dst_slice_sel", 1),
        ("ping_pong", 1),
        ("mem_loop", 4, lambda x : x-1),
    ]

    def from_json(self, cfg: dict):
        cfg = cfg.get("stream_engine", cfg)
        n2n = cfg.get("n2n", {})
        super().from_json(n2n)