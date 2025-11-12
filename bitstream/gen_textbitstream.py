#!/usr/bin/env python3
"""Generate text-format bitstream matching config_generator_ver2.py output."""

import sys
import os
from enum import IntEnum
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
    ReadStreamConfig,
    WriteStreamConfig,
)
from bitstream.index import NodeIndex

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

def split_config(config):
    """Split config string into chunks of up to 63 bits each."""
    total_len = len(config)
    if total_len == 0:
        return []

    for i in range(min(63, total_len), 0, -1):
        if total_len % i == 0:
            chunk_size = i
            break
        
    chunks = [config[i:i + chunk_size] for i in range(0, total_len, chunk_size)]
    return chunks

def bitstring(bits):
    """Convert list of Bit objects to string."""
    return ''.join(format(bit.value, f'0{bit.width}b') for bit in bits)

def main():
    # Load configuration - use converted config with aligned stream structure
    with open('./data/gemm_config_reference_aligned_aligned_with_config.json') as f:
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
    
    # 6. Initialize stream engine configs using config.stream module
    # Mapping between JSON and hardware indices:
    # JSON stream0 (weight) -> hardware index 0 (SE_RD_MSE[0])
    # JSON stream1 (activation) -> hardware index 1 (SE_RD_MSE[1])
    # JSON stream2 (output, write) -> hardware index 0 (SE_WR_MSE[0])
    #
    # But config_generator_ver2.py uses:
    # rd_mse_params_1 (weight) -> index 0
    # rd_mse_params_0 (activation) -> index 1
    # wr_mse_params_0 (output) -> index 0
    
    # Create stream configs - 3 read streams and 1 write stream
    read_stream_weight = ReadStreamConfig()  # JSON stream0 -> weight
    read_stream_activation = ReadStreamConfig()  # JSON stream1 -> activation
    read_stream_unused = ReadStreamConfig()  # Unused stream (can be empty)
    write_stream_output = WriteStreamConfig()  # JSON stream2 -> output (write)
    
    # Parse from JSON - each stream from its corresponding stream_engine entry
    stream_engine = cfg.get('stream_engine', {})
    if 'stream0' in stream_engine:
        read_stream_weight.from_json(stream_engine['stream0'])
    if 'stream1' in stream_engine:
        read_stream_activation.from_json(stream_engine['stream1'])
    if 'stream2' in stream_engine:
        write_stream_output.from_json(stream_engine['stream2'])
    
    # Resolve any remaining indices
    NodeIndex.resolve_all()
    
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
    
    # Stream engines - using config.stream module
    # SE_RD_MSE entries (3 engines)
    # Note: config_generator_ver2.py has weight at index 0, activation at index 1
    entries.append((ModuleID.SE_RD_MSE, bitstring(read_stream_weight.to_bits())))  # index 0: weight
    entries.append((ModuleID.SE_RD_MSE, bitstring(read_stream_activation.to_bits())))  # index 1: activation
    entries.append((ModuleID.SE_RD_MSE, ''))  # index 2: unused
    
    # SE_WR_MSE entries (1 engine)
    entries.append((ModuleID.SE_WR_MSE, bitstring(write_stream_output.to_bits())))  # index 0: output (write)
    
    # SE_NSE entries (2 engines: neighbor streams, currently not used in this config)
    # Add empty configs for NSE
    neighbor_stream = NeighborStreamConfig()
    neighbor_stream.from_json(cfg)
    entries.append((ModuleID.SE_NSE, bitstring(neighbor_stream.to_bits())))
    entries.append((ModuleID.SE_NSE, ''))  # Second NSE is empty
    
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
    
    # Return detailed section info for comparison
    section_info = {
        'bitstream': bitstream,
        'entries': entries,
        'config_mask': config_mask,
    }
    return section_info

