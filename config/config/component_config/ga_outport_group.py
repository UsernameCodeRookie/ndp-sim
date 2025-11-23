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

# config_bits = [[ModuleID.GA_OUTPORT_GROUP, None]] * GA_OUTPORT_GROUP_NUM
config_bits = [[ModuleID.GA_OUTPORT_GROUP, None] for _ in range(GA_OUTPORT_GROUP_NUM)]
config_bits_len = GA_OUTPORT_NUM + GA_OUTPORT_SRC_ID_WIDTH + 1 + 1
config_chunk_size = find_factor(config_bits_len)
config_chunk_cnt = config_bits_len // config_chunk_size
def get_config_bits(params, idx):

    # ========================
    # 位宽定义（对应 RTL 宏）
    # ========================
    E_ga_outport_enable   = GA_OUTPORT_NUM
    N_ga_outport_enable   = 1
    E_ga_outport_src_id   = GA_OUTPORT_SRC_ID_WIDTH
    N_ga_outport_src_id   = 1
    E_ga_outport_fp32to16 = 1
    N_ga_outport_fp32to16 = 1
    E_ga_outport_int32to8 = 1
    N_ga_outport_int32to8 = 1

    # ========================
    # 参数设置（示例值，可修改）
    # ========================
    # params = {
    #     "ga_outport_mask": 0b1111,    # 根据 GA_OUTPORT_NUM 调整二进制位宽
    #     "ga_outport_src_id": 1,
    #     "ga_outport_fp32to16": 0,
    #     "ga_outport_int32to8": 1,
    # }

    params = params

    # ========================
    # 打包字段（高位在前）
    # ========================
    bit_fields = [
        pack_field_decimal(params["ga_outport_mask"],   E_ga_outport_enable,   N_ga_outport_enable),
        pack_field_decimal(params["ga_outport_src_id"],   E_ga_outport_src_id,   N_ga_outport_src_id),
        pack_field_decimal(params["ga_outport_fp32to16"], E_ga_outport_fp32to16, N_ga_outport_fp32to16),
        pack_field_decimal(params["ga_outport_int32to8"], E_ga_outport_int32to8, N_ga_outport_int32to8),
    ]

    # ========================
    # 合并 + 转换
    # ========================
    _config_bits = concat_bits_high_to_low(bit_fields)
    _config_hex  = bits_to_hex(_config_bits)
    _config_int  = int(_config_bits, 2)

    # global config_bits
    config_bits[idx][1] = config_bits

    # ========================
    # 打印结果
    # ========================
    # print("=== GA_OUTPORT CONFIG ===")
    # print("Fields (high → low):")
    # for name, bits in zip(params.keys(), bit_fields):
    #     print(f"  {name:28s} = {bits}")

    # print("\nConcatenated bits:")
    # print(_config_bits)
    # print(f"\nHex value : {_config_hex}")
    # print(f"Int value : {_config_int}")

# def get_config_bits():
#     return config_bits
if __name__ == "__main__":
    # 测试代码
    test_params = {
        "ga_outport_enable": 0b1111,    # 根据 GA_OUTPORT_NUM 调整二进制位宽
        "ga_outport_src_id": 1,
        "ga_outport_fp32to16": 0,
        "ga_outport_int32to8": 1,
    }
    get_config_bits(test_params, 0)