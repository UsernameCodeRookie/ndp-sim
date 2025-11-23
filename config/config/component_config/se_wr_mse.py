#!/usr/bin/env python3
import os
import sys
from config.utils.module_idx import *

# ========================
# 添加项目根路径
# ========================
PROJECT_ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
if PROJECT_ROOT not in sys.path:
    sys.path.insert(0, PROJECT_ROOT)

# ========================
# 导入公共函数与配置参数
# ========================
from config.utils.config_parameters import *
from config.utils.bitgen import (
    pack_field_decimal,
    concat_bits_high_to_low,
    bits_to_hex,
    find_factor
)




E_mse_enable                     = 0
N_mse_enable                     = 1

E_mem_inport_src_id              = MEM_INPORT_SRC_ID_WIDTH
N_mem_inport_src_id              = MSE_MEM_AG_INPORT_NUM

E_mse_mem_idx_enable             = 1
N_mse_mem_idx_enable             = MSE_MEM_AG_INPORT_NUM

E_mse_mem_idx_keep_mode          = 2
N_mse_mem_idx_keep_mode          = MSE_MEM_AG_INPORT_NUM

E_mse_mem_idx_keep_last_index    = PORT_LAST_INDEX
N_mse_mem_idx_keep_last_index    = MSE_MEM_AG_INPORT_NUM

E_mse_buf_idx_enable             = 1
N_mse_buf_idx_enable             = MSE_BUF_AG_INPORT_NUM

E_mse_buf_idx_keep_mode          = 1
N_mse_buf_idx_keep_mode          = MSE_BUF_AG_INPORT_NUM

E_mse_buf_idx_keep_last_index    = PORT_LAST_INDEX
N_mse_buf_idx_keep_last_index    = MSE_BUF_AG_INPORT_NUM

E_mse_pingpong_enable            = 1
N_mse_pingpong_enable            = 1

E_mse_pingpong_last_index        = PORT_LAST_INDEX
N_mse_pingpong_last_index        = 1

E_mse_stream_base_addr           = MSE_STREAM_BASE_ADDR_WIDTH
N_mse_stream_base_addr           = 1

E_mse_transaciton_layout_size    = MSE_TSA_SP_SIZE_WIDTH
N_mse_transaciton_layout_size    = MSE_MEM_AG_INPORT_NUM

E_mse_transaciton_layout_size_log = MSE_TSA_SP_SIZE_LOG_WIDTH
N_mse_transaciton_layout_size_log = MSE_MEM_AG_INPORT_NUM

E_mse_transaciton_total_size     = MSE_TSA_SIZE_WIDTH
N_mse_transaciton_total_size     = 1

E_mse_transaciton_mult           = MSE_TSA_MULT_WIDTH
N_mse_transaciton_mult           = MSE_MEM_AG_INPORT_NUM

# 矩阵：按“行作为一个元素”的打包方式（N=行数，E=每行的位宽）
E_mse_map_matrix_b               = MSE_REMAP_MATRIX_WIDTH
N_mse_map_matrix_b               = MSE_REMAP_MATRIX_HEIGHT

E_mse_buf_spatial_stride         = MSE_BUF_STRIDE_WIDTH
N_mse_buf_spatial_stride         = MSE_BUF_REQ_NUM

E_mse_buf_spatial_size           = MSE_BUF_SIZE_WIDTH
N_mse_buf_spatial_size           = 1

E_mse_mem_idx_constant           = MEM_INPORT_CONSTANT_WIDTH
N_mse_mem_idx_constant           = MSE_MEM_AG_INPORT_NUM

