#!/usr/bin/env python3
"""Generate text-format bitstream."""

import sys
import os
from enum import IntEnum
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

import json
from bitstream.config.mapper import NodeGraph
from bitstream.config import (
    DramLoopControlConfig, BufferLoopControlGroupConfig, LCPEConfig,
    NeighborStreamConfig, BufferConfig, SpecialArrayConfig
)
from bitstream.config.stream import StreamConfig
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

MODULE_CFG_CHUNK_SIZES = [1, 1, 1, 1, 8, 6, 1, 1, 1, 1, 1, 4]
MODULE_ID_TO_MASK = [0, 0, 0, 0, 1, 1, 1, 1, 2, 3, 3, 3]

def split_config(config, max_chunk=63):
    """Split config into equal chunks."""
    if not config:
        return []
    total = len(config)
    for size in range(min(max_chunk, total), 0, -1):
        if total % size == 0:
            return [config[i:i+size] for i in range(0, total, size)]
    return [config]

def bitstring(bits):
    """Convert Bit objects to binary string."""
    return ''.join(f'{bit.value:0{bit.width}b}' for bit in bits)

def load_config(config_file='./data/gemm_config_reference_aligned.json'):
    """Load and parse JSON configuration."""
    with open(config_file) as f:
        return json.load(f)

def init_modules(cfg, use_direct_mapping=False, use_heuristic_search=True, heuristic_iterations=5000):
    """Initialize all hardware modules from config and perform resource mapping.
    
    Args:
        cfg: Configuration dictionary
        use_direct_mapping: If True, use direct logical→physical mapping without constraint search
        use_heuristic_search: If True, use simulated annealing heuristic search for large graphs
        heuristic_iterations: Maximum iterations for heuristic search (default: 5000)
    """
    # Reset NodeIndex state for clean initialization
    NodeIndex._queue = []
    NodeIndex._registry = {}
    NodeIndex._counter = 0
    NodeIndex._resolved = False
    
    # Reset NodeGraph singleton BEFORE creating modules
    # This must happen before from_json is called, as Connect() will populate it
    NodeGraph._instance = None
    
    # Create all modules in a unified list
    modules =   [DramLoopControlConfig(i) for i in range(8)] + \
                [BufferLoopControlGroupConfig(i) for i in range(4)] + \
                [LCPEConfig(i) for i in range(8)] + \
                [StreamConfig(i) for i in range(4)] + \
                [NeighborStreamConfig()] + \
                [BufferConfig(i) for i in range(6)] + \
                [SpecialArrayConfig()]
    
    # Load configurations from JSON for all modules
    # During this process, Connect() objects will populate NodeGraph.connections
    print("\n=== Loading Configurations from JSON ===")
    for module in modules:
        module.from_json(cfg)
    
    # Perform resource allocation and mapping
    print("\n=== Resource Allocation & Mapping ===")
    
    # Set direct mapping mode before allocation if requested
    if use_direct_mapping:
        NodeGraph.get().mapping.use_direct_mapping = True
        print("[Mode] Direct logical→physical index mapping enabled")
    
    # First, only allocate resources for nodes that appear in connections
    NodeGraph.get().allocate_resources(only_connected_nodes=True)
    
    # Choose mapping strategy based on parameters (heuristic enabled by default)
    if use_direct_mapping:
        # Direct mapping: use allocation order without constraint search
        print("\n=== Using Direct Mapping (No Constraint Search) ===")
        NodeGraph.get().direct_mapping()
    elif use_heuristic_search:
        # Heuristic search: use simulated annealing for large graphs
        print("\n=== Using Heuristic Search (Simulated Annealing) ===")
        NodeGraph.get().heuristic_search_mapping(max_iterations=heuristic_iterations)
    
    # After mapping, allocate remaining resources for unconnected nodes
    NodeGraph.get().allocate_resources(only_connected_nodes=False)
    
    # Resolve node indices and auto-register modules to mapper
    NodeIndex.resolve_all(modules)

    # Print mapping summary
    NodeGraph.get().mapping.summary()
    
    return modules

