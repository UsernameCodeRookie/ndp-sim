#!/usr/bin/env python3
"""Debug script to count bits for each module."""

import json
from bitstream.config.mapper import NodeGraph
from bitstream.config import (
    DramLoopControlConfig,
    BufferLoopControlGroupConfig,
    LCPEConfig,
    NeighborStreamConfig,
    BufferConfig,
    SpecialArrayConfig,
)
from bitstream.index import NodeIndex
from config.component_config import se_rd_mse, se_wr_mse, se_nse

# Load config
with open('./data/gemm_config_reference_aligned.json') as f:
    cfg = json.load(f)

# Initialize modules
dram_modules = [DramLoopControlConfig(i) for i in range(8)]
for m in dram_modules:
    m.from_json(cfg)

NodeGraph.get().allocate_resources()
NodeGraph.get().search_mapping()
NodeIndex.resolve_all()

buffer_groups = [BufferLoopControlGroupConfig(i) for i in range(4)]
for m in buffer_groups:
    m.from_json(cfg)

pe_modules = [LCPEConfig(i) for i in range(8)]
neighbor_module = NeighborStreamConfig()
buffer_configs = [BufferConfig(i) for i in range(6)]
special_module = SpecialArrayConfig()

for m in pe_modules + [neighbor_module] + buffer_configs + [special_module]:
    m.from_json(cfg)

NodeIndex.resolve_all()

# Initialize stream engines (simplified, without full JSON conversion)
se_rd_mse.config_bits = [[4, None] for _ in range(3)]
se_wr_mse.config_bits = [[5, None] for _ in range(1)]
se_nse.config_bits = [[6, None] for _ in range(2)]

def bitstring(bits):
    return ''.join(format(bit.value, f'0{bit.width}b') for bit in bits)

# Count bits for each module type
print('Module bit counts:')
total = 8  # CONFIG mask
print(f'CONFIG: 8 bits, cumulative: {total}')

# DRAM LC
for i, m in enumerate(dram_modules):
    bits = bitstring(m.to_bits())
    total += 1 + len(bits)  # 1 enable bit + data
    print(f'IGA_LC[{i}]: {len(bits)} bits (+1 enable), cumulative: {total}')

# Buffer groups - ROW_LC
for i, group in enumerate(buffer_groups):
    row, col = group.submodules
    bits = bitstring(row.to_bits())
    total += 1 + len(bits)
    print(f'IGA_ROW_LC[{i}]: {len(bits)} bits (+1 enable), cumulative: {total}')

# Buffer groups - COL_LC  
for i, group in enumerate(buffer_groups):
    row, col = group.submodules
    bits = bitstring(col.to_bits())
    total += 1 + len(bits)
    print(f'IGA_COL_LC[{i}]: {len(bits)} bits (+1 enable), cumulative: {total}')

# PE configs
for i, m in enumerate(pe_modules):
    bits = bitstring(m.to_bits())
    total += 1 + len(bits)
    print(f'IGA_PE[{i}]: {len(bits)} bits (+1 enable), cumulative: {total}')

print(f'\nBefore stream engines: {total} bits')

# Stream engines (just count what's in config_bits)
for i, (mid, config_bits) in enumerate(se_rd_mse.config_bits):
    if config_bits:
        total += 1 + len(config_bits)
        print(f'SE_RD_MSE[{i}]: {len(config_bits)} bits (+1 enable), cumulative: {total}')
    else:
        total += 1  # Just the enable bit (0)
        print(f'SE_RD_MSE[{i}]: 0 bits (+1 enable=0), cumulative: {total}')

for i, (mid, config_bits) in enumerate(se_wr_mse.config_bits):
    if config_bits:
        total += 1 + len(config_bits)
        print(f'SE_WR_MSE[{i}]: {len(config_bits)} bits (+1 enable), cumulative: {total}')
    else:
        total += 1
        print(f'SE_WR_MSE[{i}]: 0 bits (+1 enable=0), cumulative: {total}')

for i, (mid, config_bits) in enumerate(se_nse.config_bits):
    if config_bits:
        total += 1 + len(config_bits)
        print(f'SE_NSE[{i}]: {len(config_bits)} bits (+1 enable), cumulative: {total}')
    else:
        total += 1
        print(f'SE_NSE[{i}]: 0 bits (+1 enable=0), cumulative: {total}')

print(f'\nBefore buffer/special: {total} bits')

# Buffer configs
for i, m in enumerate(buffer_configs):
    bits = bitstring(m.to_bits())
    total += 1 + len(bits)
    print(f'BUFFER[{i}]: {len(bits)} bits (+1 enable), cumulative: {total}')

# Special array
bits = bitstring(special_module.to_bits())
total += 1 + len(bits)
print(f'SPECIAL_ARRAY: {len(bits)} bits (+1 enable), cumulative: {total}')

print(f'\nTotal before padding: {total} bits')
pad_len = (64 - total % 64) % 64
print(f'Padding: {pad_len} bits')
print(f'Total: {total + pad_len} bits')
print(f'Lines: {(total + pad_len) // 64}')
