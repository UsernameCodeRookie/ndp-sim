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


# config_bits = [[ModuleID.SE_NSE, None]] * NEIGHBOR_STREAM_ENGINE_NUM
config_bits = [[ModuleID.SE_NSE, None] for _ in range(NEIGHBOR_STREAM_ENGINE_NUM)]
config_bits_len = NSE_SLICE_SEL_WIDTH + NSE_SLICE_SEL_WIDTH + 1 + NSE_CNT_WIDTH
config_chunk_size = find_factor(config_bits_len)
config_chunk_cnt = config_bits_len // config_chunk_size
def get_config_bits(params, idx):
    

    # ========================
    # 添加项目根路径
    # ========================
    # PROJECT_ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
    # if PROJECT_ROOT not in sys.path:
    #     sys.path.insert(0, PROJECT_ROOT)

    # ========================
    # 导入公共函数与配置参数
    # ========================
    # from config.utils.config_parameters import *
    # from config.utils.bitgen import (
    #     pack_field_decimal,
    #     concat_bits_high_to_low,
    #     bits_to_hex
    # )

    # ========================
    # 位宽定义（对应 RTL 宏）
    # ========================
    E_nse_enable            = 0
    N_nse_enable            = 1
    E_nse_in_src_slice_sel  = NSE_SLICE_SEL_WIDTH
    N_nse_in_src_slice_sel  = 1
    E_nse_out_dst_slice_sel = NSE_SLICE_SEL_WIDTH
    N_nse_out_dst_slice_sel = 1
    E_nse_pingpong_enable   = 1
    N_nse_pingpong_enable   = 1
    E_nse_cnt_size          = NSE_CNT_WIDTH
    N_nse_cnt_size          = 1

    # ========================
    # 参数示例（可修改）
    # ========================
    # params = {
    #     # "nse_enable": 1,
    #     "nse_in_src_slice_sel": 0,
    #     "nse_out_dst_slice_sel": 1,
    #     "nse_pingpong_enable": 1,
    #     "nse_cnt_size": 15,
    # }
    params = params

    # ========================
    # 打包字段（高位在前）
    # ========================
    bit_fields = [
        # pack_field_decimal(params["nse_enable"],            E_nse_enable,            N_nse_enable),
        pack_field_decimal(params["nse_in_src_slice_sel"],  E_nse_in_src_slice_sel,  N_nse_in_src_slice_sel),
        pack_field_decimal(params["nse_out_dst_slice_sel"], E_nse_out_dst_slice_sel, N_nse_out_dst_slice_sel),
        pack_field_decimal(params["nse_pingpong_enable"],   E_nse_pingpong_enable,   N_nse_pingpong_enable),
        pack_field_decimal(params["nse_cnt_size"],          E_nse_cnt_size,          N_nse_cnt_size),
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
    # print("=== NSE CONFIG ===")
    # print("Fields (high → low):")
    # for name, bits in zip(params.keys(), bit_fields):
    #     print(f"  {name:28s} = {bits}")

    # print("\nConcatenated bits:")
    # print(_config_bits)
    # print(f"\nHex value : {config_hex}")
    # print(f"Int value : {config_int}")

# def get_config_bits():
#     return config_bits