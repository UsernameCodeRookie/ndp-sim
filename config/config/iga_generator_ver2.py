import sys
import os
from config.config_parameters import *


MEM_LC_NUM = 6
BUF_LC_NUM = 2
TOTAL_LC_NUM = MEM_LC_NUM + BUF_LC_NUM
IGA_PE_NUM = 6


PORT_VALID_BIT = 1
PORT_LAST_BIT = 1
PORT_SAME_BIT = 1
PORT_LAST_INDEX = 3

IGA_LC_PORT_TAG_WIDTH = PORT_VALID_BIT + PORT_LAST_BIT + PORT_SAME_BIT + PORT_LAST_INDEX
IGA_LC_PORT_DATA_WIDTH = 12
IGA_LC_PORT_WIDTH = IGA_LC_PORT_TAG_WIDTH + IGA_LC_PORT_DATA_WIDTH

def config2bits(value, bit_width):
    if value >= (1 << bit_width) or value < 0:
        raise ValueError(f"Value {value} exceeds the bit width of {bit_width}")
    result = bin(value)[2:]  # Convert to binary and remove '0b' prefix
    result = result.zfill(bit_width)  # Pad with leading zeros
    return result

def get_configstream(configs):
    result = ''
    for value, width in configs:
        result += config2bits(value, width)
    return result



class LoopControl:
    def __init__(self, id, start=0, end=0, last_index=0, step=1):
        self.cur_idx = 0
        self.pre = None
        self.next = None
        self.id = id
        self.start = start
        self.end = end
        self.step = step
        # tag information
        self.tag = {
            "port_valid_bit": 0,
            "port_last_bit": 0,
            "port_same_bit": 0,
            "port_last_index": last_index
        }
        # optional helper flags (some code used `.same` elsewhere)
        self.same = 1
        self.bitstream = None
    
    def reset(self):
        self.cur_idx = 0
        self.pre = None
        self.next = None
        self.start = 0
        self.end = 0
        self.step = 1
        self.tag = { 
            "port_valid_bit": 0,
            "port_last_bit": 0,
            "port_same_bit": 0,
            "port_last_index": 0
        }
        self.same = 1

    def _get_info(self):
        # placeholder - original referenced IGAConflc/Tag which are not defined here
        pass


# generate loop controls (global)
lc = [LoopControl(id=idx) for idx in range(TOTAL_LC_NUM)]  # lc[6] is row buffer lc, lc[7] is col buffer lc


class PEArray:
    def __init__(self, id, op = None):
        self.id = id
        self.outport = 0
        self.op = op
        self.tag = {
            "port_valid_bit": 0,
            "port_last_bit": 0,
            "port_same_bit": 0,
            "port_last_index": 0
        }
        self.bitstream = None

    def get_tag(self, lcs):
        self.tag["port_valid_bit"] = 1
        self.tag["port_last_bit"] = 1 if any(x.tag["port_last_bit"] == 1 for x in lcs) else 0
        # port_same_bit should be 1 only if all port_same_bit == 1, kept original logic but clearer:
        self.tag["port_same_bit"] = 1 if all(x.tag["port_same_bit"] == 1 for x in lcs) else 0
        self.tag["port_last_index"] = max(x.tag["port_last_index"] for x in lcs) if lcs else 0

    def compute(self, *args):
        if self.op is None:
            raise RuntimeError("op not set")
        val = self.op(*args)  # 约定签名: (p0,p1,p2,cfg)->result
        self.outport = val


pea = [PEArray(id=idx) for idx in range(IGA_PE_NUM)]


def update_last_index(lc_list, idx):
    # keep the same signature but renamed param to avoid confusion with global lc
    if idx < 0:
        return

    cur = lc_list[idx]
    # original code had suspicious "cur_idx + cur_idx" — keep simpler guard
    if cur.tag["port_valid_bit"] == 0:
        # find previous valid
        update_last_index(lc_list, idx - 1)
    elif cur.tag["port_last_bit"] == 1:
        # propagate last_index to following LCs
        for i in range(idx+1, len(lc_list)):
            lc_list[i].tag["port_last_index"] = cur.tag["port_last_index"]
        update_last_index(lc_list, idx - 1)
    else:
        update_last_index(lc_list, idx - 1)