# ===================================================
# 2) 全局：字段规格表（用于总位宽等在 import 时即计算）
#    顺序要与 bit_fields 拼接顺序一致
# ===================================================
FIELD_SPECS = [
    ("config_padding",                3,          1),  # 填充位，凑齐342位
    ("mse_enable",                   E_mse_enable,                   N_mse_enable),
    ("mse_mem_idx_mode",             E_mse_mem_idx_keep_mode,        N_mse_mem_idx_keep_mode),
    ("mse_mem_idx_keep_last_index",  E_mse_mem_idx_keep_last_index,  N_mse_mem_idx_keep_last_index),
    ("mem_inport_src_id",            E_mem_inport_src_id,            N_mem_inport_src_id),
    ("mse_mem_idx_constant",         E_mse_mem_idx_constant,         N_mse_mem_idx_constant),
    ("mse_buf_idx_mode",             E_mse_buf_idx_keep_mode,        N_mse_buf_idx_keep_mode),
    ("mse_buf_idx_keep_last_index",  E_mse_buf_idx_keep_last_index,  N_mse_buf_idx_keep_last_index),
    ("mse_pingpong_enable",          E_mse_pingpong_enable,          N_mse_pingpong_enable),
    ("mse_pingpong_last_index",      E_mse_pingpong_last_index,      N_mse_pingpong_last_index),
    ("mse_stream_base_addr",         E_mse_stream_base_addr,         N_mse_stream_base_addr),
    ("mse_transaciton_layout_size",  E_mse_transaciton_layout_size,  N_mse_transaciton_layout_size),
    ("mse_transaciton_layout_size_log", E_mse_transaciton_layout_size_log, N_mse_transaciton_layout_size_log),
    ("mse_transaciton_total_size",   E_mse_transaciton_total_size,   N_mse_transaciton_total_size),
    ("mse_transaciton_mult",         E_mse_transaciton_mult,         N_mse_transaciton_mult),
    ("mse_map_matrix_b",             E_mse_map_matrix_b,             N_mse_map_matrix_b),
    ("mse_buf_spatial_stride",       E_mse_buf_spatial_stride,       N_mse_buf_spatial_stride),
    ("mse_buf_spatial_size",         E_mse_buf_spatial_size,         N_mse_buf_spatial_size),
]

def _compute_total_len(specs):
    return sum(e * n for _, e, n in specs)

def _compute_field_offsets(specs):
    """返回每个字段在拼接后 [hi:lo] 的bit范围（高位在前）。"""
    total = _compute_total_len(specs)
    offsets = {}
    hi = total - 1
    for name, e, n in specs:
        width = e * n
        lo = hi - width + 1
        offsets[name] = (hi, lo)  # [hi:lo]
        hi = lo - 1
    return offsets

# 导入时即计算
config_bits_len = _compute_total_len(FIELD_SPECS)
# total_bytes = (total_len + 7) // 8
# total_hex_nibbles = (total_len + 3) // 4
# field_bit_ranges = _compute_field_offsets(FIELD_SPECS)  # 可用于调试定位

config_chunk_size = find_factor(config_bits_len)

config_chunk_cnt = config_bits_len // config_chunk_size


# config_bits = [[ModuleID.SE_WR_MSE, None]] * MEMORY_WR_STREAM_ENGINE_NUM
config_bits = [[ModuleID.SE_WR_MSE, None] for _ in range(MEMORY_WR_STREAM_ENGINE_NUM)]