def compare_bitstreams(generated_info):
    """Compare generated bitstream with reference section by section."""
    bitstream = generated_info['bitstream']
    entries = generated_info['entries']
    config_mask = generated_info['config_mask']
    
    # Load reference
    try:
        with open('data/bitstream.txt') as f:
            ref = ''.join(line.strip() for line in f)
    except FileNotFoundError:
        print("Reference file not found!")
        return
    
    print(f"\n{'='*80}")
    print(f"BITSTREAM COMPARISON")
    print(f"{'='*80}")
    print(f"Generated: {len(bitstream)} bits ({len(bitstream)//64} lines)")
    print(f"Reference: {len(ref)} bits ({len(ref)//64} lines)")
    print(f"Difference: {len(ref) - len(bitstream)} bits")
    
    # Compare config mask
    print(f"\n{'='*80}")
    print(f"CONFIG MASK (8 bits)")
    print(f"{'='*80}")
    gen_mask = bitstream[:8]
    ref_mask = ref[:8]
    print(f"Generated: {gen_mask}")
    print(f"Reference: {ref_mask}")
    print(f"Match: {'✓' if gen_mask == ref_mask else '✗'}")
    
    # Track bit position
    gen_pos = 8
    ref_pos = 8
    
    # Module names for display
    module_names = {
        ModuleID.IGA_LC: "IGA_LC",
        ModuleID.IGA_ROW_LC: "IGA_ROW_LC",
        ModuleID.IGA_COL_LC: "IGA_COL_LC",
        ModuleID.IGA_PE: "IGA_PE",
        ModuleID.SE_RD_MSE: "SE_RD_MSE",
        ModuleID.SE_WR_MSE: "SE_WR_MSE",
        ModuleID.SE_NSE: "SE_NSE",
        ModuleID.BUFFER_MANAGER_CLUSTER: "BUFFER_MANAGER_CLUSTER",
        ModuleID.SPECIAL_ARRAY: "SPECIAL_ARRAY",
    }
    
    # Group entries by module type
    from collections import defaultdict
    module_groups = defaultdict(list)
    for mid, config in entries:
        module_groups[mid].append(config)
    
    # Compare each module type
    for mid in [ModuleID.IGA_LC, ModuleID.IGA_ROW_LC, ModuleID.IGA_COL_LC, 
                ModuleID.IGA_PE, ModuleID.SE_RD_MSE, ModuleID.SE_WR_MSE, 
                ModuleID.SE_NSE, ModuleID.BUFFER_MANAGER_CLUSTER, ModuleID.SPECIAL_ARRAY]:
        
        if not config_mask[ModuleID2Mask[mid]]:
            continue
            
        configs = module_groups[mid]
        print(f"\n{'='*80}")
        print(f"{module_names[mid]} ({len(configs)} entries)")
        print(f"{'='*80}")
        
        for idx, config in enumerate(configs):
            # Calculate expected bits for this entry
            if not config or set(config) == {'0'}:
                # Empty config
                gen_bits = '0' * MODULE_CFG_CHUNK_SIZES[mid]
                gen_section = bitstream[gen_pos:gen_pos+MODULE_CFG_CHUNK_SIZES[mid]]
                ref_section = ref[ref_pos:ref_pos+MODULE_CFG_CHUNK_SIZES[mid]]
                
                match = gen_section == ref_section
                print(f"\n  [{idx}] Empty config ({MODULE_CFG_CHUNK_SIZES[mid]} enable bits)")
                print(f"    Generated: {gen_section}")
                print(f"    Reference: {ref_section}")
                print(f"    Match: {'✓' if match else '✗'}")
                
                gen_pos += MODULE_CFG_CHUNK_SIZES[mid]
                ref_pos += MODULE_CFG_CHUNK_SIZES[mid]
            else:
                # Has data
                chunks = split_config(config)
                print(f"\n  [{idx}] {len(config)} bits -> {len(chunks)} chunks")
                
                all_match = True
                for chunk_idx, chunk in enumerate(chunks):
                    chunk_with_enable = '1' + chunk
                    gen_section = bitstream[gen_pos:gen_pos+len(chunk_with_enable)]
                    ref_section = ref[ref_pos:ref_pos+len(chunk_with_enable)]
                    
                    match = gen_section == ref_section
                    all_match = all_match and match
                    
                    if not match or chunk_idx == 0:  # Always show first chunk, and mismatches
                        print(f"    Chunk {chunk_idx}: {len(chunk)} bits (+1 enable)")
                        print(f"      Generated: {gen_section}")
                        print(f"      Reference: {ref_section}")
                        print(f"      Match: {'✓' if match else '✗'}")
                    
                    gen_pos += len(chunk_with_enable)
                    ref_pos += len(chunk_with_enable)
                
                if len(chunks) > 1:
                    print(f"    Overall: {'✓ All chunks match' if all_match else '✗ Some chunks differ'}")
    
    # Check remaining bits (padding)
    if gen_pos < len(bitstream) or ref_pos < len(ref):
        print(f"\n{'='*80}")
        print(f"PADDING")
        print(f"{'='*80}")
        gen_padding = bitstream[gen_pos:]
        ref_padding = ref[ref_pos:]
        print(f"Generated padding: {len(gen_padding)} bits")
        print(f"Reference padding: {len(ref_padding)} bits")
        if gen_padding or ref_padding:
            print(f"Generated: {gen_padding[:64]}{'...' if len(gen_padding) > 64 else ''}")
            print(f"Reference: {ref_padding[:64]}{'...' if len(ref_padding) > 64 else ''}")
            print(f"Match: {'✓' if gen_padding == ref_padding else '✗'}")
    
    # Summary
    print(f"\n{'='*80}")
    print(f"SUMMARY")
    print(f"{'='*80}")
    total_match = bitstream == ref
    if total_match:
        print("✓ PERFECT MATCH! Generated bitstream matches reference exactly.")
    else:
        # Find first difference
        for i in range(min(len(bitstream), len(ref))):
            if bitstream[i] != ref[i]:
                print(f"✗ First difference at bit {i}")
                print(f"  Context (bits {max(0,i-20)}:{i+20}):")
                print(f"    Generated: {bitstream[max(0,i-20):i+20]}")
                print(f"    Reference: {ref[max(0,i-20):i+20]}")
                break
        else:
            if len(bitstream) != len(ref):
                print(f"✗ Bitstreams match up to bit {min(len(bitstream), len(ref))}, but lengths differ")

if __name__ == '__main__':
    result = main()
    compare_bitstreams(result)
