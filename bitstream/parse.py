import json
from typing import List
from bitstream.bit import Bit
from bitstream.visualize import visualize_modules
from bitstream.config import (
    DramLoopControlConfig,
    LCPEConfig,
    BufferLoopControlGroupConfig,
    NeighborStreamConfig,
    StreamConfig,
    BufferConfig,
    SpecialArrayConfig,
    NodeGraph,
)

def bits_to_binfile(bits_list: List[Bit], byteorder: str = "little"):
    """
    Write a list of Bit objects to a binary file using each Bit's to_bytes() method.

    Args:
        bits_list: List of Bit objects in order (MSB to LSB for each Bit).
        byteorder: Byte order for writing bytes ('little' or 'big').

    Each Bit object is converted to bytes using its own `to_bytes` method. 
    Bits are concatenated in the given order, and padding is applied to align to full bytes.
    """
    # Step 1: Flatten all bits into a single Bit object by concatenation
    combined = None
    for b in bits_list:
        if combined is None:
            combined = b
        else:
            combined = combined.concat(b)  # concatenate using Bit.concat()

    if combined is None:
        # No bits to write
        combined = Bit(0, 0)
        
    bin = combined.to_bytes(byteorder=byteorder)

    return bin
    
if __name__ == "__main__":
    # Load JSON configuration - use aligned config for testing
    config_file = "./data/gemm_config_aligned_with_config.json"
    with open(config_file, "r") as f:
        cfg = json.load(f)

    # Initialize modules
    modules = (
        [DramLoopControlConfig(i) for i in range(8)] +
        [BufferLoopControlGroupConfig(i) for i in range(4)] +
        [LCPEConfig(i) for i in range(4)] +
        [StreamConfig(i) for i in range(4)] +
        [NeighborStreamConfig()] +
        [BufferConfig(i) for i in range(6)] +
        [SpecialArrayConfig()]
    )

    total_bits = []
    print("Configuration Bitstream:")
    # Process each module - first load all JSON to register all nodes
    for m in modules:
        m.from_json(cfg)
    
    # Now allocate resources and resolve node indices BEFORE generating bits
    from bitstream.index import NodeIndex
    node_graph = NodeGraph.get()
    node_graph.allocate_resources()
    node_graph.search_mapping()
    # Resolve all logical node indices to physical resource IDs ahead of encoding
    NodeIndex.resolve_all()
    
    # Separate modules by type for physical-order output
    dram_lc_modules = [m for m in modules if isinstance(m, DramLoopControlConfig)]
    buffer_lc_modules = [m for m in modules if isinstance(m, BufferLoopControlGroupConfig)]
    lc_pe_modules = [m for m in modules if isinstance(m, LCPEConfig)]
    stream_modules = [m for m in modules if isinstance(m, StreamConfig)]
    other_modules = [m for m in modules if not isinstance(m, (DramLoopControlConfig, BufferLoopControlGroupConfig, LCPEConfig, StreamConfig))]
    
    # Sort DRAM LC modules by physical_index
    dram_lc_modules.sort(key=lambda m: m.physical_index if hasattr(m, 'physical_index') and m.physical_index is not None else 999)
    
    # Reconstruct modules in physical order
    ordered_modules = dram_lc_modules + buffer_lc_modules + lc_pe_modules + stream_modules + other_modules
    
    # Now generate bits with resolved node IDs in physical order
    for m in ordered_modules:
        bits = m.to_bits()
        total_bits.extend(bits)
        
        # Dump field values vs binary
        m.dump(indent=2)
        
    # Write to binary file
    bin = bits_to_binfile(total_bits, byteorder="little")
    
    with open("./data/config_bitstream.bin", "wb") as f:
        f.write(bin)
        
    # Visualize the configuration
    # visualize_modules(modules, save_path="./data/bitstream.png")
    
    # Print NodeGraph summary
    # NodeGraph.get().summary()
    
    
    