# def update_last_index(c, idx):
#     # keep the same signature but renamed param to avoid confusion with global lc
#     if c.pre is None:
#         return 
    
#     if c.tag["port_last_bit"] == 1:
#         # propagate last_index to following LCs
#         n = c.next
#         while n is not None:
#             n.tag["port_last_index"] = c.tag["port_last_index"]
#             n = n.next
#         update_last_index(c.pre, idx - 1)
#     else:
#         update_last_index(c.pre, idx - 1)

def lc_gene():
    global lc  # explicitly use the global lc list
    # initialize some lc entries (consistent with your original intent)
    lc[0].tag["port_valid_bit"] = 1
    lc[0].tag["port_last_bit"] = 0
    lc[0].tag["port_same_bit"] = 0
    lc[0].tag["port_last_index"] = 0
    lc[0].start = 0
    lc[0].end = 15
    lc[0].step = 1

    lc[1].tag["port_valid_bit"] = 1
    lc[1].tag["port_last_bit"] = 0
    lc[1].tag["port_same_bit"] = 0
    lc[1].tag["port_last_index"] = 1
    lc[1].start = 0
    lc[1].end = 2
    lc[1].step = 1

    lc[2].tag["port_valid_bit"] = 1
    lc[2].tag["port_last_bit"] = 0
    lc[2].tag["port_same_bit"] = 0
    lc[2].tag["port_last_index"] = 2
    lc[2].start = 0
    lc[2].end = 2
    lc[2].step = 1

    # buffer lc
    lc[6+0].tag["port_valid_bit"] = 1
    lc[6+0].tag["port_last_bit"] = 0
    lc[6+0].tag["port_same_bit"] = 0
    lc[6+0].tag["port_last_index"] = 3
    lc[6+0].start = 0
    lc[6+0].end = 4 - 1
    lc[6+0].step = 1

    lc[6+1].tag["port_valid_bit"] = 1
    lc[6+1].tag["port_last_bit"] = 0
    lc[6+1].tag["port_same_bit"] = 0
    lc[6+1].tag["port_last_index"] = 4
    lc[6+1].start = 0
    lc[6+1].end = 32 - 1
    lc[6+1].step = 16

    # connect
    lc[0].next = lc[1]
    lc[1].pre = lc[0]
    lc[1].next = lc[2]
    lc[2].pre = lc[1]
    lc[2].next = lc[6+0]
    # lc[6] must be col buffer lc and lc[7] must be row buffer lc
    lc[6+1].pre = lc[2]
    lc[6+1].next = lc[6+0]
    lc[6+0].pre = lc[6+1]

    # pe array config
    pea[0].op = lambda p0: p0 * 4
    pea[0].get_tag([lc[0]])
    pea[0].tag = lc[0].tag.copy()

    # ensure results dir exists
    os.makedirs("./results", exist_ok=True)


    # main loop: iterate lc[0] from start to end (inclusive)
    while lc[0].cur_idx <= lc[0].end:

        # update buffer lc bitstreams for mem lc (first 6)
        for c in lc[:6]:
            c.bitstream = get_configstream([
                (c.tag["port_valid_bit"], PORT_VALID_BIT),
                (c.tag["port_last_bit"], PORT_LAST_BIT),
                (c.tag["port_same_bit"], PORT_SAME_BIT),
                (c.tag["port_last_index"], PORT_LAST_INDEX),
                (c.cur_idx, IGA_LC_PORT_DATA_WIDTH),
            ])

        # update pea bitstreams
        for pe in pea:
            pe.bitstream = get_configstream([
                (pe.tag["port_valid_bit"], PORT_VALID_BIT),
                (pe.tag["port_last_bit"], PORT_LAST_BIT),
                (pe.tag["port_same_bit"], PORT_SAME_BIT),
                (pe.tag["port_last_index"], PORT_LAST_INDEX),
                (pe.outport, IGA_LC_PORT_DATA_WIDTH),
            ])

        lc_config = "".join([c.bitstream for c in lc[:6]] + [p.bitstream for p in pea])

        with open("./results/lc_config.txt", "a") as f:
            f.write(lc_config + "\n")

        row_config = get_configstream([
            (lc[6+0].tag["port_valid_bit"], PORT_VALID_BIT),
            (lc[6+0].tag["port_last_bit"], PORT_LAST_BIT),
            (lc[6+0].tag["port_same_bit"], PORT_SAME_BIT),
            (lc[6+0].tag["port_last_index"], PORT_LAST_INDEX),
            (lc[6+0].cur_idx, SE_BUF_ROW_INPORT_IDX_WIDTH),
        ])

        col_config = get_configstream([
            (lc[6+1].tag["port_valid_bit"], PORT_VALID_BIT),
            (lc[6+1].tag["port_last_bit"], PORT_LAST_BIT),
            (lc[6+1].tag["port_same_bit"], PORT_SAME_BIT),
            (lc[6+1].tag["port_last_index"], PORT_LAST_INDEX),
            (lc[6+1].cur_idx, SE_BUF_COL_INPORT_IDX_WIDTH),
        ])

        with open("./results/buf_lc_col_config.txt", "a") as f:
            f.write(col_config + "\n")
        with open("./results/buf_lc_row_config.txt", "a") as f:
            f.write(row_config + "\n")

        # backtrack: reset last_bit and same_bit flags before update
        for pe in pea:
            pe.tag["port_last_bit"] = 0
        for c in lc:
            c.tag["port_last_bit"] = 0
        
        lc[0].tag["port_same_bit"] = 1 
        lc[1].tag["port_same_bit"] = 1 
        lc[2].tag["port_same_bit"] = 1
        lc[6].tag["port_same_bit"] = 1

        
        # restore last_index mapping (as in original)
        lc[2].tag["port_last_index"] = 2
        lc[1].tag["port_last_index"] = 1
        lc[0].tag["port_last_index"] = 0
        lc[6].tag["port_last_index"] = 3
        lc[7].tag["port_last_index"] = 4

        # update indices with nested loops semantics
        lc[7].cur_idx += lc[7].step
        if lc[7].cur_idx > lc[7].end:
            lc[7].cur_idx = lc[7].start
            lc[6].cur_idx += lc[6].step
            lc[6].tag["port_same_bit"] = 0
            if lc[6].cur_idx > lc[6].end:
                lc[6].cur_idx = lc[6].start
                lc[2].cur_idx += lc[2].step
                lc[2].tag["port_same_bit"] = 0
                if lc[2].cur_idx > lc[2].end:
                    lc[2].cur_idx = lc[2].start
                    lc[1].cur_idx += lc[1].step
                    lc[1].tag["port_same_bit"] = 0
                    if lc[1].cur_idx > lc[1].end:
                        lc[1].cur_idx = lc[1].start
                        lc[0].cur_idx += lc[0].step
                        lc[0].tag["port_same_bit"] = 0
                        if lc[0].cur_idx > lc[0].end:
                            break

        # compute pe
        for pe in pea:
            if pe.op is not None:
                pe.compute(lc[0].cur_idx)

        # set port_last_bit when lc reaches end
        for c in lc:
            if c.tag["port_valid_bit"] == 1 and c.cur_idx + c.step > c.end:
                c.tag["port_last_bit"] = 1

        update_last_index(lc, TOTAL_LC_NUM - 2)  # original used -1-1; keep -2 to match intent

        pea[0].tag = lc[0].tag.copy()


def clear_files():
    with open("./results/lc_config.txt", "w") as f:
        f.write("")
    with open("./results/buf_lc_col_config.txt", "w") as f:
        f.write("")
    with open("./results/buf_lc_row_config.txt", "w") as f:
        f.write("")


if __name__ == "__main__":
    clear_files()
    lc_gene()