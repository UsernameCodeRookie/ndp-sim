#!/usr/bin/env python3
"""Generate text-format bitstream matching config_generator_ver2.py output."""

import sys
import os
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

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
from config.utils.module_idx import ModuleID, MODULE_CFG_CHUNK_SIZES, ModuleID2Mask
from config.config_generator_ver2 import split_config
from config.component_config import se_rd_mse, se_wr_mse, se_nse
from config.component_config import se_rd_mse, se_wr_mse, se_nse

# Import component_config modules for stream engines
from config.component_config import se_rd_mse, se_wr_mse, se_nse, buffer_manager_cluster, special_array as special_array_config

def bitstring(bits):
    """Convert list of Bit objects to string."""
    return ''.join(format(bit.value, f'0{bit.width}b') for bit in bits)

def main():
    # Load configuration
    with open('./data/gemm_config_reference_aligned.json') as f:
        cfg = json.load(f)
    
    # Initialize modules in dependency order:
    # 1. DRAM LC first (no dependencies)
    dram_modules = [DramLoopControlConfig(i) for i in range(8)]
    for m in dram_modules:
        m.from_json(cfg)
    
    # 2. Allocate and resolve DRAM LC nodes
    NodeGraph.get().allocate_resources()
    NodeGraph.get().search_mapping()
    NodeIndex.resolve_all()
    
    # 3. Now initialize buffer groups (which reference DRAM LC)
    buffer_groups = [BufferLoopControlGroupConfig(i) for i in range(4)]
    for m in buffer_groups:
        m.from_json(cfg)
    
    # 4. Initialize remaining modules
    pe_modules = [LCPEConfig(i) for i in range(8)]  # 8 PE configs (PE0-PE7)
    neighbor_module = NeighborStreamConfig()
    buffer_configs = [BufferConfig(i) for i in range(6)]
    special_module = SpecialArrayConfig()
    
    for m in pe_modules + [neighbor_module] + buffer_configs + [special_module]:
        m.from_json(cfg)
    
    # 5. Final resolution
    NodeIndex.resolve_all()
    
    # 6. Convert JSON stream_engine configs to component_config format
    def json_to_hw_params(stream_cfg):
        """Convert JSON stream config to hardware params format."""
        mem_ag = stream_cfg.get('memory_AG', {})
        buf_ag = stream_cfg.get('buffer_AG', {})
        stream = stream_cfg.get('stream', {})
        hw = stream_cfg.get('_hw_params', {})
        
        return {
            "mse_enable": 1,
            "mse_mem_idx_mode": mem_ag.get('idx_keep_mode', [0, 0, 0]),
            "mse_mem_idx_keep_last_index": mem_ag.get('idx_keep_last_index', [0, 0, 0]),
            "mem_inport_src_id": mem_ag.get('idx', [0, 0, 0]),
            "mse_mem_idx_constant": mem_ag.get('idx_constant', [0, 0, 0]),
            "mse_buf_idx_mode": buf_ag.get('idx_keep_mode', [0, 0]),
            "mse_buf_idx_keep_last_index": buf_ag.get('idx_keep_last_index', [0, 0]),
            "mse_pingpong_enable": stream.get('ping_pong', 0),
            "mse_pingpong_last_index": stream.get('pingpong_last_index', 0),
            "mse_stream_base_addr": mem_ag.get('base_addr', 0),
            "mse_transaciton_layout_size": mem_ag.get('idx_size', [0, 0, 0]),
            "mse_transaciton_layout_size_log": hw.get('mse_transaciton_layout_size_log', [0, 0, 0]),
            "mse_transaciton_total_size": hw.get('mse_transaciton_total_size', 0),
            "mse_transaciton_mult": mem_ag.get('dim_stride', [0, 0, 0]),
            "mse_map_matrix_b": mem_ag.get('address_remapping', list(range(16))),
            "mse_padding_reg_value": mem_ag.get('padding_reg_value', 0),
            "mse_padding_valid": mem_ag.get('padding_enable', [0, 0, 0]),
            "mse_padding_low_bound": mem_ag.get('idx_padding_range', {}).get('low_bound', [0, 0, 0]) if isinstance(mem_ag.get('idx_padding_range'), dict) else [0, 0, 0],
            "mse_padding_up_bound": mem_ag.get('idx_padding_range', {}).get('up_bound', [0, 0, 0]) if isinstance(mem_ag.get('idx_padding_range'), dict) else [0, 0, 0],
            "mse_branch_valid": mem_ag.get('tailing_enable', [0, 0, 0]),
            "mse_branch_low_bound": mem_ag.get('idx_tailing_range', {}).get('low', [0, 0, 0]) if isinstance(mem_ag.get('idx_tailing_range'), dict) else [0, 0, 0],
            "mse_branch_up_bound": mem_ag.get('idx_tailing_range', {}).get('up', [0, 0, 0]) if isinstance(mem_ag.get('idx_tailing_range'), dict) else [0, 0, 0],
            "mse_buf_spatial_stride": buf_ag.get('spatial_stride', list(range(16))),
            "mse_buf_spatial_size": buf_ag.get('spatial_size', 0),
        }
    
    # Reset component_config arrays
    se_rd_mse.config_bits = [[ModuleID.SE_RD_MSE, None] for _ in range(3)]
    se_wr_mse.config_bits = [[ModuleID.SE_WR_MSE, None] for _ in range(1)]
    se_nse.config_bits = [[ModuleID.SE_NSE, None] for _ in range(2)]
    
    # Process stream configs from JSON
    stream_engine = cfg.get('stream_engine', {})
    for stream_key in ['stream0', 'stream1', 'stream2']:
        if stream_key in stream_engine:
            stream_cfg = stream_engine[stream_key]
            mode = stream_cfg.get('memory_AG', {}).get('mode', 'read')
            hw_params = json_to_hw_params(stream_cfg)
            
            if mode == 'read':
                # Determine which SE_RD_MSE index to use
                # stream0 -> index 0, stream1 -> index 1
                idx = 0 if stream_key == 'stream0' else 1
                se_rd_mse.get_config_bits(hw_params, idx)
            elif mode == 'write':
                # stream2 -> SE_WR_MSE index 0
                se_wr_mse.get_config_bits(hw_params, 0)
    
    # SE_NSE from n2n config (currently disabled)
    # neighbor_module already loaded from JSON
    
    # Build entries list with (ModuleID, bitstring) tuples
    entries = []
    
    # DRAM LC (IGA_LC) - 8 entries
    dram_lc = dram_modules  # Already sorted by physical_index during initialization
    dram_lc.sort(key=lambda m: m.physical_index if hasattr(m, 'physical_index') and m.physical_index is not None else 999)
    entries.extend((ModuleID.IGA_LC, bitstring(m.to_bits())) for m in dram_lc)
    
    # Buffer groups - ALL ROW_LC first, then ALL COL_LC
    for group in buffer_groups:
        row, col = group.submodules
        entries.append((ModuleID.IGA_ROW_LC, bitstring(row.to_bits())))
    
    for group in buffer_groups:
        row, col = group.submodules
        entries.append((ModuleID.IGA_COL_LC, bitstring(col.to_bits())))
    
    # PE configs (IGA_PE)
    entries.extend((ModuleID.IGA_PE, bitstring(m.to_bits())) for m in pe_modules)
    
    # Stream engines - use component_config generated bitstreams
    # SE_RD_MSE entries (3 engines: indices 0, 1, 2)
    for mid, config_bits in se_rd_mse.config_bits:
        entries.append((mid, config_bits if config_bits else ''))
    
    # SE_WR_MSE entries (1 engine: index 0)
    for mid, config_bits in se_wr_mse.config_bits:
        entries.append((mid, config_bits if config_bits else ''))
    
    # SE_NSE entries (2 engines: indices 0, 1)
    for mid, config_bits in se_nse.config_bits:
        entries.append((mid, config_bits if config_bits else ''))
    
    # Buffer manager
    entries.extend((ModuleID.BUFFER_MANAGER_CLUSTER, bitstring(m.to_bits())) for m in buffer_configs)
    
    # Special array
    entries.append((ModuleID.SPECIAL_ARRAY, bitstring(special_module.to_bits())))
    
    # Generate bitstream with enable bits
    config_mask = [1, 1, 1, 0, 1, 1, 1, 0]  # Enable IGA, SE, Buffer, Special; disable GA
    bitstream = ''.join(str(x) for x in config_mask)
    
    for mid, config in entries:
        if not config_mask[ModuleID2Mask[mid]]:
            continue
        # Check if config is empty (all zeros or no data)
        if not config or set(config) == {'0'}:
            # Empty config: add one '0' per chunk
            bitstream += '0' * MODULE_CFG_CHUNK_SIZES[mid]
        else:
            chunks = split_config(config)
            for chunk in chunks:
                bitstream += '1' + chunk
    
    # Pad to 64-bit boundary
    pad_len = (64 - len(bitstream) % 64) % 64
    bitstream += '0' * pad_len
    
    # Write to file in 64-bit chunks
    with open('./data/generated_bitstream.txt', 'w') as f:
        for i in range(0, len(bitstream), 64):
            f.write(bitstream[i:i+64] + '\n')
    
    print(f'Generated {len(bitstream)} bits ({len(bitstream)//64} lines)')
    return bitstream

if __name__ == '__main__':
    bitstream = main()
    
    # Debug: show bit 333 context
    if bitstream:
        with open('data/bitstream.txt') as f:
            ref = ''.join(line.strip() for line in f)
        print(f'\nBit 333 context (bits 320-350):')
        print(f'  Generated: {bitstream[320:350]}')
        print(f'  Reference: {ref[320:350]}')
        print(f'\nBit 333:')
        print(f'  Generated: {bitstream[333]}')
        print(f'  Reference: {ref[333]}')
