# from config.config_parameters import *
from config.utils.bitgen import *
from config.utils.config_parameters import *
from config.utils.module_idx import *

# config_bits = [[ModuleID.SE_RD_MSE, None]] * MEMORY_RD_STREAM_ENGINE_NUM
config_bits = [[ModuleID.SE_RD_MSE, None] for _ in range(MEMORY_RD_STREAM_ENGINE_NUM)]
# ===================================================
# 全局常量定义（位宽与数量）
# ===================================================
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

E_mse_trans_layout_size          = MSE_TSA_SP_SIZE_WIDTH
N_mse_trans_layout_size          = MSE_MEM_AG_INPORT_NUM

E_mse_trans_layout_size_log      = MSE_TSA_SP_SIZE_LOG_WIDTH
N_mse_trans_layout_size_log      = MSE_MEM_AG_INPORT_NUM

E_mse_trans_total_size           = MSE_TSA_SIZE_WIDTH
N_mse_trans_total_size           = 1

E_mse_trans_mult                 = MSE_TSA_MULT_WIDTH
N_mse_trans_mult                 = MSE_MEM_AG_INPORT_NUM

E_mse_map_matrix_b               = MSE_REMAP_MATRIX_WIDTH
N_mse_map_matrix_b               = MSE_REMAP_MATRIX_HEIGHT

E_mse_padding_reg_value          = MSE_PADDING_VALUE_WIDTH
N_mse_padding_reg_value          = 1

E_mse_padding_valid              = 1
N_mse_padding_valid              = MSE_MEM_AG_INPORT_NUM

E_mse_padding_low_bound          = MSE_PADDING_BOUNDARY_WIDTH
N_mse_padding_low_bound          = MSE_MEM_AG_INPORT_NUM

E_mse_padding_up_bound           = MSE_PADDING_BOUNDARY_WIDTH
N_mse_padding_up_bound           = MSE_MEM_AG_INPORT_NUM

E_mse_branch_valid               = 1
N_mse_branch_valid               = MSE_MEM_AG_INPORT_NUM

E_mse_branch_low_bound           = MSE_BRANCH_BOUNDARY_WIDTH
N_mse_branch_low_bound           = MSE_MEM_AG_INPORT_NUM

E_mse_branch_up_bound            = MSE_BRANCH_BOUNDARY_WIDTH
N_mse_branch_up_bound            = MSE_MEM_AG_INPORT_NUM

E_mse_buf_spatial_stride         = MSE_BUF_STRIDE_WIDTH
N_mse_buf_spatial_stride         = MSE_BUF_REQ_NUM

E_mse_buf_spatial_size           = MSE_BUF_SIZE_WIDTH
N_mse_buf_spatial_size           = 1

E_mse_mem_idx_constant           = MEM_INPORT_CONSTANT_WIDTH
N_mse_mem_idx_constant           = MSE_MEM_AG_INPORT_NUM

def _compute_total_len():
    total = 0
    for e, n in [
        (4,1),
        (E_mse_enable, N_mse_enable),
        (E_mse_mem_idx_keep_mode, N_mse_mem_idx_keep_mode),
        (E_mse_mem_idx_keep_last_index, N_mse_mem_idx_keep_last_index),
        (E_mem_inport_src_id, N_mem_inport_src_id),
        (E_mse_mem_idx_constant, N_mse_mem_idx_constant),
        (E_mse_buf_idx_keep_mode, N_mse_buf_idx_keep_mode),
        (E_mse_buf_idx_keep_last_index, N_mse_buf_idx_keep_last_index),
        (E_mse_pingpong_enable, N_mse_pingpong_enable),
        (E_mse_pingpong_last_index, N_mse_pingpong_last_index),
        (E_mse_stream_base_addr, N_mse_stream_base_addr),
        (E_mse_trans_layout_size, N_mse_trans_layout_size),
        (E_mse_trans_layout_size_log, N_mse_trans_layout_size_log),
        (E_mse_trans_total_size, N_mse_trans_total_size),
        (E_mse_trans_mult, N_mse_trans_mult),
        (E_mse_map_matrix_b, N_mse_map_matrix_b),
        (E_mse_padding_reg_value, N_mse_padding_reg_value),
        (E_mse_padding_valid, N_mse_padding_valid),
        (E_mse_padding_low_bound, N_mse_padding_low_bound),
        (E_mse_padding_up_bound, N_mse_padding_up_bound),
        (E_mse_branch_valid, N_mse_branch_valid),
        (E_mse_branch_low_bound, N_mse_branch_low_bound),
        (E_mse_branch_up_bound, N_mse_branch_up_bound),
        (E_mse_buf_spatial_stride, N_mse_buf_spatial_stride),
        (E_mse_buf_spatial_size, N_mse_buf_spatial_size),
    ]:
        # print(f"e:{e}, n:{n}, e*n:{e*n}")
        total += e * n
    return total

