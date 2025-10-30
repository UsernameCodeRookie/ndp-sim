
import importlib

if __name__ == "__main__":

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
    names = [f"{m.upper()}_CFG_CHUNK_SIZE" for m in PARSE_ORDER]
    max_len = max(len(n) for n in names)

    for module_name in PARSE_ORDER:
        pkg_name = f"config.component_config.{module_name}"
        mod = importlib.import_module(pkg_name)
        macro_name = f"{module_name.upper()}_CFG_CHUNK_SIZE"
        spaces = " " * 12
        print(f"{macro_name:<{max_len+1}} {spaces}{mod.config_chunk_size}  {mod.config_chunk_cnt}")