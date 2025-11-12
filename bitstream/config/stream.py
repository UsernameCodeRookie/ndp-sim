from bitstream.config.base import BaseConfigModule
from typing import List, Optional
from bitstream.bit import Bit
from bitstream.index import Connect, NodeIndex
from math import log2

class ReadStreamConfig(BaseConfigModule):
    # Field order must match config_generator_ver2.py se_rd_mse bit_fields order
    FIELD_MAP = [
        # Padding (4 bits, not used but needed for alignment)
        ("_padding", 4),  
        # Memory AG fields
        ("idx_mode", 6),                        # mse_mem_idx_keep_mode
        ("idx_keep_last_index", 9),              # mse_mem_idx_keep_last_index
        ("idx", 12),                             # mem_inport_src_id
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

    def from_json(self, cfg: dict):
        cfg = cfg.get("memory_AG", cfg)
        super().from_json(cfg)


class WriteStreamConfig(BaseConfigModule):
    # Field order must match config_generator_ver2.py se_wr_mse bit_fields order
    FIELD_MAP = [
        # Memory AG fields
        ("idx_mode", 6),                         # mse_mem_idx_keep_mode
        ("idx_keep_last_index", 9),               # mse_mem_idx_keep_last_index
        ("idx", 12),                              # mem_inport_src_id
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

    def from_json(self, cfg: dict):
        cfg = cfg.get("memory_AG", cfg)
        super().from_json(cfg)



