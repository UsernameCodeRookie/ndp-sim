from config.utils.config_parameters import *
from config.utils.bitgen import *
from config.utils.module_idx import *
# config_bits = [[ModuleID.GENERAL_ARRAY, None]] * GA_PE_NUM
config_bits = [[ModuleID.GENERAL_ARRAY, None] for _ in range(GA_PE_NUM)]

config_bits_len = GA_PE_ALU_OPCODE_WIDTH + (GA_PE_SRC_ID_WIDTH + PORT_LAST_INDEX + GA_PE_INPORT_MODE_WIDTH) * 3 + GA_PE_CONSTANT_VALUE_WIDTH * 3 + 5 # 5 is config padding length
config_chunk_size = find_factor(config_bits_len)
config_chunk_cnt = config_bits_len // config_chunk_size
def get_config_bits(params, idx):

    # =========================
    # 示例参数 GA_PE（与 RTL 命名保持一致）
    # =========================
    params = {
        "ga_pe_constant":       [15, 23, 42],      # 对应 constant0/1/2
        "ga_pe_src_id":         [1, 3, 2],         # 对应 src_id0/1/2
        "ga_pe_keep_last_index":[5, 2, 7],         # 对应 keep_last_index0/1/2
        "ga_pe_inport_mode":    [3, 1, 2],         # 对应 inport_mode0/1/2
        "ga_pe_alu_opcode":     12
    }

    params = params 

    # =========================
    # 位宽常量（与 RTL 参数保持一致）
    # =========================
    E_ga_pe_constant       = GA_PE_CONSTANT_VALUE_WIDTH
    E_ga_pe_src_id         = GA_PE_SRC_ID_WIDTH
    E_ga_pe_keep_last_index= PORT_LAST_INDEX
    E_ga_pe_inport_mode    = GA_PE_INPORT_MODE_WIDTH
    E_ga_pe_alu_opcode     = GA_PE_ALU_OPCODE_WIDTH

    # =========================
    # 按 GA_PE_Config 中寄存器高位到低位顺序打包 bit_fields
    # 高位 → ALU opcode → CONSTANT2 → CONSTANT1 → CONSTANT0 → SRC2 → KEEP2 → MODE2 → SRC1 → KEEP1 → MODE1 → SRC0 → KEEP0 → MODE0
    # 注意：pack_field_decimal 最后一个参数 1 用于调试打印
    bit_fields_ga = [
        '0'*5,
        pack_field_decimal(params["ga_pe_alu_opcode"],      E_ga_pe_alu_opcode, 1),

        pack_field_decimal(params["ga_pe_src_id"][2],       E_ga_pe_src_id, 1),
        pack_field_decimal(params["ga_pe_keep_last_index"][2], E_ga_pe_keep_last_index, 1),
        pack_field_decimal(params["ga_pe_inport_mode"][2], E_ga_pe_inport_mode, 1),

        pack_field_decimal(params["ga_pe_src_id"][1],       E_ga_pe_src_id, 1),
        pack_field_decimal(params["ga_pe_keep_last_index"][1], E_ga_pe_keep_last_index, 1),
        pack_field_decimal(params["ga_pe_inport_mode"][1], E_ga_pe_inport_mode, 1),

        pack_field_decimal(params["ga_pe_src_id"][0],       E_ga_pe_src_id, 1),
        pack_field_decimal(params["ga_pe_keep_last_index"][0], E_ga_pe_keep_last_index, 1),
        pack_field_decimal(params["ga_pe_inport_mode"][0], E_ga_pe_inport_mode, 1),

        pack_field_decimal(params["ga_pe_constant"][2], E_ga_pe_constant, 1),
        pack_field_decimal(params["ga_pe_constant"][1], E_ga_pe_constant, 1),
        pack_field_decimal(params["ga_pe_constant"][0], E_ga_pe_constant, 1),
    ]

    # =========================
    # 合并位串
    _config_bits = concat_bits_high_to_low(bit_fields_ga)
    ga_pe_hex  = bits_to_hex(_config_bits)
    ga_pe_int  = int(_config_bits, 2)

    config_bits[idx][1] = _config_bits

    # =========================
    # 输出
    # print("GA_PE Configure Reg (bits):", _config_bits)
    # print("GA_PE Configure Reg (hex) :", ga_pe_hex)
    # print("GA_PE Configure Reg (int) :", ga_pe_int)

# def get_config_bits():
#     return config_bits