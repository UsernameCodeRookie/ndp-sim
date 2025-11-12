from bitstream.config.base import BaseConfigModule
from typing import List, Optional
from bitstream.bit import Bit
from bitstream.index import Connect, NodeIndex
from math import log2

class ReadStreamConfig(BaseConfigModule):
    FIELD_MAP = [
        ("idx_mode", 6),
        ("idx_keep_last_index", 9),
        ("idx", 12),
        ("idx_constant", 24),
        ("buf_idx_mode", 2),
        ("buf_idx_keep_last_index", 6),
        ("pingpong", 1),
        ("pingpong_last_index", 3),
        ("base_addr", 29),
        ("idx_size", 24),
        ("idx_size_log", 9),
        ("total_size", 8),
        ("dim_stride", 60),
        ("address_remapping", 64),
        ("padding_reg_value", 8),
        ("padding_enable", 3),
        ("idx_padding_range_low_bound", 36),
        ("idx_padding_range_up_bound", 36),
        ("tailing_enable", 3),
        ("idx_tailing_range_low", 36),
        ("idx_tailing_range_up", 36),
        ("spatial_stride", 80),
        ("spatial_size", 5),
    ]

    def from_json(self, cfg: dict):
        cfg = cfg.get("memory_AG", cfg)
        super().from_json(cfg)


class WriteStreamConfig(BaseConfigModule):
    FIELD_MAP = [
        ("idx_mode", 6),
        ("idx_keep_last_index", 9),
        ("idx", 12),
        ("mse_mem_idx_constant", 24),
        ("buf_idx_mode", 2),
        ("buf_idx_keep_last_index", 6),
        ("ping_pong", 1),
        ("pingpong_last_index", 3),
        ("base_addr", 29),
        ("idx_size", 24),
        ("idx_size_log", 9),
        ("total_size", 8),
        ("dim_stride", 60),
        ("address_remapping", 64),
        ("spatial_stride", 80),
        ("spatial_size", 5),
    ]

    def from_json(self, cfg: dict):
        cfg = cfg.get("memory_AG", cfg)
        super().from_json(cfg)



