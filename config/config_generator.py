#!/usr/bin/env python3
# -*- coding: utf-8 -*-
import os
import sys
import importlib
import importlib.util
import re
from datetime import datetime

# 项目根路径（本脚本位于 config/ 下）
PROJECT_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
if PROJECT_ROOT not in sys.path:
    sys.path.insert(0, PROJECT_ROOT)

COMPONENT_DIR = os.path.join(PROJECT_ROOT, "config", "component_config")
if not os.path.isdir(COMPONENT_DIR):
    raise FileNotFoundError(f"component_config 目录不存在: {COMPONENT_DIR}")

# 从工具库导入 bit 拼接/转换函数（请确保该模块存在）
try:
    from config.utils.bitgen import concat_bits_high_to_low, bits_to_hex
except Exception as e:
    raise ImportError("无法导入 config.utils.bitgen，请确认该模块存在且路径正确。") from e

import config.component_config as cc

def hardware_config():
    pass




# 按照 localparam 顺序（文件名不带 .py）
PARSE_ORDER = [
    "iga_lc",
    "iga_row_lc",
    "iga_col_lc",
    "iga_pe",
    "se_rd_mse",
    "se_wr_mse",
    "se_nse",
    "bmc",
    "sa",
    "ga_inport_group",
    "ga_outport_group",
    "ga_pe",
]

BIN_RE = re.compile(r"^[01]+$")

def import_component(module_name):
    """尝试以 package 或 文件路径导入组件模块，返回 (module_obj, path)"""
    pkg_name = f"config.component_config.{module_name}"
    try:
        mod = importlib.import_module(pkg_name)
        return mod, getattr(mod, "__file__", None)
    except Exception:
        file_path = os.path.join(COMPONENT_DIR, f"{module_name}.py")
        if os.path.isfile(file_path):
            try:
                spec = importlib.util.spec_from_file_location(f"component_config.{module_name}", file_path)
                mod = importlib.util.module_from_spec(spec)
                spec.loader.exec_module(mod)
                return mod, file_path
            except Exception:
                return None, file_path
        else:
            return None, file_path

def merge_config():
    full_config = []

    for module_name in PARSE_ORDER:
        print(f"[+] 处理 {parse_label} -> {module_name} ...")
        mod, path = import_component(module_name)

        getter = getattr(mod, "get_config_bits", None)

        bits = getter()

        bits = bits.strip()

        full_config.append(bits)


    # 按 PARSE_ORDER 的顺序拼接（extracted 已按遍历顺序）
    full_config = concat_bits_high_to_low(full_config)

def get_bitstream(full_config):
    ENTRY_LENGTH = 64




if __name__ == "__main__":
    main()