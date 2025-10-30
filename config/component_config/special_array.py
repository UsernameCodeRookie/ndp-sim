import sys, os
sys.path.append(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from config.utils.config_parameters import *
from config.utils.bitgen import pack_field_decimal, concat_bits_high_to_low, bits_to_hex, find_factor
from config.utils.module_idx import *

# config_bits = [[ModuleID.SPECIAL_ARRAY, None]] * 1
config_bits = [[ModuleID.SPECIAL_ARRAY, None] for _ in range(1)]


def _compute_total_len():
    pass


config_bits_len = (1 + 1 + PORT_LAST_INDEX)*3 + SA_PE_COMP_DATA_TYPE_WIDTH + SA_PE_TRANSOUT_LAST_INDEX + 1 + 1

# config_chunk_cnt = config_bits_len // config_chunk_size

config_chunk_size = find_factor(config_bits_len)
config_chunk_cnt = config_bits_len // config_chunk_size
def get_config_bits(params, idx):

    # =========================
    # 示例参数
    # =========================
    # params = {
    #     "sa_inport_enable":        [1, 0, 1],        # 按索引 [0..2]
    #     "sa_inport_pingpong_en":   [0, 1, 1],
    #     "sa_inport_pingpong_last_index": [3, 1, 7],
    #     "sa_pe_computation_data_type": 2,            # 示例
    #     "sa_pe_config_last_index":      5,          # 示例
    #     "sa_outport_major":             1,
    #     "sa_outport_fp32to16":          0,
    # }

    params = params

    # =========================
    # 位宽常量
    # =========================
    E_sa_inport_enable       = 1
    E_sa_inport_pingpong_en  = 1
    E_sa_inport_pingpong_last_index = PORT_LAST_INDEX
    E_sa_pe_computation_data_type   = SA_PE_COMP_DATA_TYPE_WIDTH
    E_sa_pe_config_last_index       = SA_PE_TRANSOUT_LAST_INDEX
    E_sa_outport_major              = 1
    E_sa_outport_fp32to16           = 1

    # =========================
    # 按 Verilog 高位到低位顺序打包 bit_fields
    # 注意列表索引高位在前
    bit_fields = [
        pack_field_decimal(params["sa_inport_enable"][2],       E_sa_inport_enable, 1),
        pack_field_decimal(params["sa_inport_pingpong_en"][2],  E_sa_inport_pingpong_en, 1),
        pack_field_decimal(params["sa_inport_pingpong_last_index"][2], E_sa_inport_pingpong_last_index, 1),
        
        pack_field_decimal(params["sa_inport_enable"][1],       E_sa_inport_enable, 1),
        pack_field_decimal(params["sa_inport_pingpong_en"][1],  E_sa_inport_pingpong_en, 1),
        pack_field_decimal(params["sa_inport_pingpong_last_index"][1], E_sa_inport_pingpong_last_index, 1),
        
        pack_field_decimal(params["sa_inport_enable"][0],       E_sa_inport_enable, 1),
        pack_field_decimal(params["sa_inport_pingpong_en"][0],  E_sa_inport_pingpong_en, 1),
        pack_field_decimal(params["sa_inport_pingpong_last_index"][0], E_sa_inport_pingpong_last_index, 1),

        pack_field_decimal(params["sa_pe_computation_data_type"], E_sa_pe_computation_data_type, 1),
        pack_field_decimal(params["sa_pe_config_last_index"],     E_sa_pe_config_last_index, 1),
        pack_field_decimal(params["sa_outport_major"],            E_sa_outport_major, 1),
        pack_field_decimal(params["sa_outport_fp32to16"],         E_sa_outport_fp32to16, 1),
    ]

    # =========================
    # 合并位串
    _config_bits = concat_bits_high_to_low(bit_fields)
    config_hex  = bits_to_hex(_config_bits)
    config_int  = int(_config_bits, 2)

    config_bits[idx][1] = _config_bits

    # =========================
    # 输出
    # print("SA Configure Reg (bits):", _config_bits)
    # print("SA Configure Reg (hex) :", config_hex)
    # print("SA Configure Reg (int) :", config_int)

# def get_config_bits():
#     return config_bits