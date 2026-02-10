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
    NeighborStreamConfig, BufferConfig, SpecialArrayConfig, GAInportConfig, GAOutportConfig, GAPEConfig
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
    GENERAL_PE = 11

MODULE_CFG_CHUNK_SIZES = [1, 1, 1, 2, 10, 8, 1, 1, 1, 1, 1, 4]
MODULE_ID_TO_MASK = [0, 0, 0, 0, 1, 1, 1, 1, 2, 3, 3, 3]

def split_config(config, module_id):
    """Split configuration into chunks based on MODULE_CFG_CHUNK_SIZES.
    
    Args:
        config: Binary string to split
        module_id: ModuleID enum value to look up number of chunks in MODULE_CFG_CHUNK_SIZES
    
    Returns:
        List of config chunks, where number of chunks = MODULE_CFG_CHUNK_SIZES[module_id]
    """
    if not config or len(config) == 0:
        return []
    
    num_chunks = MODULE_CFG_CHUNK_SIZES[module_id]
    if num_chunks <= 0:
        return []
    
    chunk_size = len(config) // num_chunks
    if chunk_size == 0:
        # If config is too small to divide into num_chunks, treat as single chunk
        return [config]
    
    chunks = [config[i:i + chunk_size] for i in range(0, len(config), chunk_size)]
    return chunks

def bitstring(bits):
    """Convert Bit objects to binary string."""
    return ''.join(f'{bit.value:0{bit.width}b}' for bit in bits)

def load_config(config_file='./data/gemm_config_reference_aligned.json'):
    """Load and parse JSON configuration."""
    with open(config_file) as f:
        return json.load(f)

