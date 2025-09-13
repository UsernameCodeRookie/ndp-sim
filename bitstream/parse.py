import json
from math import ceil
import matplotlib
from matplotlib.patches import Patch
import matplotlib.pyplot as plt
from typing import List
from bitstream.bit import Bit
from bitstream.config import (
    BaseConfigModule,
    NeighborStreamConfig,
    StreamConfig,
    BufferConfig,
    SpecialArrayConfig,
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

def visualize_modules(modules, row_bit_width=32, save_path="./data/bitstream.png"):
    """
    Visualize bitstream of multiple config modules in one contiguous address space.

    Each row represents `row_bit_width` bits. Fields are drawn as rectangles
    proportional to their bit width. If a field does not fit in the current row,
    it wraps to the next row. Binary value is above the rectangle, field name is
    horizontal inside the rectangle. Each module is colored differently, and a legend
    is added in the lower-right corner.

    Args:
        modules (list): List of BaseConfigModule or derived instances
        row_bit_width (int): Number of bits per row
        save_path (str): Path to save the PNG
    """
    cmap = matplotlib.colormaps['tab10']
    module_colors = [cmap(i % 10) for i in range(len(modules))]

    all_bits = []
    all_fields = []
    all_colors = []

    for m_idx, module in enumerate(modules):
        bits = module.to_bits()
        for bit, field_info in zip(bits, module.FIELD_MAP):
            if len(field_info) == 2:
                name, width = field_info
            elif len(field_info) == 3:
                name, width, _ = field_info
            else:
                raise ValueError(f"Unexpected FIELD_MAP entry: {field_info}")

            for w in range(width-1, -1, -1):
                all_bits.append(bit[w])
                all_fields.append(name if w == width-1 else "")
                all_colors.append(module_colors[m_idx])

    total_bits = len(all_bits)
    n_rows = ceil(total_bits / row_bit_width)

    plt.figure(figsize=(max(12, row_bit_width / 4), 2 * n_rows))
    ax = plt.gca()

    bit_idx = 0
    for row in range(n_rows):
        remaining_in_row = row_bit_width
        rect_x = 0
        rect_y = n_rows - row - 1

        while remaining_in_row > 0 and bit_idx < total_bits:
            if bit_idx >= total_bits:
                break

            field_name = all_fields[bit_idx]

            # Count consecutive bits of the same field within the module
            field_width = 1
            while (bit_idx + field_width < total_bits and 
                   all_fields[bit_idx + field_width] == "" and
                   all_colors[bit_idx + field_width] == all_colors[bit_idx]):
                field_width += 1

            draw_width = min(field_width, remaining_in_row)

            # Draw rectangle
            bin_val = "".join(str(all_bits[bit_idx + i]) for i in range(draw_width))
            ax.add_patch(plt.Rectangle(
                (rect_x, rect_y), draw_width, 0.8,
                edgecolor='black', facecolor=all_colors[bit_idx]
            ))

            # Binary value above
            ax.text(rect_x + draw_width/2, rect_y + 0.65, bin_val,
                    ha='center', va='bottom', fontsize=6, color='black')

            # Field name horizontal inside
            if field_name and draw_width > 0:
                ax.text(rect_x + draw_width/2, rect_y + 0.35, field_name,
                        ha='center', va='center', fontsize=6, color='white')

            rect_x += draw_width
            bit_idx += draw_width
            remaining_in_row -= draw_width

    # Legend: module name -> color, placed in lower-right corner
    legend_handles = [Patch(facecolor=module_colors[i], edgecolor='black', label=modules[i].__class__.__name__)
                      for i in range(len(modules))]
    ax.legend(handles=legend_handles, loc='lower right', fontsize=8)

    ax.set_xlim(0, row_bit_width)
    ax.set_ylim(0, n_rows)
    ax.set_xticks([])
    ax.set_yticks([])
    ax.set_title("Bitstream Visualization (Module-Colored)")
    plt.tight_layout()
    plt.savefig(save_path, dpi=150)
    print(f"Bitstream visualization saved to {save_path}")
    
if __name__ == "__main__":
    # Load JSON configuration
    with open("./data/new_gemm_config_ringbroadcast.json", "r") as f:
        cfg = json.load(f)

    # Initialize modules
    modules = (
        [NeighborStreamConfig()] +
        # [StreamConfig(i) for i in range(3)] +
        [BufferConfig(i) for i in range(6)] +
        [SpecialArrayConfig()]
    )

    total_bits = []
    print("Configuration Bitstream:")
    # Process each module
    for m in modules:
        m.from_json(cfg)
        bits = m.to_bits()
        print(f"{m.__class__.__name__}: {[str(b) for b in bits]}")
        total_bits.extend(bits)
        
    print(f"Total bits: {len(total_bits)}")
    # Write to binary file
    bin = bits_to_binfile(total_bits, byteorder="little")
    
    with open("./data/config_bitstream.bin", "wb") as f:
        f.write(bin)
        
    # Visualize the configuration
    visualize_modules(modules, save_path="./data/bitstream.png")
    
