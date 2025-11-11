from config.utils.config_parameters import *
# from config.component_config.buffer_manager_cluster import BufferManagerCluster
# from config.component_config import *
from config.utils.excel_generator import multiple_dicts_to_excel
import config.component_config as cc
import importlib
import importlib.util
from config.utils.module_idx import *
import copy
# def import_component(module_name):
#     pkg_name = f"config.component_config.{module_name}"
#     mod = importlib.import_module(pkg_name)
#     return mod


# def split_config(config, init_len):
#     end_len = 0
#     total_len = len(config)
#     result = []
#     if total_len > init_len:
#         end_len = (total_len - init_len) % 63
#         init_config = config[:init_len]
#         # result = 0
#         result.append(config[:init_len] if init_len > 0 else "")
#         iter_times = int((total_len - init_len - end_len) / 63)
#         for i in range(iter_times):
#             result.append(config[init_len+i*63:init_len+i*63+63])
#         if end_len != 0:
#             result.append(config[-end_len:])
#         # new_init_len = 63 - (end_len+1) if end_len != 0 else 63
#         return result
#     elif total_len == init_len:
#         result.append(config)
#         return result
#     else:
#         result.append(config)
#         return result

# def split_config(config):
#     total_len = len(config)
    
#     factor = 0
#     for i in range(1, 30):
#         if (total_len // i) <= 63:
#             factor = i
#             break
    
#     new_len = ( (total_len+factor-1) // factor)  * factor

#     new_config = config + (new_len - total_len) * '0'

#     chunk_len = new_len // factor 

#     chunks = []

#     for i in range(factor):
#         chunks.append(new_config[i * chunk_len : i * chunk_len+chunk_len])

#     return chunks

def print_parameters(component_name, width):
    print(f"`define {component_name} {width}")
    pass

def split_config(config):
    """
    将由'0'和'1'组成的字符串config均分为若干子串，
    每个子串长度 <= 63，且尽量均分（长度为total_len的小于等于63的最大因子）。
    """
    total_len = len(config)
    if total_len == 0:
        return []

    # 找 total_len 的最大因子 ≤ 63
    for i in range(min(63, total_len), 0, -1):
        if total_len % i == 0:
            chunk_size = i
            break

    # if chunk_size == 1:
    #     pass

    # print(chunk_size)

    # 按 chunk_size 切分字符串
    chunks = [config[i:i + chunk_size] for i in range(0, total_len, chunk_size)]
    # chunks = []
    # for i in range(0, total_len, chunk_size):
    #     chunks.append(config[i:i + chunk_size])
    return chunks


def reset_file():
    with open('/cluster/home/zhaohc/NDP_DL/results/bitstream.txt', 'w') as file:
        pass
    with open('/cluster/home/zhaohc/NDP_DL/results/parsed_bitsream.txt', 'w') as file:
        pass

