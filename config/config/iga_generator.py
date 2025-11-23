import sys

# class Tag:
#     def __init__(self, port_valid_bit, port_last_bit, port_same_bit, port_last_index):
#         self.port_valid_bit = port_valid_bit
#         self.port_last_bit = port_last_bit
#         self.port_same_bit = port_same_bit
#         self.port_last_index = port_last_index

# class IGAConfig:
#     def __init__(self, data, tag):
#         self.data = data
#         self.tag = tag

# constants
# `define PORT_VALID_BIT                     1
# `define PORT_BRANCH_BIT                    1
# `define PORT_LAST_BIT                      1
# `define PORT_SAME_BIT                      1
# `define PORT_LAST_INDEX                    3
PORT_VALID_BIT = 1
PORT_LAST_BIT = 1
PORT_SAME_BIT = 1
PORT_LAST_INDEX = 3

# `define IGA_LC_PORT_TAG_WIDTH              (`PORT_VALID_BIT + `PORT_LAST_BIT + `PORT_SAME_BIT + `PORT_LAST_INDEX) 
# `define IGA_LC_PORT_DATA_WIDTH             12
# `define IGA_LC_PORT_WIDTH                  (`IGA_LC_PORT_TAG_WIDTH + `IGA_LC_PORT_DATA_WIDTH)
IGA_LC_PORT_TAG_WIDTH = PORT_VALID_BIT + PORT_LAST_BIT + PORT_SAME_BIT + PORT_LAST_INDEX
IGA_LC_PORT_DATA_WIDTH = 12
IGA_LC_PORT_WIDTH = IGA_LC_PORT_TAG_WIDTH + IGA_LC_PORT_DATA_WIDTH


# `define IGA_ROW_LC_PORT_TAG_WIDTH          (`PORT_VALID_BIT + `PORT_LAST_BIT + `PORT_SAME_BIT + `PORT_LAST_INDEX) 
# `define IGA_ROW_LC_PORT_DATA_WIDTH         13
# `define IGA_ROW_LC_PORT_WIDTH              (`IGA_ROW_LC_PORT_TAG_WIDTH + `IGA_ROW_LC_PORT_DATA_WIDTH)
IGA_ROW_LC_PORT_TAG_WIDTH = PORT_VALID_BIT + PORT_LAST_BIT + PORT_SAME_BIT + PORT_LAST_INDEX
IGA_ROW_LC_PORT_DATA_WIDTH = 13
IGA_ROW_LC_PORT_WIDTH = IGA_ROW_LC_PORT_TAG_WIDTH + IGA_ROW_LC_PORT_DATA_WIDTH


# `define IGA_COL_LC_PORT_TAG_WIDTH          (`PORT_VALID_BIT + `PORT_LAST_BIT + `PORT_SAME_BIT + `PORT_LAST_INDEX) 
# `define IGA_COL_LC_PORT_DATA_WIDTH         6
# `define IGA_COL_LC_PORT_WIDTH              (`IGA_COL_LC_PORT_TAG_WIDTH + `IGA_COL_LC_PORT_DATA_WIDTH)
IGA_COL_LC_PORT_TAG_WIDTH = PORT_VALID_BIT + PORT_LAST_BIT + PORT_SAME_BIT + PORT_LAST_INDEX
IGA_COL_LC_PORT_DATA_WIDTH = 6
IGA_COL_LC_PORT_WIDTH = IGA_COL_LC_PORT_TAG_WIDTH + IGA_COL_LC_PORT_DATA_WIDTH


def config2bits(value, bit_width):
    if value >= (1 << bit_width):
        raise ValueError(f"Value {value} exceeds the bit width of {bit_width}")
    result = bin(value)[2:]  # Convert to binary and remove '0b' prefix
    result = result.zfill(bit_width)  # Pad with leading zeros
    return result

def get_configstream(configs):
    result = ''
    for value, width in configs:
        result += config2bits(value, width)
    
    return result


class IndexGeneratorArray:
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
        # self.valid = 0
        # self.last = 0
        # self.same = 0
        # self.last_index = last_index
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

    def _get_info(self):
        self.info = IGAConfig(data=self.cur_idx, tag=Tag)