# 计算全局变量
config_bits_len = _compute_total_len()
# print(f"{config_bits_len-4}")
# config_chunk_cnt = config_bits_len // config_chunk_size

config_chunk_size = find_factor(config_bits_len)
config_chunk_cnt = config_bits_len // config_chunk_size
def get_config_bits(params, idx): 

    # E_config_padding = 504 - config_bits_len

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

    # E_mse_buf_idx_enable             = 1
    # N_mse_buf_idx_enable             = MSE_BUF_AG_INPORT_NUM

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

    E_mse_trans_layout_size          = MSE_TSA_SP_SIZE_WIDTH
    N_mse_trans_layout_size          = MSE_MEM_AG_INPORT_NUM

    E_mse_trans_layout_size_log      = MSE_TSA_SP_SIZE_LOG_WIDTH
    N_mse_trans_layout_size_log      = MSE_MEM_AG_INPORT_NUM

    E_mse_trans_total_size           = MSE_TSA_SIZE_WIDTH
    N_mse_trans_total_size           = 1

    E_mse_trans_mult                 = MSE_TSA_MULT_WIDTH
    N_mse_trans_mult                 = MSE_MEM_AG_INPORT_NUM

    E_mse_map_matrix_b               = MSE_REMAP_MATRIX_WIDTH
    N_mse_map_matrix_b               = MSE_REMAP_MATRIX_HEIGHT

    E_mse_padding_reg_value          = MSE_PADDING_VALUE_WIDTH
    N_mse_padding_reg_value          = 1

    E_mse_padding_valid              = 1
    N_mse_padding_valid              = MSE_MEM_AG_INPORT_NUM

    E_mse_padding_low_bound          = MSE_PADDING_BOUNDARY_WIDTH
    N_mse_padding_low_bound          = MSE_MEM_AG_INPORT_NUM

    E_mse_padding_up_bound           = MSE_PADDING_BOUNDARY_WIDTH
    N_mse_padding_up_bound           = MSE_MEM_AG_INPORT_NUM

    E_mse_branch_valid               = 1
    N_mse_branch_valid               = MSE_MEM_AG_INPORT_NUM

    E_mse_branch_low_bound           = MSE_BRANCH_BOUNDARY_WIDTH
    N_mse_branch_low_bound           = MSE_MEM_AG_INPORT_NUM

    E_mse_branch_up_bound            = MSE_BRANCH_BOUNDARY_WIDTH
    N_mse_branch_up_bound            = MSE_MEM_AG_INPORT_NUM

    E_mse_buf_spatial_stride         = MSE_BUF_STRIDE_WIDTH
    N_mse_buf_spatial_stride         = MSE_BUF_REQ_NUM

    E_mse_buf_spatial_size           = MSE_BUF_SIZE_WIDTH
    N_mse_buf_spatial_size           = 1

    E_mse_mem_idx_constant = MEM_INPORT_CONSTANT_WIDTH

    N_mse_mem_idx_constant = MSE_MEM_AG_INPORT_NUM

    # params = {
    #     # "mse_enable": 1,

    #     # mem_inport_src_id：假设有 NUM_mem_inport_src_id 个，每个占 WIDTH_mem_inport_src_id_each bit
    #     # 输入十进制数组（左边元素将放在更高位）
    #     # ??????????????????????
    #     #port2 port1 port0
    #     "mse_mem_idx_mode" : [1, 0, 1],
    #      # keep_last_index：有 NUM 个，每个占 WIDTH_xxx_each 位
    #     "mse_mem_idx_keep_last_index": [1, 3, 2],  # 3个元素，每个3bit
    #     "mem_inport_src_id": [6, 2, 1],  # 举例：3,1,0
    #     # port 2  port 1 port 0
    #     "mse_mem_idx_constant" : [0, 0, 0],
    #     "mse_buf_idx_mode": [1, 0],        # [row, col]
    #     "mse_buf_idx_keep_last_index": [4,0],   # [row, col]

    #     # "mse_mem_idx_enable": [1, 1, 1],     # 也可以直接十进制：5
    #     # "mse_mem_idx_keep_mode": [1, 0, 1], # ?????????????????

    #     # keep_last_index：有 NUM 个，每个占 WIDTH_xxx_each 位

    #     # buf idx
    #     # "mse_buf_idx_enable": [1, 1],           # 总共2bit（两个通道各1bit
    #     # "mse_buf_idx_keep_mode": [1, 0],        # [row, col]
    #     # "mse_buf_idx_keep_last_index": [4,0],   # [row, col]

    #     "mse_pingpong_enable": 1,
    #     "mse_pingpong_last_index": 2, 

    #     "mse_stream_base_addr": 0x10_0000,
    #     "mse_transaciton_layout_size": [4,32,1],

    #     "mse_transaciton_layout_size_log": [2, 5+2, 0],
    #     "mse_transaciton_total_size": 1*4*32,
    #     "mse_transaciton_mult": [4, 2, 1], # [56*4, 4, 4*56*56]

    #     "mse_map_matrix_b": [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15][::-1],  # 十进制/十六进制都可
    #     "mse_padding_reg_value": 0,
    #     "mse_padding_valid": [0,1,1],
    #     "mse_padding_low_bound": [0,1,1],
    #     "mse_padding_up_bound": [63, 56, 56],

    #     "mse_branch_valid": [0,1,1],
    #     "mse_branch_low_bound": [0,0,0],
    #     "mse_branch_up_bound": [63,57,67],

    #     "mse_buf_spatial_stride": [0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15][::-1],
    #     "mse_buf_spatial_size": 16, # 0 base
    # }

    params = params


    # print(pack_field_decimal(params["mse_mem_idx_constant"], E_mse_mem_idx_constant, N_mse_mem_idx_constant))

    bit_fields = [
        # mse_enable
        '0' * 4,
        pack_field_decimal(params["mse_mem_idx_mode"], E_mse_mem_idx_keep_mode, N_mse_mem_idx_keep_mode),
        pack_field_decimal(params["mse_mem_idx_keep_last_index"], E_mse_mem_idx_keep_last_index, N_mse_mem_idx_keep_last_index),
        # mem_inport_src_id

        pack_field_decimal(params["mem_inport_src_id"], E_mem_inport_src_id, N_mem_inport_src_id),
        pack_field_decimal(params["mse_mem_idx_constant"][::-1], E_mse_mem_idx_constant, N_mse_mem_idx_constant),

        pack_field_decimal(params["mse_buf_idx_mode"], E_mse_buf_idx_keep_mode, N_mse_buf_idx_keep_mode),

        pack_field_decimal(params["mse_buf_idx_keep_last_index"], E_mse_buf_idx_keep_last_index, N_mse_buf_idx_keep_last_index),

        # pingpong

        pack_field_decimal(params["mse_pingpong_enable"], E_mse_pingpong_enable, N_mse_pingpong_enable),

        pack_field_decimal(params["mse_pingpong_last_index"], E_mse_pingpong_last_index, N_mse_pingpong_last_index),

        # 地址/尺寸/映射

        pack_field_decimal(params["mse_stream_base_addr"], E_mse_stream_base_addr, N_mse_stream_base_addr),

        pack_field_decimal(params["mse_transaciton_layout_size"], E_mse_trans_layout_size, N_mse_trans_layout_size),

        pack_field_decimal(params["mse_transaciton_layout_size_log"], E_mse_trans_layout_size_log, N_mse_trans_layout_size_log),

        pack_field_decimal(params["mse_transaciton_total_size"], E_mse_trans_total_size, N_mse_trans_total_size),

        pack_field_decimal(params["mse_transaciton_mult"], E_mse_trans_mult, N_mse_trans_mult),

        # 重映射矩阵

        pack_field_decimal(params["mse_map_matrix_b"], E_mse_map_matrix_b, N_mse_map_matrix_b),

        # padding

        pack_field_decimal(params["mse_padding_reg_value"], E_mse_padding_reg_value, N_mse_padding_reg_value),

        pack_field_decimal(params["mse_padding_valid"], E_mse_padding_valid, N_mse_padding_valid),

        pack_field_decimal(params["mse_padding_low_bound"], E_mse_padding_low_bound, N_mse_padding_low_bound),

        pack_field_decimal(params["mse_padding_up_bound"], E_mse_padding_up_bound, N_mse_padding_up_bound),

        # branch

        pack_field_decimal(params["mse_branch_valid"], E_mse_branch_valid, N_mse_branch_valid),

        pack_field_decimal(params["mse_branch_low_bound"], E_mse_branch_low_bound, N_mse_branch_low_bound),

        pack_field_decimal(params["mse_branch_up_bound"], E_mse_branch_up_bound, N_mse_branch_up_bound),

        # buffer 空间信息

        pack_field_decimal(params["mse_buf_spatial_stride"], E_mse_buf_spatial_stride, N_mse_buf_spatial_stride),

        pack_field_decimal(params["mse_buf_spatial_size"], E_mse_buf_spatial_size, N_mse_buf_spatial_size),

        # pack_field_decimal(params["mse_mem_idx_constant"][::-1], E_mse_mem_idx_constant, N_mse_mem_idx_constant),


    ]

    # 拼接为总配置（高位在前）
    _config_bits = concat_bits_high_to_low(bit_fields)
    config_hex  = bits_to_hex(_config_bits)
    config_int  = int(_config_bits, 2)

    config_bits[idx][1] = _config_bits

# if __name__ == "__main__":
#     get_config_bits()