if __name__ == "__main__":
    # ========================
    # reset file
    # ========================
    reset_file()


    all_data = {}


    # ========================
    # config components
    # ========================

    # iga_lc
    sheet1_data = []
    lc_params_0 = {
        "iga_lc_src_id": 1,              # 例如 src id = 2。若多个： [2,1,0]
        "iga_lc_outmost_loop": 0,        # 是否作为最外层循环（或宽度值）
        "iga_lc_initial_value": 0,       # 初始值
        "iga_lc_stride_value": 1,        # stride
        "iga_lc_end_value": 32,          # end value（示例）
        "iga_lc_index": 1,               # last index（PORT_LAST_INDEX 宽度内的值）
    }
    sheet1_data.append(lc_params_0)
    cc.iga_lc.get_config_bits(lc_params_0,6)

    lc_params_10 = {
        "iga_lc_src_id": 1,              # 例如 src id = 2。若多个： [2,1,0]
        "iga_lc_outmost_loop": 0,        # 是否作为最外层循环（或宽度值）
        "iga_lc_initial_value": 0,       # 初始值
        "iga_lc_stride_value": 128,        # stride
        "iga_lc_end_value": 128,          # end value（示例）
        "iga_lc_index": 2,               # last index（PORT_LAST_INDEX 宽度内的值）
    }
    sheet1_data.append(lc_params_10)
    cc.iga_lc.get_config_bits(lc_params_10,7)

    lc_params_1 = {
        "iga_lc_src_id": 0,              # 例如 src id = 2。若多个： [2,1,0]
        "iga_lc_outmost_loop": 1,        # 是否作为最外层循环（或宽度值）
        "iga_lc_initial_value": 0,       # 初始值
        "iga_lc_stride_value": 1,        # stride
        "iga_lc_end_value": 1,          # end value（示例）
        "iga_lc_index": 0,               # last index（PORT_LAST_INDEX 宽度内的值）
    }
    sheet1_data.append(lc_params_1)
    cc.iga_lc.get_config_bits(lc_params_1,5)

    lc_params_2 = {
        "iga_lc_src_id": 3,              # 例如 src id = 2。若多个： [2,1,0]
        "iga_lc_outmost_loop": 0,        # 是否作为最外层循环（或宽度值）
        "iga_lc_initial_value": 0,       # 初始值
        "iga_lc_stride_value": 4,        # stride
        "iga_lc_end_value": 64,          # end value（示例）
        "iga_lc_index": 1,               # last index（PORT_LAST_INDEX 宽度内的值）
    }
    sheet1_data.append(lc_params_2)
    cc.iga_lc.get_config_bits(lc_params_2,3)

    lc_params_3 = {
        "iga_lc_src_id": 2,              # 例如 src id = 2。若多个： [2,1,0]
        "iga_lc_outmost_loop": 0,        # 是否作为最外层循环（或宽度值）
        "iga_lc_initial_value": 0,       # 初始值
        "iga_lc_stride_value": 1,        # stride
        "iga_lc_end_value": 3,          # end value（示例）
        "iga_lc_index": 2,               # last index（PORT_LAST_INDEX 宽度内的值）
    }
    sheet1_data.append(lc_params_3)
    cc.iga_lc.get_config_bits(lc_params_3, 2)

    lc_params_4 = {
        "iga_lc_src_id": 2,              # 例如 src id = 2。若多个： [2,1,0]
        "iga_lc_outmost_loop": 0,        # 是否作为最外层循环（或宽度值）
        "iga_lc_initial_value": 0,       # 初始值
        "iga_lc_stride_value": 1,        # stride
        "iga_lc_end_value": 3,          # end value（示例）
        "iga_lc_index": 3,               # last index（PORT_LAST_INDEX 宽度内的值）
    }
    sheet1_data.append(lc_params_4)
    cc.iga_lc.get_config_bits(lc_params_4, 1)

    lc_params_5 = {
        "iga_lc_src_id": 2,              # 例如 src id = 2。若多个： [2,1,0]
        "iga_lc_outmost_loop": 0,        # 是否作为最外层循环（或宽度值）
        "iga_lc_initial_value": 0,       # 初始值
        "iga_lc_stride_value": 1,        # stride
        "iga_lc_end_value": 1,          # end value（示例）
        "iga_lc_index": 4,               # last index（PORT_LAST_INDEX 宽度内的值）
    }
    sheet1_data.append(lc_params_5)
    cc.iga_lc.get_config_bits(lc_params_5, 0)



    all_data["IGA_LC_Config"] = sheet1_data

    sheet2_data = []
    # iga_pe_params = {
         
    # }
    # params = {
    #     "iga_pe_alu_opcode":,
    #     "iga_pe_src_id":,
    #     "iga_pe_keep_last_index":,
    #     # NULL:0 BUFFER:1 KEEP:2 CONSTANT:3
    #     "iga_pe_inport_mode":,
    #     "iga_pe_cfg_constant_pos":,
    # }
    iga_pe_params_0 = {
        "iga_pe_alu_opcode" : 0,
        # port0 port1 port2
        "iga_pe_src_id" : [0, 1, 0],
        "iga_pe_keep_last_index" : [0, 4, 0],
        # NULL:0 BUFFER:1 KEEP:2 CONSTANT:3
        "iga_pe_inport_mode" : [1, 2, 0],
        "iga_pe_cfg_constant_pos" : [0, 0, 0],
    }
    sheet2_data.append(iga_pe_params_0)
    cc.iga_pe.get_config_bits(iga_pe_params_0, 1)

    iga_pe_params_1 = {
        "iga_pe_alu_opcode" : 2,
        # port0 port1 port2
        "iga_pe_src_id" : [1, 0, 4],
        "iga_pe_keep_last_index" : [3, 0, 0],
        # NULL:0 BUFFER:1 KEEP:2 CONSTANT:3
        "iga_pe_inport_mode" : [2, 3, 1],
        "iga_pe_cfg_constant_pos" : [0, 3, 0],
    }
    sheet2_data.append(iga_pe_params_1)
    cc.iga_pe.get_config_bits(iga_pe_params_1,  2)


    all_data["IGA_PE_Config"] = sheet2_data


    # iga_row
    sheet3_data = []
    row_lc_params_0 = {
        "iga_row_lc_src_id": 2,         # 例如通道号 / src id
        "iga_row_lc_initial_value": 0,  # 初始值
        "iga_row_lc_stride_value": 1,   # 步长
        "iga_row_lc_end_value": 4,     # 结束值
        "iga_row_lc_index":5,          # index
    }
    sheet3_data.append(row_lc_params_0)
    cc.iga_row_lc.get_config_bits(row_lc_params_0, 0)

    row_lc_params_1 = {
        "iga_row_lc_src_id": 0,         # 例如通道号 / src id
        "iga_row_lc_initial_value": 0,  # 初始值
        "iga_row_lc_stride_value": 1,   # 步长
        "iga_row_lc_end_value": 4,     # 结束值
        "iga_row_lc_index":5,          # index
    }
    sheet3_data.append(row_lc_params_1)
    cc.iga_row_lc.get_config_bits(row_lc_params_1, 1)
 
    row_lc_params_2 = {
        "iga_row_lc_src_id": 3,         # 例如通道号 / src id
        "iga_row_lc_initial_value": 0,  # 初始值
        "iga_row_lc_stride_value": 1,   # 步长
        "iga_row_lc_end_value": 4,     # 结束值
        "iga_row_lc_index":3,          # index
    }
    sheet3_data.append(row_lc_params_2)
    cc.iga_row_lc.get_config_bits(row_lc_params_2, 3)


    all_data["IGA_ROW_LC_Config"] = sheet3_data    

    # iga_col
    sheet4_data = []
    col_lc_params_0 = {
        "iga_col_lc_src_id": 6,
        "iga_col_lc_initial_value": 0,
        "iga_col_lc_stride_value": 16,
        "iga_col_lc_end_value": 32,
        "iga_col_lc_index": 6,
    }
    sheet4_data.append(col_lc_params_0)
    cc.iga_col_lc.get_config_bits(col_lc_params_0, 0)

    col_lc_params_1 = {
        "iga_col_lc_src_id": 6,
        "iga_col_lc_initial_value": 0,
        "iga_col_lc_stride_value": 16,
        "iga_col_lc_end_value": 32,
        "iga_col_lc_index": 6,
    }
    sheet4_data.append(col_lc_params_1)
    cc.iga_col_lc.get_config_bits(col_lc_params_1, 1)

    col_lc_params_2 = {
        "iga_col_lc_src_id": 6,
        "iga_col_lc_initial_value": 0,
        "iga_col_lc_stride_value": 16,
        "iga_col_lc_end_value": 32,
        "iga_col_lc_index": 4,
    }
    sheet4_data.append(col_lc_params_2)
    cc.iga_col_lc.get_config_bits(col_lc_params_2, 3)

    all_data["IGA_COL_LC_Config"] = sheet4_data

    # ================================================
    # read_stream
    # ================================================

    # write
    sheet5_data = []    
    wr_mse_params_0 = {
        "mse_enable": 1,
        "mse_mem_idx_mode": [AGMode.BUFFER, AGMode.KEEP, AGMode.CONSTANT],
        # "mem_inport_src_id": [0]*MSE_MEM_AG_INPORT_NUM,
        # "mse_mem_idx_enable": [1]*MSE_MEM_AG_INPORT_NUM,
        # "mse_mem_idx_mode": [0]*MSE_MEM_AG_INPORT_NUM,
        "mse_mem_idx_keep_last_index": [3,2,0],
        "mem_inport_src_id": [3, 2, 0],
        "mse_mem_idx_constant" : [0, 0, 0],
        # "mse_buf_idx_enable": [1]*MSE_BUF_AG_INPORT_NUM,
        # [row, col]
        "mse_buf_idx_mode": [1, 0], 
        "mse_buf_idx_keep_last_index": [4, 0],
        "mse_pingpong_enable": 0,
        "mse_pingpong_last_index": 1,
        "mse_stream_base_addr": 0x0_1004,
        "mse_transaciton_layout_size": [128-1,1-1,1-1],
        "mse_transaciton_layout_size_log": [7,7,0],
        "mse_transaciton_total_size": 128,
        "mse_transaciton_mult": [1, 128, 0],
        "mse_map_matrix_b": [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15][::-1],
        "mse_buf_spatial_stride": [0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15][::-1],
        "mse_buf_spatial_size": 16,
    }
    cc.se_wr_mse.get_config_bits(wr_mse_params_0, 0)
    sheet5_data.append(wr_mse_params_0)
    all_data["SE_WR_MSE_Config"] = sheet5_data

    # activations
    sheet6_data = []
    rd_mse_params_0 = {
        "mse_enable": 1,

        # mem_inport_src_id：假设有 NUM_mem_inport_src_id 个，每个占 WIDTH_mem_inport_src_id_each bit
        # 输入十进制数组（左边元素将放在更高位）
        #port2 port1 port0
        "mse_mem_idx_mode" : [AGMode.KEEP, AGMode.BUFFER, AGMode.KEEP],
         # keep_last_index：有 NUM 个，每个占 WIDTH_xxx_each 位
        "mse_mem_idx_keep_last_index": [2, 5, 3],  # 3个元素，每个3bit
        "mem_inport_src_id": [3, 7, 2],  # 举例：3,1,0
        "mse_mem_idx_constant" : [0, 0, 0],
        "mse_buf_idx_mode": [BAGMode.KEEP, BAGMode.BUFFER],   # [row, col]
        "mse_buf_idx_keep_last_index": [6,0],   # [row, col]

        "mse_pingpong_enable": 1,
        "mse_pingpong_last_index": 4, 

        "mse_stream_base_addr": 0x10_0000,
        # !!!!!!!!!!
        "mse_transaciton_layout_size": [4-1,32-1,1-1],
        "mse_transaciton_layout_size_log": [2, 5+2, 0], 
        "mse_transaciton_total_size": 1*4*32,

        "mse_transaciton_mult": [56, 4, 4*56*56], # [56*4, 4, 4*56*56]
        "mse_map_matrix_b": [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15][::-1],  # 十进制/十六进制都可
        "mse_padding_reg_value": 0,
        "mse_padding_valid": [0,1,1],
        "mse_padding_low_bound": [0,1,1],
        "mse_padding_up_bound": [63, 33, 33],

        "mse_branch_valid": [0,1,1],
        "mse_branch_low_bound": [0,0,0],
        "mse_branch_up_bound": [63,33,33],

        "mse_buf_spatial_stride": [0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15][::-1],
        "mse_buf_spatial_size": 16, # 0 base
    }
    sheet6_data.append(rd_mse_params_0)
    cc.se_rd_mse.get_config_bits(rd_mse_params_0, 1)
    

    # weight
    rd_mse_params_1 = {
        "mse_enable": 1,

        # mem_inport_src_id：假设有 NUM_mem_inport_src_id 个，每个占 WIDTH_mem_inport_src_id_each bit
        # 输入十进制数组（左边元素将放在更高位）
        #port2 port1 port0
        "mse_mem_idx_mode" : [AGMode.KEEP, AGMode.CONSTANT, AGMode.BUFFER],
         # keep_last_index：有 NUM 个，每个占 WIDTH_xxx_each 位
        "mse_mem_idx_keep_last_index": [2, 0, 5],  # 3个元素，每个3bit
        "mem_inport_src_id": [5, 10, 10],  # 举例：3,1,0
        "mse_mem_idx_constant" : [0, 0, 0],
        "mse_buf_idx_mode": [BAGMode.KEEP, BAGMode.BUFFER],        # [row, col]
        "mse_buf_idx_keep_last_index": [6,0],   # [row, col]

        "mse_pingpong_enable": 1,
        "mse_pingpong_last_index": 4, 

        "mse_stream_base_addr": 0x20_0000,
        "mse_transaciton_layout_size": [4-1,32-1,1-1],
        "mse_transaciton_layout_size_log": [2, 7, 0],
        "mse_transaciton_total_size": 1*4*32,
        "mse_transaciton_mult": [64, 4, 64*64], # [56*4, 4, 4*56*56]
        "mse_map_matrix_b": [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15][::-1],  # 十进制/十六进制都可
        "mse_padding_reg_value": 0,
        "mse_padding_valid": [0,0,0],
        "mse_padding_low_bound": [0,0,0],
        "mse_padding_up_bound": [63, 31, 31],

        "mse_branch_valid": [0,0,0],
        "mse_branch_low_bound": [0,0,0],
        "mse_branch_up_bound": [63,31,31],

        "mse_buf_spatial_stride": [0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15][::-1],
        "mse_buf_spatial_size": 16, # 0 base
    }
    sheet6_data.append(rd_mse_params_1)
    cc.se_rd_mse.get_config_bits(rd_mse_params_1, 0)

    all_data["SE_RD_MSE_Config"] = sheet6_data

    # =================================
    # buffer manager cluster
    # =================================
    sheet7_data = []
    bmc_params_0 = {
        "buffer_enable": 1,
        "buf_wr_src_id": 0,
        "buffer_life_time": 3,  # 示例
        "buffer_mode": 0,
        "buffer_mask": 0b1111_1111,  # 假设 BUFFER_BANK_NUM = 4
    }
    sheet7_data.append(bmc_params_0)
    cc.buffer_manager_cluster.get_config_bits(bmc_params_0, 0)

    bmc_params_1 = {
        "buffer_enable": 1,
        "buf_wr_src_id": 0,
        "buffer_life_time": 3,  # 示例
        "buffer_mode": 0,
        "buffer_mask": 0b1111_1111,  # 假设 BUFFER_BANK_NUM = 4
    }
    sheet7_data.append(bmc_params_1)
    cc.buffer_manager_cluster.get_config_bits(bmc_params_1, 1)

    bmc_params_2 = {
        "buffer_enable": 1,
        "buf_wr_src_id": 0,
        "buffer_life_time": 3,  # 示例
        "buffer_mode": 1,
        "buffer_mask": 0b1111_1111,  # 假设 BUFFER_BANK_NUM = 4
    }
    cc.buffer_manager_cluster.get_config_bits(bmc_params_2, 2)

    bmc_params_3 = {
        "buffer_enable": 1,
        "buf_wr_src_id": 0,
        "buffer_life_time": 3,  # 示例
        "buffer_mode": 1,
        "buffer_mask": 0b1111_1111,  # 假设 BUFFER_BANK_NUM = 4
    }
    sheet7_data.append(bmc_params_3)
    cc.buffer_manager_cluster.get_config_bits(bmc_params_3, 3)

    # bmc_params_4 = {
    #     "buffer_enable": 1,
    #     "buf_wr_src_id": 0,
    #     "buffer_life_time": 3,  # 示例
    #     "buffer_mode": 1,
    #     "buffer_mask": 0b1111_1111,  # 假设 BUFFER_BANK_NUM = 4
    # }
    # sheet7_data.append(bmc_params_4)
    # cc.buffer_manager_cluster.get_config_bits(bmc_params_4, 4)

    bmc_params_5 = {
        "buffer_enable": 1,
        "buf_wr_src_id": 0,
        "buffer_life_time": 0,  # 示例
        "buffer_mode": 0,
        "buffer_mask": 0b1111_1111,  # 假设 BUFFER_BANK_NUM = 4
    }
    sheet7_data.append(bmc_params_5)
    cc.buffer_manager_cluster.get_config_bits(bmc_params_5, 5)
    all_data["Buffer_Manager_Cluster_Config"] = sheet7_data

    # =================================
    # pe array
    # =================================

    sheet8_data = []
    spa_params = {
        # port0 port1 port2
        "sa_inport_enable":        [1, 1, 0],        # 按索引 [0..2]
        "sa_inport_pingpong_en":   [1, 1, 0],
        # pe array什么时候切换buffer
        "sa_inport_pingpong_last_index": [5, 5, 0],
        "sa_pe_computation_data_type":  0,            # 示例
        # 什么时候row换完
        # "sa_pe_keep_last_index":        5,          # 示例
        # 什么时候累加完成
        "sa_pe_transout_last_index":    1,          # 示例
        "sa_pe_bias_enable":            0,
        "sa_outport_major":             0,
        "sa_outport_fp32to16":          0,
    }
    cc.special_array.get_config_bits(spa_params, 0)
    sheet8_data.append(spa_params)

    all_data["Special_Array_Config"] = sheet8_data


    # generator Excel
    multiple_dicts_to_excel(all_data, file_name="NDP_DL_Config_Parameters.xlsx")




    # ========================
    # merge all config bits
    # ========================

    PARSE_ORDER = [
        "iga_lc", # 54
        "iga_row_lc", #12
        "iga_col_lc", #21
        "iga_pe",
        "se_rd_mse",
        "se_wr_mse",
        "se_nse",
        "buffer_manager_cluster",
        "special_array",
        "ga_inport_group",
        "ga_outport_group",
        "general_array_pe",
    ]

    full_config = []
    for module_name in PARSE_ORDER:
        pkg_name = f"config.component_config.{module_name}"
        mod = importlib.import_module(pkg_name)
        full_config.extend(mod.config_bits)

    # ========================
    # generate bitstream
    # ========================

    bitstream = []
    # iga lsu sa ga
    # bitstream_entry = "1110"
    # use + update
    config_mask = [1,1,1,0] + [1, 1, 1, 0]
    # config_mask = [1,1,1,0]
    # entry_state = 0
    bitstream_entry = ''.join(str(x) for x in config_mask)
    unconfig_modules = [ModuleID.GENERAL_ARRAY]
    # check_file = full_config.copy()
    check_file = copy.deepcopy(full_config)
    for idx, (mid, config) in enumerate(full_config):
        if not config_mask[ModuleID2Mask[mid]]:
            continue
        if config is None:
            bitstream_entry += "0" * MODULE_CFG_CHUNK_SIZES[mid]      
        else:
            splitted_config = split_config(config)
            temp = []
            for sc in splitted_config:
                bitstream_entry = bitstream_entry + '1' + sc
                temp.append('1 ' + sc)
            check_file[idx][1] = temp

    # extend to 64*n
    # print(len(bitstream_entry))
    extend_len = ( ( len(bitstream_entry) + 63 ) // 64 ) * 64 - len(bitstream_entry)

    extend_bitstream = bitstream_entry + extend_len * '0'
    entry_num =  len(extend_bitstream) // 64
    for i in range(entry_num):
        bitstream.append(extend_bitstream[i*64:i*64+64])


    # ===========================
    # print bitstream
    # ===========================
    with open('/cluster/home/zhaohc/NDP_DL/results/bitstream.txt', 'a') as file:
        for entry in bitstream:
            file.write(entry + '\n')
        # file.write(bitstream[-1])

    with open('/cluster/home/zhaohc/NDP_DL/results/parsed_bitsream.txt', 'a') as file:
        print_mask = [1 for _ in range(len(ModuleID2Mask))]
        for mid, config in check_file:
            if print_mask[mid]:
                # print(f"{PARSE_ORDER[mid]}:\n")
                file.write(f"{PARSE_ORDER[mid]}:\n")
                print_mask[mid] = 0
            if not config_mask[ModuleID2Mask[mid]]:
                continue
            if config is None:
                file.write('0\n'* MODULE_CFG_CHUNK_SIZES[mid])
            else:
                for chunk in config:
                    file.write(chunk + '\n')