iga = [IndexGeneratorArray(id=idx) for idx in range(6)]
buf_lc = [IndexGeneratorArray(id=idx) for idx in range(2)]

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

    def get_tag(self, lcs):
        self.tag["port_valid_bit"] = 1
        self.tag["port_last_bit"] = 1 if any(lc.tag["port_last_bit"] == 1 for lc in lcs) else 0
        self.tag["port_same_bit"] = 0 if any(lc.tag["port_same_bit"] == 0 for lc in lcs) else 1
        self.tag["port_last_index"] = max(lc.tag["port_last_index"] for lc in lcs)

    def compute(self, *args):
        if self.op is None:
            raise RuntimeError("op not set")
        val = self.op(*args)  # 约定签名: (p0,p1,p2,cfg)->result
        self.outport = val

pea = [PEArray(id=idx) for idx in range(6)]

# for c_in in range(64//4):
#     for w_k in range(3):
#         for h_k in range(3):
#             # idx0 = w_k + w * 1
#             idx0 = w_k
#             # idx1 = h_k + h * 32
#             idx1 = h_k
#             idx2 = c_in * 4


def merge_lc():
    pass

def update_last_index(iga, idx):
    if idx < 0 or (iga[idx].cur_idx + iga[idx].cur_idx <= iga[idx].end and iga[idx].tag["port_valid_bit"] == 1):
        return
    if iga[idx].tag["port_valid_bit"] == 0:
        update_last_index(iga, idx - 1)  
    if iga[idx].cur_idx + iga[idx].cur_idx > iga[idx].end and iga[idx].tag["port_valid_bit"] == 1:
        for id in range(idx, len(iga)):
            iga[id].tag["port_last_index"] = iga[idx].tag["port_last_index"]
        update_last_index(iga, idx - 1)

def buf_iga_gene():
    # init lc
    for lc in buf_lc:
        lc.reset()
    # config lc
    buf_lc[0].tag["port_valid_bit"] = 1
    buf_lc[0].tag["port_last_bit"] = 0
    buf_lc[0].tag["port_same_bit"] = 1
    buf_lc[0].tag["port_last_index"] = 3
    buf_lc[0].start = 0
    buf_lc[0].end = 4-1
    buf_lc[0].step = 1
    buf_lc[1].tag["port_valid_bit"] = 1
    buf_lc[1].tag["port_last_bit"] = 0
    buf_lc[1].tag["port_same_bit"] = 0
    buf_lc[1].tag["port_last_index"] = 4
    buf_lc[1].start = 0
    buf_lc[1].end = 32-1
    buf_lc[1].step = 16
    # connect
    buf_lc[0].next = buf_lc[1]
    buf_lc[1].pre = buf_lc[0]




    while buf_lc[0].cur_idx <= buf_lc[0].end + 1:
        # print
        # print(f"lc0: {buf_lc[0].cur_idx}, lc1: {buf_lc[1].cur_idx}")
        iga_config = ""

        # for ig in buf_lc:
        #     # print(f"IndexGeneratorArray id: {ig.id}, data: {ig.cur_idx}, tag: {ig.tag}")
        #     single_config=get_configstream([(ig.cur_idx, IGA_COL_LC_PORT_DATA_WIDTH),
        #                       (ig.tag["port_valid_bit"], PORT_VALID_BIT),
        #                       (ig.tag["port_last_bit"], PORT_LAST_BIT),
        #                       (ig.tag["port_same_bit"], PORT_SAME_BIT),
        #                       (ig.tag["port_last_index"], PORT_LAST_INDEX)])
        #     iga_config += single_config
            # print(single_config)
        col_config=get_configstream([
                        # (buf_lc[0].cur_idx, IGA_COL_LC_PORT_DATA_WIDTH),
                        (buf_lc[0].tag["port_valid_bit"], PORT_VALID_BIT),
                        (buf_lc[0].tag["port_last_bit"], PORT_LAST_BIT),
                        (buf_lc[0].tag["port_same_bit"], PORT_SAME_BIT),
                        (buf_lc[0].tag["port_last_index"], PORT_LAST_INDEX),
                        (buf_lc[0].cur_idx, IGA_COL_LC_PORT_DATA_WIDTH),
                    ])
        
        row_config=get_configstream([
                        # (buf_lc[1].cur_idx, IGA_ROW_LC_PORT_DATA_WIDTH),
                        (buf_lc[1].tag["port_valid_bit"], PORT_VALID_BIT),
                        (buf_lc[1].tag["port_last_bit"], PORT_LAST_BIT),
                        (buf_lc[1].tag["port_same_bit"], PORT_SAME_BIT),
                        (buf_lc[1].tag["port_last_index"], PORT_LAST_INDEX),
                        (buf_lc[1].cur_idx, IGA_ROW_LC_PORT_DATA_WIDTH),
                    ])
        

        with open("./results/buf_iga_col_config.txt", "a") as f:
            f.write(col_config + "\n")
        with open("./results/buf_iga_row_config.txt", "a") as f:
            f.write(row_config + "\n")

        # backtrack
        buf_lc[0].same = 1
        for ig in buf_lc:
            ig.tag["port_last_bit"] = 0
        buf_lc[1].tag["port_last_index"] = 1
        buf_lc[0].tag["port_last_index"] = 0

        # update
        buf_lc[1].cur_idx += buf_lc[1].step
        if buf_lc[1].cur_idx > buf_lc[1].end:
            buf_lc[1].cur_idx = buf_lc[1].start
            buf_lc[0].cur_idx += buf_lc[0].step
            buf_lc[0].same = 0
            if buf_lc[0].cur_idx > buf_lc[0].end:
                break
        
        for ig in buf_lc:
            if ig.cur_idx + ig.step > ig.end and ig.tag["port_valid_bit"] == 1:
                ig.tag["port_last_bit"] = 1
        
        if buf_lc[0].cur_idx + buf_lc[0].step > buf_lc[0].end:
            buf_lc[1].tag["port_last_index"] = 0

    
        

        # last_index update
        total_lc = iga[0:3] + buf_lc[:]
        for lc in total_lc:
            pass
            



