from enum import IntEnum

# PARSE_ORDER = [
#     "iga_lc", # 54
#     "iga_row_lc", #12
#     "iga_col_lc", #21
#     "iga_pe",
#     "se_rd_mse",
#     "se_wr_mse",
#     "se_nse",
#     "buffer_manager_cluster",
#     "special_array",
#     "ga_inport_group",
#     "ga_outport_group",
#     "general_array",
# ]

class ModuleID(IntEnum):
    IGA_LC = 0
    IGA_ROW_LC = 1
    IGA_COL_LC = 2
    IGA_PE = 3
    SE_RD_MSE = 4
    SE_WR_MSE = 5
    SE_NSE = 6
    BUFFER_MANAGER_CLUSTER = 7
    SPECIAL_ARRAY = 8
    GA_INPORT_GROUP = 9
    GA_OUTPORT_GROUP = 10
    GENERAL_ARRAY = 11


# IGA_LC_CFG_CHUNK_SIZE                              42  1
# IGA_ROW_LC_CFG_CHUNK_SIZE                          12  1
# IGA_COL_LC_CFG_CHUNK_SIZE                          21  1
# IGA_PE_CFG_CHUNK_SIZE                              62  1
# SE_RD_MSE_CFG_CHUNK_SIZE                           63  8
# SE_WR_MSE_CFG_CHUNK_SIZE                           57  6
# SE_NSE_CFG_CHUNK_SIZE                              8  1
# BUFFER_MANAGER_CLUSTER_CFG_CHUNK_SIZE              13  1
# SPECIAL_ARRAY_CFG_CHUNK_SIZE                       22  1
# GA_INPORT_GROUP_CFG_CHUNK_SIZE                     15  1
# GA_OUTPORT_GROUP_CFG_CHUNK_SIZE                    11  1
# GENERAL_ARRAY_CFG_CHUNK_SIZE                       32  4

MODULE_CFG_CHUNK_SIZES = [1] * 12
MODULE_CFG_CHUNK_SIZES[ModuleID.SE_RD_MSE] = 8
MODULE_CFG_CHUNK_SIZES[ModuleID.SE_WR_MSE] = 6
MODULE_CFG_CHUNK_SIZES[ModuleID.GENERAL_ARRAY] = 4

ModuleID2Mask = [0]*12

ModuleID2Mask[ModuleID.IGA_LC] = 0
ModuleID2Mask[ModuleID.IGA_ROW_LC] = 0
ModuleID2Mask[ModuleID.IGA_COL_LC] = 0
ModuleID2Mask[ModuleID.IGA_PE] = 0
ModuleID2Mask[ModuleID.SE_RD_MSE] = 1
ModuleID2Mask[ModuleID.SE_WR_MSE] = 1
ModuleID2Mask[ModuleID.SE_NSE] = 1
ModuleID2Mask[ModuleID.BUFFER_MANAGER_CLUSTER] = 1 
ModuleID2Mask[ModuleID.SPECIAL_ARRAY] = 2
ModuleID2Mask[ModuleID.GA_INPORT_GROUP] = 3

