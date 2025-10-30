from config.utils.config_parameters import *
from config.utils.bitgen import *
from config.utils.module_idx import *


config_bits = [[ModuleID.IGA_LC, None] for _ in range(IGA_LC_NUM)]
config_bits_len = IGA_LC_SRC_ID_WIDTH + IGA_LC_OUTMOST_LOOP + IGA_LC_INITIAL_VALUE_WIDTH + \
    IGA_LC_STRIDE_VALUE_WIDTH + IGA_LC_END_VALUE_WIDTH + PORT_LAST_INDEX

config_chunk_size = find_factor(config_bits_len)
config_chunk_cnt = config_bits_len // config_chunk_size
def get_config_bits(params, idx):
    # =========================
    # 宽度定义（对应 RTL 位宽）
    # =========================

    E_iga_lc_src_id = IGA_LC_SRC_ID_WIDTH
    N_iga_lc_src_id = 1
    E_iga_lc_outmost_loop = IGA_LC_OUTMOST_LOOP
    N_iga_lc_outmost_loop = 1
    E_iga_lc_initial_value = IGA_LC_INITIAL_VALUE_WIDTH
    N_iga_lc_initial_value = 1
    E_iga_lc_stride_value = IGA_LC_STRIDE_VALUE_WIDTH
    N_iga_lc_stride_value = 1
    E_iga_lc_end_value = IGA_LC_END_VALUE_WIDTH
    N_iga_lc_end_value = 1
    E_iga_lc_index = PORT_LAST_INDEX
    N_iga_lc_index = 1


    # # 在 params 字典中添加（示例值）
    # params = {
    #     # IGA loop controller: 若只有一组，用单个整数；若多组，用列表（左侧高位）
    #     "iga_lc_src_id": 2,              # 例如 src id = 2。若多个： [2,1,0]
    #     "iga_lc_outmost_loop": 1,        # 是否作为最外层循环（或宽度值）
    #     "iga_lc_initial_value": 0,       # 初始值
    #     "iga_lc_stride_value": 1,        # stride
    #     "iga_lc_end_value": 63,          # end value（示例）
    #     "iga_lc_index": 7,               # last index（PORT_LAST_INDEX 宽度内的值）
    # }

    params = params

    # =========================
    # 在 bit_fields 中按 RTL assign 顺序加入 pack 调用（高位在前）
    # RTL 中顺序（你给的 assign）：
    #   iga_lc_src_id
    #   iga_lc_outmost_loop
    #   iga_lc_initial_value
    #   iga_lc_stride_value
    #   iga_lc_end_value
    #   iga_lc_index
    # =========================

    # 把下面这几行插入到你的 bit_fields 的合适位置（例如在相关模块的最后或你希望的 place）
    bit_fields = [
        # IGA LC — 按 RTL assign 顺序，高位在前
        # pack_field_decimal(value, E_width_const, N_count_const)
        # 这里的 E_iga_* 和 N_iga_* 常量应已在你的环境中定义（与其它 E_/N_ 命名一致）
        pack_field_decimal(params["iga_lc_src_id"],        E_iga_lc_src_id,        N_iga_lc_src_id),
        pack_field_decimal(params["iga_lc_outmost_loop"],  E_iga_lc_outmost_loop,  N_iga_lc_outmost_loop),
        pack_field_decimal(params["iga_lc_initial_value"], E_iga_lc_initial_value, N_iga_lc_initial_value),
        pack_field_decimal(params["iga_lc_stride_value"],  E_iga_lc_stride_value,  N_iga_lc_stride_value),
        pack_field_decimal(params["iga_lc_end_value"],     E_iga_lc_end_value,     N_iga_lc_end_value),
        pack_field_decimal(params["iga_lc_index"],         E_iga_lc_index,         N_iga_lc_index),
    ]

    _config_bits = concat_bits_high_to_low(bit_fields)
    config_hex  = bits_to_hex(_config_bits)
    config_int  = int(_config_bits, 2)

    config_bits[idx][1] = _config_bits
    pass

        # print(_config_bits)
        # print(config_hex)
        # print(config_int)

# def get_config_bits():
#     return config_bits