def iga_gene():
    # config lc
    # lc0 = IndexGeneratorArray(0, 0, 15, 1)
    # iga[0].valid = 1
    # iga[0].last_index = 0
    # iga[0].same = 1
    iga[0].tag["port_valid_bit"] = 1
    iga[0].tag["port_last_bit"] = 0
    iga[0].tag["port_same_bit"] = 1
    iga[0].tag["port_last_index"] = 0
    iga[0].start = 0
    iga[0].end = 15
    iga[0].step = 1
    # lc1 = IndexGeneratorArray(1, 0, 2, 1)
    # iga[1].valid = 1
    # iga[1].last_index = 1
    # iga[1].same = 1
    iga[1].tag["port_valid_bit"] = 1
    iga[1].tag["port_last_bit"] = 0
    iga[1].tag["port_same_bit"] = 1
    iga[1].tag["port_last_index"] = 1
    iga[1].start = 0
    iga[1].end = 2
    iga[1].step = 1
    # lc2 = IndexGeneratorArray(2, 0, 2, 1)
    # iga[2].valid = 1
    # iga[2].last_index = 2
    # iga[2].last = 1
    iga[2].tag["port_valid_bit"] = 1
    iga[2].tag["port_last_bit"] = 0
    iga[2].tag["port_same_bit"] = 0
    iga[2].tag["port_last_index"] = 2
    iga[2].start = 0
    iga[2].end = 2
    iga[2].step = 1
    # connect
    iga[0].next = iga[1]
    iga[1].pre = iga[0]
    iga[1].next = iga[2]
    iga[2].pre = iga[1]

    # pe array
    # pea[0].tag["port_valid_bit"] = 1
    # pea[0].tag["port_last_bit"] = 0
    # pea[0].tag["port_same_bit"] = 0
    # pea[0].tag["port_last_index"] = 2
    pea[0].op = lambda p0: p0*4
    pea[0].get_tag([iga[0]])
    pea[0].tag = iga[0].tag.copy()

    while iga[0].cur_idx <= iga[0].end + 1:
        # update buffer lc
        buf_iga_gene()
        # buf_iga_gene()
        # print
        # print(f"lc0: {iga[0].cur_idx}, lc1: {iga[1].cur_idx}, lc2: {iga[2].cur_idx}")
        # iga_config = ""
        for ig in iga:
            # print(f"IndexGeneratorArray id: {ig.id}, data: {ig.cur_idx}, tag: {ig.tag}")
            # single_config=get_configstream([
            #                   (ig.tag["port_valid_bit"], PORT_VALID_BIT),
            #                   (ig.tag["port_last_bit"], PORT_LAST_BIT),
            #                   (ig.tag["port_same_bit"], PORT_SAME_BIT),
            #                   (ig.tag["port_last_index"], PORT_LAST_INDEX),
            #                   (ig.cur_idx, IGA_COL_LC_PORT_DATA_WIDTH),
            #                 ])
            # iga_config += single_config
            # print(single_config)
            ig.bitstream = get_configstream([
                            (ig.tag["port_valid_bit"], PORT_VALID_BIT),
                            (ig.tag["port_last_bit"], PORT_LAST_BIT),
                            (ig.tag["port_same_bit"], PORT_SAME_BIT),
                            (ig.tag["port_last_index"], PORT_LAST_INDEX),
                            (ig.cur_idx, IGA_LC_PORT_DATA_WIDTH),
            ])
        

        for pe in pea:
            # print(f"PEArray id: {pe.id}, outport: {pe.outport}, tag: {pe.tag}")
            # single_config=get_configstream([
            #                 #   (pe.outport, IGA_COL_LC_PORT_DATA_WIDTH),
            #                   (pe.tag["port_valid_bit"], PORT_VALID_BIT),
            #                   (pe.tag["port_last_bit"], PORT_LAST_BIT),
            #                   (pe.tag["port_same_bit"], PORT_SAME_BIT),
            #                   (pe.tag["port_last_index"], PORT_LAST_INDEX),
            #                   (pe.outport, IGA_COL_LC_PORT_DATA_WIDTH),
            #                 ])
            # iga_config += single_config
            pe.bitstream = get_configstream([
                            (pe.tag["port_valid_bit"], PORT_VALID_BIT),
                            (pe.tag["port_last_bit"], PORT_LAST_BIT),
                            (pe.tag["port_same_bit"], PORT_SAME_BIT),
                            (pe.tag["port_last_index"], PORT_LAST_INDEX),
                            (pe.outport, IGA_LC_PORT_DATA_WIDTH),
            ])
            # print(single_config)
        
        # iga_config = "".join([pe.bitstream for pe in reversed(pea)]+[ig.bitstream for ig in reversed(iga)])
        iga_config = "".join([ig.bitstream for ig in iga]+[pe.bitstream for pe in pea])

        with open("./results/iga_config.txt", "a") as f:
            f.write(iga_config + "\n")
        # print(iga_config)
        # print(f"{int(iga_config, 2):X}")


        # backtrack
        iga[1].same = 1
        iga[0].same = 1
        for pe in pea:
            pe.tag["port_last_bit"] = 0       
        for ig in iga:
            ig.tag["port_last_bit"] = 0

        iga[2].tag["port_last_index"] = 2
        iga[1].tag["port_last_index"] = 1
        iga[0].tag["port_last_index"] = 0

        # update
        iga[2].cur_idx += iga[2].step
        if iga[2].cur_idx > iga[2].end:
            iga[2].cur_idx = iga[2].start
            iga[1].cur_idx += iga[1].step
            iga[1].same = 0
            if iga[1].cur_idx > iga[1].end:
                iga[1].cur_idx = iga[1].start
                iga[0].cur_idx += iga[0].step
                iga[0].same = 0
                if iga[0].cur_idx > iga[0].end:
                    break

        # compute pe
        for pe in pea:
            if pe.op is not None:
                pe.compute(iga[0].cur_idx)

        for ig in iga:
            if ig.cur_idx == ig.end and ig.tag["port_valid_bit"] == 1:
                ig.tag["port_last_bit"] = 1
        
        if iga[1].cur_idx == iga[1].end:
            iga[2].tag["port_last_index"] = 1
        
        if iga[0].cur_idx == iga[0].end and iga[1].cur_idx == iga[1].end:
            iga[2].tag["port_last_index"] = 0
            iga[1].tag["port_last_index"] = 0




        pea[0].tag = iga[0].tag.copy()

        # backtrack
        # iga[1].same = 1
        # iga[0].same = 1


        # # update buffer lc
        # buf_iga_gene()


def clear_files():
    with open("./results/iga_config.txt", "w") as f:
        f.write("")
    with open("./results/buf_iga_col_config.txt", "w") as f:
        f.write("")
    with open("./results/buf_iga_row_config.txt", "w") as f:
        f.write("")

if __name__ == "__main__":
    clear_files()
    iga_gene()
    # buf_iga_gene()