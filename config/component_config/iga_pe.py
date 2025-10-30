import sys, os
sys.path.append(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from config.utils.config_parameters import *
from config.utils.bitgen import pack_field_decimal, concat_bits_high_to_low, bits_to_hex, find_factor
from config.utils.module_idx import *

# config_bits = [[ModuleID.IGA_PE, None]] * IGA_PE_NUM
config_bits = [[ModuleID.IGA_PE, None] for _ in range(IGA_PE_NUM)]
config_bits_len = IGA_PE_ALU_OPCODE_WIDTH + (IGA_PE_SRC_ID_WIDTH + PORT_LAST_INDEX + \
    IGA_PE_INPORT_MODE_WIDTH)*3 + IGA_PE_CONSTANT_VALUE_WIDTH*3
config_chunk_size = find_factor(config_bits_len)
config_chunk_cnt = config_bits_len // config_chunk_size
def get_config_bits(params, idx):

    # =========================
    # 示例参数（请根据实际修改）
    # =========================
    # params = {
    #     "iga_pe_alu_opcode": 0b01,  # 示例 ALU 操作码
    #     "iga_pe_src_id": [2, 1, 0],  # 3 个 inport
    #     "iga_pe_keep_last_index": [7, 6, 3],
    #     "iga_pe_inport_mode": [0b10, 0b01, 0b11],  # 3 个 inport mode
    # }

    # params = {
    #     "iga_pe_alu_opcode":,
    #     "iga_pe_src_id":,
    #     "iga_pe_keep_last_index":,
    #     # NULL:0 BUFFER:1 KEEP:2 CONSTANT:3
    #     "iga_pe_inport_mode":,
    #     "iga_pe_cfg_constant_pos":,
    # }

    params = params

    # =========================
    # 位宽常量（来自 NDP_Parameters.svh）
    # =========================
    # E_iga_pe_* 对应每个字段的位宽
    E_iga_pe_alu_opcode       = IGA_PE_ALU_OPCODE_WIDTH
    E_iga_pe_src_id           = IGA_PE_SRC_ID_WIDTH
    E_iga_pe_keep_last_index  = PORT_LAST_INDEX
    E_iga_pe_inport_mode      = IGA_PE_INPORT_MODE_WIDTH
    E_iga_pe_cfg_constant     = IGA_PE_CONSTANT_VALUE_WIDTH

    # =========================
    # 打包寄存器字段
    # 高位在前，对应 Verilog: [ALU_OPCODE | SRC2 | KEEP2 | MODE2 | SRC1 | KEEP1 | MODE1 | SRC0 | KEEP0 | MODE0]
    # =========================
    bit_fields = [
        pack_field_decimal(params["iga_pe_alu_opcode"],        E_iga_pe_alu_opcode,       1),
        pack_field_decimal(params["iga_pe_src_id"][2],         E_iga_pe_src_id,           1),
        pack_field_decimal(params["iga_pe_keep_last_index"][2],E_iga_pe_keep_last_index,1),
        pack_field_decimal(params["iga_pe_inport_mode"][2],    E_iga_pe_inport_mode,      1),

        pack_field_decimal(params["iga_pe_src_id"][1],         E_iga_pe_src_id,           1),
        pack_field_decimal(params["iga_pe_keep_last_index"][1],E_iga_pe_keep_last_index,1),
        pack_field_decimal(params["iga_pe_inport_mode"][1],    E_iga_pe_inport_mode,      1),

        pack_field_decimal(params["iga_pe_src_id"][0],         E_iga_pe_src_id,           1),
        pack_field_decimal(params["iga_pe_keep_last_index"][0],E_iga_pe_keep_last_index,1),
        pack_field_decimal(params["iga_pe_inport_mode"][0],    E_iga_pe_inport_mode,      1),

        pack_field_decimal(params["iga_pe_cfg_constant_pos"][2], E_iga_pe_cfg_constant, 1),
        pack_field_decimal(params["iga_pe_cfg_constant_pos"][1], E_iga_pe_cfg_constant, 1),
        pack_field_decimal(params["iga_pe_cfg_constant_pos"][0], E_iga_pe_cfg_constant, 1),
    ]

    # =========================
    # 合并 bit_fields
    # =========================
    _config_bits = concat_bits_high_to_low(bit_fields)
    config_hex  = bits_to_hex(_config_bits)
    config_int  = int(_config_bits, 2)

    config_bits[idx][1] = _config_bits

    # =========================
    # 输出结果
    # =========================
    # print("IGA PE Configure Reg (bits)  :", _config_bits)
    # print("IGA PE Configure Reg (hex)   :", config_hex)
    # print("IGA PE Configure Reg (int)   :", config_int)

# def get_config_bits():
#     return config_bits