def dec_to_bin(val: int, width: int) -> str:
    """十进制整数 -> 固定位宽的二进制字符串（高位在左，零填充）"""
    if val < 0:
        raise ValueError(f"value {val} must be non-negative")
    if val >= (1 << width):
        raise ValueError(f"value {val} exceeds {width} bits capacity")
    return format(val, f"0{width}b")

def flatten_iter(x):
    """把标量 / 一维list / 二维list 全部拍平成一维列表"""
    if isinstance(x, (list, tuple)):
        out = []
        for e in x:
            if isinstance(e, (list, tuple)):
                out.extend(list(e))
            else:
                out.append(e)
        return out
    else:
        return [x]

def pack_field_decimal(values, elem_width, total_elems=None):
    """
    把十进制值（标量或数组）按固定 elem_width 转成二进制并拼接（按给定顺序，前者为高位）。
    values: 标量 / list / list of lists（从左到右依次视为高位->低位）
    elem_width: 每个元素的位宽
    total_elems: 期望元素个数；若给定则会校验数量
    返回：bit字符串
    """
    # print(f"{elem_width*total_elems}")
    elems = flatten_iter(values)
    if total_elems is not None and len(elems) != total_elems:
        raise ValueError(f"expect {total_elems} elements, got {len(elems)}")
    bits = [dec_to_bin(v, elem_width) for v in elems]
    # print(elem_width*total_elems)
    return "".join(bits)  # 高位在前顺序拼接

def pack_scalar_decimal(value, width):
    """标量字段：十进制->固定位宽二进制"""
    return dec_to_bin(value, width)

def concat_bits_high_to_low(bit_fields):
    """把若干bit字符串按给定顺序拼接（列表第一个作为最高位段）"""
    return "".join(bit_fields)

def bits_to_hex(bitstr: str) -> str:
    """bit字符串 -> 十六进制字符串（不带0x）"""
    # 左端对齐到4位
    pad = (4 - (len(bitstr) % 4)) % 4
    if pad:
        bitstr = "0" * pad + bitstr
    n = int(bitstr, 2)
    width_hex = len(bitstr) // 4
    return format(n, f"0{width_hex}X")

def find_factor(config_bits_len):

    # 找 config_bits_len 的最大因子 ≤ 63
    for i in range(min(63, config_bits_len), 0, -1):
        if config_bits_len % i == 0:
            chunk_size = i
            break

    return chunk_size

if __name__ == "__main__":
    print(find_factor(504))