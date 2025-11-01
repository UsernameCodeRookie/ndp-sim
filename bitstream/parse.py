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
    # Load JSON configuration
    with open("./data/gemm_config_ringbroadcast.json", "r") as f:
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
    # Process each module
    for m in modules:
        m.from_json(cfg)
        bits = m.to_bits()
        # print(f"{m.__class__.__name__}: {[str(b) for b in bits]}")
        total_bits.extend(bits)
        
        # Dump field values vs binary
        m.dump(indent=2)
        
    # Write to binary file
    bin = bits_to_binfile(total_bits, byteorder="little")
    
    with open("./data/config_bitstream.bin", "wb") as f:
        f.write(bin)
        
    # Visualize the configuration
    visualize_modules(modules, save_path="./data/bitstream.png")
    
    # Print NodeGraph summary
    NodeGraph.get().summary()
    
    
    