def build_entries(modules):
    """
    Build bitstream entries in physical resource order.
    Uses mapper's direct module lookup for efficient access.
    """
    entries = []
    print("\n=== Building Bitstream Entries ===")
    
    mapper = NodeGraph.get().mapping
    
    def add_entry(module_id, module):
        """Helper to add an entry with proper bitstring conversion."""
        if module:
            entries.append((module_id, bitstring(module.to_bits())))
        else:
            entries.append((module_id, ''))
    
    def get_submodule(group_idx, suffix):
        """Helper to get ROW_LC or COL_LC submodule from a GROUP."""
        parent = mapper.get_module(f"GROUP{group_idx}")
        if parent and hasattr(parent, 'submodules'):
            return next((sub for sub in parent.submodules 
                        if hasattr(sub, 'id') and sub.id.node_name.endswith(suffix)), None)
        return None
    
    # Get fixed-position modules from module list
    buffer_modules = [m for m in modules if isinstance(m, BufferConfig)]
    neighbor_module = next((m for m in modules if isinstance(m, NeighborStreamConfig)), None)
    special_module = next((m for m in modules if isinstance(m, SpecialArrayConfig)), None)
    
    # Physical resource layout: (module_id, resource_pattern, count, getter_func)
    layout = [
        # IGA resources
        (ModuleID.IGA_LC, "LC", 8, lambda i: mapper.get_module(f"LC{i}")),
        (ModuleID.IGA_ROW_LC, "GROUP", 4, lambda i: get_submodule(i, 'ROW_LC')),
        (ModuleID.IGA_COL_LC, "GROUP", 4, lambda i: get_submodule(i, 'COL_LC')),
        (ModuleID.IGA_PE, "PE", 8, lambda i: mapper.get_module(f"PE{i}")),
        # Stream Engine resources
        (ModuleID.SE_RD_MSE, "READ_STREAM", 3, lambda i: mapper.get_module(f"READ_STREAM{i}")),
        (ModuleID.SE_WR_MSE, "WRITE_STREAM", 1, lambda i: mapper.get_module(f"WRITE_STREAM{i}")),
        (ModuleID.SE_NSE, "NEIGHBOR", 1, lambda i: neighbor_module),
        (ModuleID.SE_NSE, "NEIGHBOR_EMPTY", 1, lambda i: None),  # Extra empty entry
        # Buffer manager cluster
        (ModuleID.BUFFER_MANAGER_CLUSTER, "BUFFER", len(buffer_modules), lambda i: buffer_modules[i] if i < len(buffer_modules) else None),
        # Special array
        (ModuleID.SPECIAL_ARRAY, "SPECIAL", 1, lambda i: special_module),
    ]
    
    # Build all entries from unified layout
    for module_id, resource_type, count, getter in layout:
        for i in range(count):
            add_entry(module_id, getter(i))
    
    return entries

def generate_bitstream(entries, config_mask):
    """Generate bitstream from entries."""
    bitstream = ''.join(str(x) for x in config_mask)
    
    for mid, config in entries:
        if not config_mask[MODULE_ID_TO_MASK[mid]]:
            continue
        
        if not config or set(config) == {'0'}:
            bitstream += '0' * MODULE_CFG_CHUNK_SIZES[mid]
        else:
            for chunk in split_config(config):
                bitstream += '1' + chunk
    
    # Pad to 64-bit boundary
    bitstream += '0' * ((64 - len(bitstream) % 64) % 64)
    return bitstream

def write_bitstream(entries, output_file='./data/parsed_bitstream.txt'):
    """
    Write bitstream in human-readable format.
    Groups entries by module type and shows each configuration.
    Uses same chunking logic as generate_bitstream.
    """
    from collections import defaultdict
    
    # Module names for display
    module_names = {
        ModuleID.IGA_LC: "iga_lc",
        ModuleID.IGA_ROW_LC: "iga_row_lc",
        ModuleID.IGA_COL_LC: "iga_col_lc",
        ModuleID.IGA_PE: "iga_pe",
        ModuleID.SE_RD_MSE: "se_rd_mse",
        ModuleID.SE_WR_MSE: "se_wr_mse",
        ModuleID.SE_NSE: "se_nse",
        ModuleID.BUFFER_MANAGER_CLUSTER: "buffer_manager_cluster",
        ModuleID.SPECIAL_ARRAY: "special_array",
        ModuleID.GA_INPORT_GROUP: "ga_inport_group",
        ModuleID.GA_OUTPORT_GROUP: "ga_outport_group",
        ModuleID.GENERAL_ARRAY: "general_array",
    }
    
    # Group entries by module type
    module_groups = defaultdict(list)
    for mid, config in entries:
        module_groups[mid].append(config)
    
    # Write to file
    with open(output_file, 'w') as f:
        # Process each module type in order
        for mid in sorted(module_groups.keys()):
            configs = module_groups[mid]
            module_name = module_names.get(mid, f"module_{mid}")
            
            # Write module header
            f.write(f"{module_name}:\n")
            
            # Write each configuration entry
            for config in configs:
                if not config or set(config) == {'0'}:
                    # Empty configuration
                    f.write(f"0\n")
                else:
                    # Valid configuration with data
                    # Use same chunking logic as generate_bitstream
                    for chunk in split_config(config):
                        f.write(f"1 {chunk}\n")
            
            # Add blank line after each module group
            f.write("\n")
    
    print(f'Bitstream written to {output_file}')

