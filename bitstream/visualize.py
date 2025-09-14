from math import ceil
import matplotlib
from matplotlib.patches import Patch
import matplotlib.pyplot as plt

def visualize_modules(modules, row_bit_width=128, save_path="./data/bitstream.png"):
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

    def collect_bits(module, color):
        """Recursively collect bits and field names from a module."""
        # If the module is a combination module with submodules
        if hasattr(module, "submodules"):
            for sub in module.submodules:
                collect_bits(sub, color)
        elif hasattr(module, "FIELD_MAP"):
            bits = module.to_bits()
            for bit, field_info in zip(bits, module.FIELD_MAP):
                if len(field_info) == 2:
                    name, width = field_info
                elif len(field_info) == 3:
                    name, width, _ = field_info
                else:
                    raise ValueError(f"Unexpected FIELD_MAP entry: {field_info}")

                # Expand bits to match width, pad with 0 if needed
                for w in range(width - 1, -1, -1):
                    if w < len(bit):
                        all_bits.append(bit[w])
                    else:
                        all_bits.append(0)
                    all_fields.append(name if w == width - 1 else "")
                    all_colors.append(color)

    for m_idx, module in enumerate(modules):
        collect_bits(module, module_colors[m_idx])

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

            # Address range in bytes
            start_bit = bit_idx
            end_bit = bit_idx + draw_width - 1
            start_byte = start_bit // 8
            end_byte = end_bit // 8
            addr_str = f"[0x{start_byte:04X} - 0x{end_byte:04X}]"

            # Binary value + address above
            ax.text(rect_x + draw_width/2, rect_y + 0.65,
                    f"{addr_str}\n{bin_val}",
                    ha='center', va='bottom', fontsize=6, color='black')

            # Field name inside
            if field_name and draw_width > 0:
                ax.text(rect_x + draw_width/2, rect_y + 0.35, field_name,
                        ha='center', va='center', fontsize=6, color='white')

            rect_x += draw_width
            bit_idx += draw_width
            remaining_in_row -= draw_width

    # Legend: module name -> color, placed in lower-right corner
    legend_handles = [
        Patch(facecolor=module_colors[i], edgecolor='black', label=modules[i].__class__.__name__)
        for i in range(len(modules))
    ]
    ax.legend(handles=legend_handles, loc='lower right', fontsize=8)

    ax.set_xlim(0, row_bit_width)
    ax.set_ylim(0, n_rows)
    ax.set_xticks([])
    ax.set_yticks([])
    ax.set_title("Bitstream Visualization (Module-Colored)")
    plt.tight_layout()
    plt.savefig(save_path, dpi=150)
    print(f"Bitstream visualization saved to {save_path}")
