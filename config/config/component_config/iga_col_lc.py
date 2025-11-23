#!/usr/bin/env python3
import os
import sys
from config.utils.module_idx import *
# ========================
# 添加项目根路径，确保可以导入 config.*
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


# config_bits = [[ModuleID.IGA_COL_LC, None]] * IGA_COL_LC_NUM
config_bits = [[ModuleID.IGA_COL_LC, None] for _ in range(IGA_COL_LC_NUM)]
config_bits_len = IGA_COL_LC_SRC_ID_WIDTH + IGA_COL_LC_INITIAL_VALUE_WIDTH + IGA_COL_LC_STRIDE_VALUE_WIDTH + \
    IGA_COL_LC_END_VALUE_WIDTH + PORT_LAST_INDEX

config_chunk_size = find_factor(config_bits_len)
config_chunk_cnt = config_bits_len // config_chunk_size
def get_config_bits(params, idx): 

    # ========================
    # 宽度定义（对应 RTL 位宽）
    # ========================
    E_iga_col_lc_src_id        = IGA_COL_LC_SRC_ID_WIDTH
    N_iga_col_lc_src_id        = 1
    E_iga_col_lc_initial_value = IGA_COL_LC_INITIAL_VALUE_WIDTH
    N_iga_col_lc_initial_value = 1
    E_iga_col_lc_stride_value  = IGA_COL_LC_STRIDE_VALUE_WIDTH
    N_iga_col_lc_stride_value  = 1
    E_iga_col_lc_end_value     = IGA_COL_LC_END_VALUE_WIDTH
    N_iga_col_lc_end_value     = 1
    E_iga_col_lc_index         = PORT_LAST_INDEX
    N_iga_col_lc_index         = 1

    # ========================
    # 参数设置（示例值，可修改）
    # ========================
    # params = {
    #     "iga_col_lc_src_id": 1,
    #     "iga_col_lc_initial_value": 0,
    #     "iga_col_lc_stride_value": 1,
    #     "iga_col_lc_end_value": 63,
    #     "iga_col_lc_index": 7,
    # }

    params = params

    # ========================
    # 打包字段（高位在前）
    # ========================
    bit_fields = [
        pack_field_decimal(params["iga_col_lc_src_id"],        E_iga_col_lc_src_id,        N_iga_col_lc_src_id),
        pack_field_decimal(params["iga_col_lc_initial_value"], E_iga_col_lc_initial_value, N_iga_col_lc_initial_value),
        pack_field_decimal(params["iga_col_lc_stride_value"],  E_iga_col_lc_stride_value,  N_iga_col_lc_stride_value),
        pack_field_decimal(params["iga_col_lc_end_value"],     E_iga_col_lc_end_value,     N_iga_col_lc_end_value),
        pack_field_decimal(params["iga_col_lc_index"],         E_iga_col_lc_index,         N_iga_col_lc_index),
    ]

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
    # print("=== IGA_COL_LC CONFIG ===")
    # print("Fields (high → low):")
    # for name, bits in zip(params.keys(), bit_fields):
    #     print(f"  {name:28s} = {bits}")

    # print("\nConcatenated bits:")
    # print(_config_bits)
    # print(f"\nHex value : {config_hex}")
    # print(f"Int value : {config_int}")

# def get_config_bits():
#     return config_bits