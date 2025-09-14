from bitstream.config.base import BaseConfigModule

class NeighborStreamConfig(BaseConfigModule):
    """Neighbor-to-neighbor stream config."""

    FIELD_MAP = [
        ("mem_loop", 4),
        ("mode", 1),
        ("stream_id", 2),
        ("src_slice_sel", 1),
        ("dst_slice_sel", 1),
        ("src_buf_ping_idx", 3),
        ("src_buf_pong_idx", 3),
        ("dst_buf_ping_idx", 3),
        ("dst_buf_pong_idx", 3),
    ]

    def from_json(self, cfg: dict):
        cfg = cfg.get("stream_engine", cfg)
        n2n = cfg.get("n2n", {})
        super().from_json(n2n)