def get_config_bits(params, idx):

    # ========================
    # 位宽定义（对应 RTL 宏）
    # ========================
    E_mse_enable                   = 0
    N_mse_enable                   = 1
    E_mem_inport_src_id             = MEM_INPORT_SRC_ID_WIDTH
    N_mem_inport_src_id             = MSE_MEM_AG_INPORT_NUM
    E_mse_mem_idx_enable            = 1
    N_mse_mem_idx_enable            = MSE_MEM_AG_INPORT_NUM
    E_mse_mem_idx_keep_mode         = 2
    N_mse_mem_idx_keep_mode         = MSE_MEM_AG_INPORT_NUM
    E_mse_mem_idx_keep_last_index   = PORT_LAST_INDEX
    N_mse_mem_idx_keep_last_index   = MSE_MEM_AG_INPORT_NUM
    E_mse_buf_idx_enable            = 1
    N_mse_buf_idx_enable            = MSE_BUF_AG_INPORT_NUM
    E_mse_buf_idx_keep_mode         = 1
    N_mse_buf_idx_keep_mode         = MSE_BUF_AG_INPORT_NUM
    E_mse_buf_idx_keep_last_index   = PORT_LAST_INDEX
    N_mse_buf_idx_keep_last_index   = MSE_BUF_AG_INPORT_NUM
    E_mse_pingpong_enable           = 1
    N_mse_pingpong_enable           = 1
    E_mse_pingpong_last_index       = PORT_LAST_INDEX
    N_mse_pingpong_last_index       = 1
    E_mse_stream_base_addr          = MSE_STREAM_BASE_ADDR_WIDTH
    N_mse_stream_base_addr          = 1
    E_mse_transaciton_layout_size   = MSE_TSA_SP_SIZE_WIDTH
    N_mse_transaciton_layout_size   = MSE_MEM_AG_INPORT_NUM
    E_mse_transaciton_layout_size_log = MSE_TSA_SP_SIZE_LOG_WIDTH
    N_mse_transaciton_layout_size_log = MSE_MEM_AG_INPORT_NUM
    E_mse_transaciton_total_size    = MSE_TSA_SIZE_WIDTH
    N_mse_transaciton_total_size    = 1
    E_mse_transaciton_mult          = MSE_TSA_MULT_WIDTH
    N_mse_transaciton_mult          = MSE_MEM_AG_INPORT_NUM
    # E_mse_map_matrix_b_height       = MSE_REMAP_MATRIX_HEIGHT
    # E_mse_map_matrix_b_width        = MSE_REMAP_MATRIX_WIDTH
    N_mse_map_matrix_b              = 1
    E_mse_buf_spatial_stride        = MSE_BUF_STRIDE_WIDTH
    N_mse_buf_spatial_stride        = MSE_BUF_REQ_NUM
    E_mse_buf_spatial_size          = MSE_BUF_SIZE_WIDTH
    N_mse_buf_spatial_size          = 1
    E_mse_mem_idx_constant          = MEM_INPORT_CONSTANT_WIDTH
    N_mse_mem_idx_constant          = MSE_MEM_AG_INPORT_NUM
    E_mse_map_matrix_b              = MSE_REMAP_MATRIX_WIDTH
    N_mse_map_matrix_b              = MSE_REMAP_MATRIX_HEIGHT
    # ========================
    # 参数示例（可修改）
    # ========================
    # params = {
    #     # "mse_enable": 1,
    #     "mse_mem_idx_mode": [0]*MSE_MEM_AG_INPORT_NUM,
    #     # "mem_inport_src_id": [0]*MSE_MEM_AG_INPORT_NUM,
    #     # "mse_mem_idx_enable": [1]*MSE_MEM_AG_INPORT_NUM,
    #     # "mse_mem_idx_mode": [0]*MSE_MEM_AG_INPORT_NUM,
    #     "mse_mem_idx_keep_last_index": [7]*MSE_MEM_AG_INPORT_NUM,
    #     "mem_inport_src_id": [0]*MSE_MEM_AG_INPORT_NUM,
    #     "mse_mem_idx_constant" : [0]*MSE_MEM_AG_INPORT_NUM,
    #     # "mse_buf_idx_enable": [1]*MSE_BUF_AG_INPORT_NUM,
    #     "mse_buf_idx_mode": [0]*MSE_BUF_AG_INPORT_NUM,
    #     "mse_buf_idx_keep_last_index": [7]*MSE_BUF_AG_INPORT_NUM,
    #     "mse_pingpong_enable": 1,
    #     "mse_pingpong_last_index": 7,
    #     "mse_stream_base_addr": 0x1000,
    #     "mse_transaciton_layout_size": [16]*MSE_MEM_AG_INPORT_NUM,
    #     "mse_transaciton_layout_size_log": [4]*MSE_MEM_AG_INPORT_NUM,
    #     "mse_transaciton_total_size": 64,
    #     "mse_transaciton_mult": [2]*MSE_MEM_AG_INPORT_NUM,
    #     "mse_map_matrix_b": [[0]*E_mse_map_matrix_b_width for _ in range(E_mse_map_matrix_b_height)],
    #     "mse_buf_spatial_stride": [1]*MSE_BUF_REQ_NUM,
    #     "mse_buf_spatial_size": 8,
    # }

    params = params

    # ========================
    # 打包字段函数（处理数组和矩阵）
    # ========================
    bit_fields = []

    bit_fields.append('0'* 3 )
    # bit_fields.append(pack_field_decimal(params["mse_enable"], E_mse_enable, N_mse_enable))
    bit_fields.append(pack_field_decimal(params["mse_mem_idx_mode"], E_mse_mem_idx_keep_mode, N_mse_mem_idx_keep_mode))
    bit_fields.append(pack_field_decimal(params["mse_mem_idx_keep_last_index"], E_mse_mem_idx_keep_last_index, N_mse_mem_idx_keep_last_index))
    bit_fields.append(pack_field_decimal(params["mem_inport_src_id"], E_mem_inport_src_id, N_mem_inport_src_id))
    # bit_fields.append(pack_field_decimal(params["mse_mem_idx_enable"], E_mse_mem_idx_enable, N_mse_mem_idx_enable))
    # bit_fields.append(pack_field_decimal(params["mse_mem_idx_keep_mode"], E_mse_mem_idx_keep_mode, N_mse_mem_idx_keep_mode))
    # bit_fields.append(pack_field_decimal(params["mse_mem_idx_keep_last_index"], E_mse_mem_idx_keep_last_index, N_mse_mem_idx_keep_last_index))
    bit_fields.append(pack_field_decimal(params["mse_mem_idx_constant"], E_mse_mem_idx_constant, N_mse_mem_idx_constant))
    # bit_fields.append(pack_field_decimal(params["mse_buf_idx_enable"], E_mse_buf_idx_enable, N_mse_buf_idx_enable))
    bit_fields.append(pack_field_decimal(params["mse_buf_idx_mode"], E_mse_buf_idx_keep_mode, N_mse_buf_idx_keep_mode))
    bit_fields.append(pack_field_decimal(params["mse_buf_idx_keep_last_index"], E_mse_buf_idx_keep_last_index, N_mse_buf_idx_keep_last_index))
    bit_fields.append(pack_field_decimal(params["mse_pingpong_enable"], E_mse_pingpong_enable, N_mse_pingpong_enable))
    bit_fields.append(pack_field_decimal(params["mse_pingpong_last_index"], E_mse_pingpong_last_index, N_mse_pingpong_last_index))
    bit_fields.append(pack_field_decimal(params["mse_stream_base_addr"], E_mse_stream_base_addr, N_mse_stream_base_addr))
    bit_fields.append(pack_field_decimal(params["mse_transaciton_layout_size"], E_mse_transaciton_layout_size, N_mse_transaciton_layout_size))
    bit_fields.append(pack_field_decimal(params["mse_transaciton_layout_size_log"], E_mse_transaciton_layout_size_log, N_mse_transaciton_layout_size_log))
    bit_fields.append(pack_field_decimal(params["mse_transaciton_total_size"], E_mse_transaciton_total_size, N_mse_transaciton_total_size))
    bit_fields.append(pack_field_decimal(params["mse_transaciton_mult"], E_mse_transaciton_mult, N_mse_transaciton_mult))
    # 对二维矩阵展开行优先
    # flat_matrix = [bit for row in params["mse_map_matrix_b"] for bit in row]
    bit_fields.append(pack_field_decimal(params["mse_map_matrix_b"], E_mse_map_matrix_b, N_mse_map_matrix_b))
    bit_fields.append(pack_field_decimal(params["mse_buf_spatial_stride"], E_mse_buf_spatial_stride, N_mse_buf_spatial_stride))
    bit_fields.append(pack_field_decimal(params["mse_buf_spatial_size"], E_mse_buf_spatial_size, N_mse_buf_spatial_size))

    # ========================
    # 合并 + 转换
    # ========================
    _config_bits = concat_bits_high_to_low(bit_fields)
    config_hex  = bits_to_hex(_config_bits)
    config_int  = int(_config_bits, 2)

    config_bits[idx][1] = _config_bits

    # ========================
    # 打印结果
    # ========================
    # print("=== WR_MSE CONFIG ===")
    # print("Fields (high → low):")
    # for name, bits in zip(params.keys(), bit_fields):
    #     print(f"  {name:28s} = {bits}")

    # print("\nConcatenated bits:")
    # print(_config_bits)
    # print(f"\nHex value : {config_hex}")
    # print(f"Int value : {config_int}")

