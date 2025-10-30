from config.utils.config_parameters import *
from config.utils.bitgen import *
from config.utils.module_idx import *

# config_bits = [[ModuleID.IGA_ROW_LC, None]] * IGA_ROW_LC_NUM
config_bits = [[ModuleID.IGA_ROW_LC, None] for _ in range(IGA_ROW_LC_NUM)]

config_bits_len = IGA_ROW_LC_SRC_ID_WIDTH + IGA_ROW_LC_INITIAL_VALUE_WIDTH + IGA_ROW_LC_STRIDE_VALUE_WIDTH + \
    IGA_ROW_LC_END_VALUE_WIDTH + PORT_LAST_INDEX
config_chunk_size = find_factor(config_bits_len)
config_chunk_cnt = config_bits_len // config_chunk_size
def get_config_bits(params, idx):

    # =========================================================
    # iga_row_lc 配置参数及宽度定义（与 Verilog 对齐）
    # =========================================================

    E_iga_row_lc_src_id        = IGA_ROW_LC_SRC_ID_WIDTH
    N_iga_row_lc_src_id        = 1
    E_iga_row_lc_initial_value = IGA_ROW_LC_INITIAL_VALUE_WIDTH
    N_iga_row_lc_initial_value = 1
    E_iga_row_lc_stride_value  = IGA_ROW_LC_STRIDE_VALUE_WIDTH
    N_iga_row_lc_stride_value  = 1
    E_iga_row_lc_end_value     = IGA_ROW_LC_END_VALUE_WIDTH
    N_iga_row_lc_end_value     = 1
    E_iga_row_lc_index         = PORT_LAST_INDEX
    N_iga_row_lc_index         = 1

    # =========================================================
    # 参数字典（示例，可按实际修改）
    # =========================================================
    # params = {
    #     "iga_row_lc_src_id": 3,         # 例如通道号 / src id
    #     "iga_row_lc_initial_value": 0,  # 初始值
    #     "iga_row_lc_stride_value": 2,   # 步长
    #     "iga_row_lc_end_value": 63,     # 结束值
    #     "iga_row_lc_index": 7,          # index
    # }

    params = params

    # =========================================================
    # 按 RTL assign 顺序拼接（高位在前）
    # 对应 Verilog:
    #   iga_row_lc_src_id
    #   iga_row_lc_initial_value
    #   iga_row_lc_stride_value
    #   iga_row_lc_end_value
    #   iga_row_lc_index
    # =========================================================

    bit_fields = [
        pack_field_decimal(params["iga_row_lc_src_id"],        E_iga_row_lc_src_id,        N_iga_row_lc_src_id),
        pack_field_decimal(params["iga_row_lc_initial_value"], E_iga_row_lc_initial_value, N_iga_row_lc_initial_value),
        pack_field_decimal(params["iga_row_lc_stride_value"],  E_iga_row_lc_stride_value,  N_iga_row_lc_stride_value),
        pack_field_decimal(params["iga_row_lc_end_value"],     E_iga_row_lc_end_value,     N_iga_row_lc_end_value),
        pack_field_decimal(params["iga_row_lc_index"],         E_iga_row_lc_index,         N_iga_row_lc_index),
    ]

    # 拼接成完整 bit 串或整数
    _config_bits = concat_bits_high_to_low(bit_fields)
    config_hex  = bits_to_hex(_config_bits)
    config_int  = int(_config_bits, 2)

    config_bits[idx][1] = _config_bits

    # print(_config_bits)
    # print(config_hex)
    # print(config_int)

# def get_config_bits():
#     return config_bits