def dump_modules_detailed(modules, output_file=None):
    """
    Dump detailed field-by-field encoding information for all modules.
    Calls each module's dump() method to show values and their binary encodings.
    Skips empty modules and avoids unnecessary blank lines.
    """
    print("\n=== Detailed Module Configuration Dump ===")
    
    for module in modules:
        if hasattr(module, 'dump'):
            if module.dump():  # Only add blank line if content was printed
                print()
    
    # Optionally write to file
    if output_file:
        import sys
        from io import StringIO
        
        # Capture stdout
        old_stdout = sys.stdout
        sys.stdout = captured_output = StringIO()
        
        for module in modules:
            if hasattr(module, 'dump'):
                if module.dump():  # Only add blank line if content was printed
                    print()
        
        # Restore stdout
        sys.stdout = old_stdout
        
        # Write captured output to file
        with open(output_file, 'w') as f:
            f.write(captured_output.getvalue())
        
        print(f"Detailed dump written to {output_file}")

def dump_modules_to_binary(modules, output_file='./data/modules_dump.bin'):
    """
    Unified function to dump all modules to binary format.
    Processes all modules in the unified list, calls to_bits() on each,
    and writes the result to a binary file.
    Skips empty modules (all None or 0).
    """
    print("\n=== Dumping All Modules to Binary ===")
    all_bits = []
    
    for module in modules:
        # Skip empty modules
        if hasattr(module, 'is_empty') and module.is_empty():
            continue
        
        module_name = type(module).__name__
        bits = module.to_bits()
        bit_count = len(bits)
        
        if bit_count > 0:
            print(f"{module_name:30s}: {bit_count:4d} bits")
            all_bits.extend(bits)
    
    # Convert bits to binary string
    binary_string = bitstring(all_bits)
    
    # Write to file
    with open(output_file, 'w') as f:
        # Write as hex for readability
        for i in range(0, len(binary_string), 64):
            chunk = binary_string[i:i+64]
            f.write(chunk + '\n')
    
    print(f"\nTotal: {len(all_bits)} bits written to {output_file}")
    return binary_string

def main():
    """Main entry point."""
    cfg = load_config()
    modules = init_modules(cfg)
    
    # TODO: Visualize the resource mapping (currently disabled due to performance issues)
    # from bitstream.config.mapper import visualize_mapping
    # print("\n=== Generating Placement Visualization ===")
    # mapper = NodeGraph.get().mapping
    # connections = NodeGraph.get().connections
    # visualize_mapping(mapper, connections)
    
    # Option 1: Unified dump of all modules (new approach)
    unified_binary = dump_modules_to_binary(modules)
    
    # Option 2: Traditional bitstream generation (for comparison with reference)
    entries = build_entries(modules)
    config_mask = [1, 1, 1, 0, 1, 1, 1, 0]  # Enable: IGA, SE, Buffer, Special
    bitstream = generate_bitstream(entries, config_mask)
    write_bitstream(entries)
    
    return {
        'bitstream': bitstream, 
        'entries': entries, 
        'config_mask': config_mask,
        'unified_binary': unified_binary,
        'modules': modules
    }

def compare_bitstreams(generated_info, reference_file=None):
    """Compare generated bitstream with reference section by section."""
    bitstream = generated_info['bitstream']
    entries = generated_info['entries']
    config_mask = generated_info['config_mask']
    
    # Get reference file from generated_info or use provided parameter
    if reference_file is None:
        reference_file = generated_info.get('reference_file', 'data/bitstream.txt')
    
    # Load reference
    try:
        with open(reference_file) as f:
            ref = ''.join(line.strip() for line in f)
    except FileNotFoundError:
        print(f"Reference file not found: {reference_file}")
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
        
        if not config_mask[MODULE_ID_TO_MASK[mid]]:
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
