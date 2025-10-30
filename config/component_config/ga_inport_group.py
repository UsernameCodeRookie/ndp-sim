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



# config_bits = [[ModuleID.GA_INPORT_GROUP, None]] * GA_INPORT_GROUP_NUM
config_bits = [[ModuleID.GA_INPORT_GROUP, None] for _ in range(GA_INPORT_GROUP_NUM)]
config_bits_len = GA_INPORT_NUM + GA_INPORT_SRC_ID_WIDTH + PORT_LAST_INDEX + 1 + 1 + 1
config_chunk_size = find_factor(config_bits_len)
config_chunk_cnt = config_bits_len // config_chunk_size
def get_config_bits(params, idx):
    # ========================
    # 位宽定义（对应 RTL 宏）
    # ========================
    E_ga_inport_enable             = GA_INPORT_NUM
    N_ga_inport_enable             = 1
    E_ga_inport_src_id             = GA_INPORT_SRC_ID_WIDTH
    N_ga_inport_src_id             = 1
    E_ga_inport_pingpong_en        = 1
    N_ga_inport_pingpong_en        = 1
    E_ga_inport_pingpong_last_index= PORT_LAST_INDEX
    N_ga_inport_pingpong_last_index= 1
    E_ga_inport_fp16to32           = 1
    N_ga_inport_fp16to32           = 1
    E_ga_inport_int32tofp          = 1
    N_ga_inport_int32tofp          = 1

    # ========================
    # 参数设置（示例值，可修改）
    # ========================
    # params = {
    #     "ga_inport_mask": 0b1111,        # 根据 GA_INPORT_NUM 实际位数调整
    #     "ga_inport_src_id": 1,
    #     "ga_inport_pingpong_en": 1,
    #     "ga_inport_pingpong_last_index": 7,
    #     "ga_inport_fp16to32": 0,
    #     "ga_inport_int32tofp": 0,
    # }

    params = params

    # ========================
    # 打包字段（高位在前）
    # ========================
    bit_fields = [
        pack_field_decimal(params["ga_inport_mask"],             E_ga_inport_enable,             N_ga_inport_enable),
        pack_field_decimal(params["ga_inport_src_id"],             E_ga_inport_src_id,             N_ga_inport_src_id),
        pack_field_decimal(params["ga_inport_pingpong_en"],        E_ga_inport_pingpong_en,        N_ga_inport_pingpong_en),
        pack_field_decimal(params["ga_inport_pingpong_last_index"],E_ga_inport_pingpong_last_index,N_ga_inport_pingpong_last_index),
        pack_field_decimal(params["ga_inport_fp16to32"],           E_ga_inport_fp16to32,           N_ga_inport_fp16to32),
        pack_field_decimal(params["ga_inport_int32tofp"],          E_ga_inport_int32tofp,          N_ga_inport_int32tofp),
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
    # print("=== GA_INPORT CONFIG ===")
    # print("Fields (high → low):")
    # for name, bits in zip(params.keys(), bit_fields):
    #     print(f"  {name:28s} = {bits}")

    # print("\nConcatenated bits:")
    # print(config_bits)
    # print(f"\nHex value : {config_hex}")
    # print(f"Int value : {config_int}")


# def get_config_bits():
#     return config_bits