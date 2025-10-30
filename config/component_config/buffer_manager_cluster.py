import sys, os
sys.path.append(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from config.utils.config_parameters import *
from config.utils.bitgen import pack_field_decimal, concat_bits_high_to_low, bits_to_hex, find_factor
from config.utils.module_idx import *

# config_bits = [[ModuleID.BUFFER_MANAGER_CLUSTER, None]] * BUFFER_NUM
config_bits = [[ModuleID.BUFFER_MANAGER_CLUSTER, None] for _ in range(BUFFER_NUM)]
config_bits_len = BUFFER_LIFE_TIME_WIDTH + BUFFER_BANK_NUM + 2

config_chunk_size = find_factor(config_bits_len)
config_chunk_cnt = config_bits_len // config_chunk_size
def get_config_bits(params, idx):
    # =========================
    # 示例参数
    # =========================

    # params = {
    #     # "buffer_enable": 1,
    #     "buf_wr_src_id": 0,
    #     "buffer_life_time": 3,  # 示例
    #     "buffer_mode": 1,
    #     "buffer_mask": 0b1010,  # 假设 BUFFER_BANK_NUM = 4
    # }
    params = params

    # =========================
    # 位宽常量
    # =========================
    # E_buffer_enable     = 1
    E_buf_wr_src_id     = 1
    E_buffer_life_time  = BUFFER_LIFE_TIME_WIDTH # 3 means 4
    E_buffer_mode       = 1
    E_buffer_mask       = BUFFER_BANK_NUM

    

    # =========================
    # 打包 bit_fields，高位在前
    # bmc_configure_reg = {buffer_enable, buf_wr_src_id, buffer_life_time, buffer_mode, buffer_mask}
    bit_fields = [
        # pack_field_decimal(params["buffer_enable"],    E_buffer_enable,    1),
        pack_field_decimal(params["buf_wr_src_id"],    E_buf_wr_src_id,    1),
        pack_field_decimal(params["buffer_life_time"], E_buffer_life_time, 1),
        pack_field_decimal(params["buffer_mode"],      E_buffer_mode,      1),
        pack_field_decimal(params["buffer_mask"],      E_buffer_mask,      1),
    ]

    # =========================
    # 合并为统一的位串
    # =========================
    _config_bits = concat_bits_high_to_low(bit_fields)
    config_hex  = bits_to_hex(_config_bits)
    config_int  = int(_config_bits, 2)

    config_bits[idx][1] = _config_bits
    # return config_bits


# def get_config_bits():
#     return bmc_bits