def init_modules(cfg, use_direct_mapping=False, use_heuristic_search=True, heuristic_iterations=5000, heuristic_restarts=1, seed=None):
    """Initialize all hardware modules from config and perform resource mapping.
    
    Args:
        cfg: Configuration dictionary
        use_direct_mapping: If True, use direct logical→physical mapping without constraint search
        use_heuristic_search: If True, use simulated annealing heuristic search for large graphs
        heuristic_iterations: Maximum iterations for heuristic search (default: 5000)
        heuristic_restarts: Number of restart attempts if heuristic search fails (default: 1)
        seed: Random seed for reproducibility in heuristic search (default: None)
    """
    # Reset NodeIndex state for clean initialization
    NodeIndex._queue = []
    NodeIndex._registry = {}
    NodeIndex._counter = 0
    NodeIndex._resolved = False
    
    # Set random seed BEFORE any random operations (including module initialization)
    # if seed is not None:
    #     import random
    #     print(f"[Seed] Using random seed: {seed}")
    #     random.seed(seed)
    
    # Reset NodeGraph singleton BEFORE creating modules
    # This must happen before from_json is called, as Connect() will populate it
    NodeGraph._instance = None
    
    # Create all modules in a unified list
    # Updated counts: 20 IGA_LC (2x10), 5 ROW_LC groups, 10 IGA_PE, 4 READ + 1 WRITE streams (total 5 configs)
    modules =   [DramLoopControlConfig(i) for i in range(20)] + \
                [BufferLoopControlGroupConfig(i) for i in range(5)] + \
                [LCPEConfig(i) for i in range(10)] + \
                [StreamConfig(i) for i in range(5)] + \
                [NeighborStreamConfig(i) for i in range(2)] + \
                [BufferConfig(i) for i in range(6)] + \
                [SpecialArrayConfig()] + \
                [GAInportConfig(i) for i in range(3)] + \
                [GAOutportConfig()] + \
                [GAPEConfig(f"PE{row}{col}") for row in range(4) for col in range(4)]
    
    # Load configurations from JSON for all modules
    # During this process, Connect() objects will populate NodeGraph.connections
    print("\n=== Loading Configurations from JSON ===")
    for module in modules:
        module.from_json(cfg)
    
    # Perform resource allocation and mapping
    print("\n=== Resource Allocation & Mapping ===")
    
    # Initialize NodeGraph with seed if provided
    # We need to preserve connections that were populated during from_json()
    if seed is not None:
        old_graph = NodeGraph.get()
        saved_connections = old_graph.connections.copy()
        saved_nodes = old_graph.nodes.copy()
        saved_metadata = old_graph.node_metadata.copy()
        saved_assigned = old_graph.mapping.assigned_node.copy()  # Save pre-assigned nodes
        saved_mapping = old_graph.mapping.node_to_resource.copy()  # Save mappings
        NodeGraph._instance = NodeGraph(seed=seed)
        NodeGraph.get().connections = saved_connections
        NodeGraph.get().nodes = saved_nodes
        NodeGraph.get().node_metadata = saved_metadata
        NodeGraph.get().mapping.assigned_node = saved_assigned  # Restore pre-assigned
        NodeGraph.get().mapping.node_to_resource = saved_mapping  # Restore mappings
    
    # Set direct mapping mode before allocation if requested
    if use_direct_mapping:
        NodeGraph.get().mapping.use_direct_mapping = True
        print("[Mode] Direct logical→physical index mapping enabled")
    
    # First, only allocate resources for nodes that appear in connections
    NodeGraph.get().allocate_resources(only_connected_nodes=True)
    
    # Choose mapping strategy based on parameters (heuristic enabled by default)
    mapping_success = False
    if use_direct_mapping:
        # Direct mapping: use allocation order without constraint search
        print("\n=== Using Direct Mapping (No Constraint Search) ===")
        NodeGraph.get().direct_mapping()
        mapping_success = True
    elif use_heuristic_search:
        # Heuristic search: use simulated annealing for large graphs with retries
        print("\n=== Using Heuristic Search (Simulated Annealing) ===")
        for attempt in range(heuristic_restarts):
            if attempt > 0:
                print(f"\n[Retry] Attempt {attempt + 1}/{heuristic_restarts}")
                # Reset NodeGraph and reload configurations for retry
                NodeGraph._instance = None
                NodeIndex._queue = []
                NodeIndex._registry = {}
                NodeIndex._counter = 0
                NodeIndex._resolved = False
                
                # Reset random seed before reloading (for reproducibility)
                # if seed is not None:
                #     import random
                #     random.seed(seed)
                
                # Reload modules
                for module in modules:
                    module.from_json(cfg)
                # Reinitialize NodeGraph with seed if provided
                # Preserve connections that were populated during from_json()
                if seed is not None:
                    old_graph = NodeGraph.get()
                    saved_connections = old_graph.connections.copy()
                    saved_nodes = old_graph.nodes.copy()
                    saved_metadata = old_graph.node_metadata.copy()
                    saved_assigned = old_graph.mapping.assigned_node.copy()  # Save pre-assigned nodes
                    saved_mapping = old_graph.mapping.node_to_resource.copy()  # Save existing mappings
                    NodeGraph._instance = NodeGraph(seed=seed)
                    NodeGraph.get().connections = saved_connections
                    NodeGraph.get().nodes = saved_nodes
                    NodeGraph.get().node_metadata = saved_metadata
                    NodeGraph.get().mapping.assigned_node = saved_assigned  # Restore pre-assigned
                    NodeGraph.get().mapping.node_to_resource = saved_mapping  # Restore mappings
                NodeGraph.get().allocate_resources(only_connected_nodes=True)
            
            # Perform heuristic search and get the cost of the best mapping found
            # Pass seed to search for reproducibility
            mapping_cost = NodeGraph.get().heuristic_search_mapping(max_iterations=heuristic_iterations, seed=seed)
            
            # Check if mapping is valid (cost == 0 means no constraint violations)
            if mapping_cost == 0:
                print("[✓] Mapping successful with zero violations")
                mapping_success = True
                break
            elif attempt < heuristic_restarts - 1:
                # More retries available - continue loop
                print(f"[→] Found violations (penalty: {mapping_cost:.2f}), retrying...")
                continue
            else:
                # Last attempt - accept the mapping even with violations
                print(f"[⚠] Max retries reached. Accepting mapping with penalty: {mapping_cost:.2f}")
                mapping_success = True
                break
    
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
    
    # Get fixed-position modules from module list
    buffer_modules = [m for m in modules if isinstance(m, BufferConfig)]
    neighbor_module = next((m for m in modules if isinstance(m, NeighborStreamConfig)), None)
    special_module = next((m for m in modules if isinstance(m, SpecialArrayConfig)), None)
    ga_inport_modules = [m for m in modules if isinstance(m, GAInportConfig)]
    ga_outport_module = next((m for m in modules if isinstance(m, GAOutportConfig)), None)
    ga_pe_modules = [m for m in modules if isinstance(m, GAPEConfig)]
    
    # Physical resource layout: (module_id, resource_name_pattern, count, getter_func)
    # Updated counts to match new architecture: LC=20, ROW_LC=5, COL_LC=5, PE=10, READ=4, WRITE=1
    layout = [
        # IGA resources
        (ModuleID.IGA_LC, "LC", 20, lambda i: mapper.get_module(f"LC{i}")),  # 20 LC resources (2 rows of 10)
        # ROW_LC: Accessed as ROW_LC0-4
        (ModuleID.IGA_ROW_LC, "ROW_LC", 5, lambda i: mapper.get_module(f"ROW_LC{i}")),
        # COL_LC: Accessed as COL_LC0-4
        (ModuleID.IGA_COL_LC, "COL_LC", 5, lambda i: mapper.get_module(f"COL_LC{i}")),
        (ModuleID.IGA_PE, "PE", 10, lambda i: mapper.get_module(f"PE{i}")),
        # Stream Engine resources
        (ModuleID.SE_RD_MSE, "READ_STREAM", 4, lambda i: mapper.get_module(f"READ_STREAM{i}")),
        (ModuleID.SE_WR_MSE, "WRITE_STREAM", 1, lambda i: mapper.get_module(f"WRITE_STREAM{i}")),
        (ModuleID.SE_NSE, "NEIGHBOR", 1, lambda i: neighbor_module),
        (ModuleID.SE_NSE, "NEIGHBOR_EMPTY", 1, lambda i: None),  # Extra empty entry
        # Buffer manager cluster
        (ModuleID.BUFFER_MANAGER_CLUSTER, "BUFFER", len(buffer_modules), lambda i: buffer_modules[i] if i < len(buffer_modules) else None),
        # Special array
        (ModuleID.SPECIAL_ARRAY, "SPECIAL", 1, lambda i: special_module),
        # General array
        (ModuleID.GA_INPORT_GROUP, "GA_INPORT", len(ga_inport_modules), lambda i: ga_inport_modules[i] if i < len(ga_inport_modules) else None),
        (ModuleID.GA_OUTPORT_GROUP, "GA_OUTPORT", 1, lambda i: ga_outport_module if i == 0 else None),
        (ModuleID.GENERAL_PE, "GA_PE", len(ga_pe_modules), lambda i: ga_pe_modules[i] if i < len(ga_pe_modules) else None),
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
            for chunk in split_config(config, mid):
                bitstream += '1' + chunk
    
    # Pad to 64-bit boundary
    bitstream += '0' * ((64 - len(bitstream) % 64) % 64)
    return bitstream

def write_bitstream(entries, config_mask, output_file='./data/parsed_bitstream.txt', binary_output_file=None):
    """
    Write bitstream in human-readable format (to output_file).
    Groups entries by module type and shows each configuration.
    Uses same chunking logic as generate_bitstream.
    
    For each module type, ensures all entries have consistent bitstream length
    by padding empty entries with '0' lines to match the maximum length of non-empty entries.
    
    If binary_output_file is provided, also writes concatenated binary data (without module headers)
    to that file as 64-bit lines.
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
        ModuleID.GENERAL_PE: "ga_pe",
    }
    
    # Group entries by module type
    module_groups = defaultdict(list)
    for mid, config in entries:
        module_groups[mid].append(config)
    
    # Helper function to get output lines for a single config entry
    def get_config_output_lines(config, module_id):
        """Returns list of output lines for a config entry."""
        if not config or set(config) == {'0'}:
            return ["0"]
        else:
            lines = []
            for chunk in split_config(config, module_id):
                lines.append(f"1 {chunk}")
            return lines
    
    # Collect all binary data if binary output is requested
    binary_data = []
    binary_data.append(''.join(str(bit) for bit in config_mask))  # Start with config mask
    
    # Write to file
    with open(output_file, 'w') as f:
        # Process each module type in order
        for mid in sorted(module_groups.keys()):
            # Skip module if config_mask is 0
            if not config_mask[MODULE_ID_TO_MASK[mid]]:
                continue
            
            configs = module_groups[mid]
            module_name = module_names.get(mid, f"module_{mid}")
            
            # Calculate max number of output lines for this module type
            max_lines = 0
            for config in configs:
                lines = get_config_output_lines(config, mid)
                max_lines = max(max_lines, len(lines))
                
            print(f"Processing {module_name} with {len(configs)} entries, max lines: {max_lines}, config bits: {max(len(c) for c in configs if c)}")
            
            # Write module header
            f.write(f"{module_name}:\n")
            
            # Write each configuration entry with padding
            for config in configs:
                lines = get_config_output_lines(config, mid)
                
                # Write the actual bitstream lines
                for line in lines:
                    f.write(f"{line}\n")
                    # Collect binary data
                    if binary_output_file is not None:
                        binary_data.append(line)
                
                # Pad with '0' lines to reach max_lines
                padding_count = max_lines - len(lines)
                for _ in range(padding_count):
                    f.write("0\n")
                    if binary_output_file is not None:
                        binary_data.append("0")
            
            # Add blank line after each module group
            f.write("\n")
    
    print(f'Bitstream written to {output_file}')
    
    # Write binary output if requested
    if binary_output_file:
        # Extract just the binary values (without '1 ' prefix for non-zero lines)
        binary_string = ''
        for line in binary_data:
            if line == '0':
                binary_string += '0'
            elif line.startswith('1 '):
                binary_string += '1' + line[2:]
            else:
                binary_string += line
        
        # Pad to 64-bit boundary
        binary_string += '0' * ((64 - len(binary_string) % 64) % 64)
        
        with open(binary_output_file, 'w') as f:
            for i in range(0, len(binary_string), 64):
                chunk = binary_string[i:i+64]
                f.write(chunk + '\n')
        
        print(f'Binary dump written to {binary_output_file}')

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
                chunks = split_config(config, mid)
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
