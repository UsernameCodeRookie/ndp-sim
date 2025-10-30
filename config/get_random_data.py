#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import argparse, os
import numpy as np

DTYPE_BITS = {
    "int8":8,"uint8":8,"int16":16,"uint16":16,
    "int32":32,"uint32":32,"int64":64,"uint64":64,
}

def parse_int_or_expr(expr: str) -> int:
    expr = expr.strip()
    try:
        if expr.startswith(("0x","0X")): return int(expr,16)
        if expr.startswith(("0b","0B")): return int(expr,2)
        # 简单安全表达式
        if not all(c.isdigit() or c in "*/+-()" for c in expr.replace(" ","")):
            raise ValueError
        return int(eval(expr, {"__builtins__": None}, {}))
    except Exception:
        raise argparse.ArgumentTypeError(f"Invalid numeric expression: {expr}")

def tokens_to_bytes(tokens, elem_bytes, endian):
    """把十六进制字符串列表按 elem_bytes 和字节序还原为字节流"""
    out = bytearray(len(tokens) * elem_bytes)
    w = 0
    for t in tokens:
        v = int(t, 16)
        out[w:w+elem_bytes] = v.to_bytes(elem_bytes, byteorder=endian, signed=False)
        w += elem_bytes
    return np.frombuffer(out, dtype=np.uint8)

def main():
    ap = argparse.ArgumentParser(description="随机HEX数据生成器（固定总行数、未覆盖补零）")
    ap.add_argument("--dtype", required=True, choices=DTYPE_BITS.keys())
    ap.add_argument("--base-addr", type=parse_int_or_expr, required=True)
    ap.add_argument("--length", type=parse_int_or_expr, required=True)
    ap.add_argument("--output", required=True)
    ap.add_argument("--seed", type=int, default=2025)
    ap.add_argument("--per-line", type=int, default=2048, help="每行元素数") # 4 * 64 * 16
    ap.add_argument("--total-lines", type=int, default=6000, help="总行数（固定输出这么多行）")
    ap.add_argument("--endian", choices=["little","big"], default="little")
    args = ap.parse_args()

    bits = DTYPE_BITS[args.dtype]
    elem_bytes = bits // 8
    width_hex  = bits // 4

    bytes_per_line = args.per_line * elem_bytes
    total_bytes    = args.total_lines * bytes_per_line   # 固定 6000 行

    # 1) 初始化目标大缓冲（全 0）
    data = np.zeros(total_bytes, dtype=np.uint8)

    # 2) 如果已有文件，按 dtype/endian 正确读入并覆盖到前部
    if os.path.exists(args.output):
        with open(args.output, "r", encoding="utf-8") as f:
            tokens = f.read().split()
        # 旧文件的 dtype/endian 需一致；若不一致，需另存或重建
        old_bytes = tokens_to_bytes(tokens, elem_bytes, args.endian)
        copy_n = min(len(old_bytes), total_bytes)
        data[:copy_n] = old_bytes[:copy_n]

    # 3) 在指定地址范围内写入随机数据
    rng = np.random.default_rng(args.seed)
    end_addr = args.base_addr + args.length
    if end_addr > total_bytes:
        raise ValueError(
            f"随机区域超出目标文件容量: end=0x{end_addr:X} > total_bytes=0x{total_bytes:X} "
            f"(total_lines={args.total_lines}, per_line={args.per_line}, elem_bytes={elem_bytes})"
        )
    rand_bytes = rng.integers(0, 256, size=args.length, dtype=np.uint8)
    data[args.base_addr:end_addr] = rand_bytes

    # 4) 写出为文本（固定 total_lines，每行 per_line 个元素）
    with open(args.output, "w", encoding="utf-8") as f:
        idx = 0
        for _ in range(args.total_lines):
            line_chunk = data[idx: idx + bytes_per_line]
            idx += bytes_per_line
            elems = line_chunk.reshape(args.per_line, elem_bytes)
            vals  = [int.from_bytes(e.tobytes(), byteorder=args.endian, signed=False) for e in elems]
            line  = " ".join(f"{v:0{width_hex}X}" for v in vals)
            f.write(line + "\n")

    print(f"[OK] 写出: {args.output}")
    print(f"     dtype={args.dtype} bits={bits} endian={args.endian}")
    print(f"     总行数={args.total_lines}, 每行元素={args.per_line}, 总字节={total_bytes}")
    print(f"     随机覆盖: 0x{args.base_addr:X} ~ 0x{end_addr-1:X}")

if __name__ == "__main__":
    main()