# def get_config_bits():
#     return config_bits
if __name__ == "__main__":
    params = {
        # "mse_enable": 1,
        "mse_mem_idx_mode": [0]*MSE_MEM_AG_INPORT_NUM,
        # "mem_inport_src_id": [0]*MSE_MEM_AG_INPORT_NUM,
        # "mse_mem_idx_enable": [1]*MSE_MEM_AG_INPORT_NUM,
        # "mse_mem_idx_mode": [0]*MSE_MEM_AG_INPORT_NUM,
        "mse_mem_idx_keep_last_index": [7]*MSE_MEM_AG_INPORT_NUM,
        "mem_inport_src_id": [0]*MSE_MEM_AG_INPORT_NUM,
        "mse_mem_idx_constant" : [0]*MSE_MEM_AG_INPORT_NUM,
        # "mse_buf_idx_enable": [1]*MSE_BUF_AG_INPORT_NUM,
        "mse_buf_idx_mode": [0]*MSE_BUF_AG_INPORT_NUM,
        "mse_buf_idx_keep_last_index": [7]*MSE_BUF_AG_INPORT_NUM,
        "mse_pingpong_enable": 1,
        "mse_pingpong_last_index": 7,
        "mse_stream_base_addr": 0x1000,
        "mse_transaciton_layout_size": [16]*MSE_MEM_AG_INPORT_NUM,
        "mse_transaciton_layout_size_log": [4]*MSE_MEM_AG_INPORT_NUM,
        "mse_transaciton_total_size": 64,
        "mse_transaciton_mult": [2]*MSE_MEM_AG_INPORT_NUM,
        "mse_map_matrix_b": [ 0 for _ in range(N_mse_map_matrix_b)],
        "mse_buf_spatial_stride": [1]*MSE_BUF_REQ_NUM,
        "mse_buf_spatial_size": 8,
    }
    get_config_bits(params,0)