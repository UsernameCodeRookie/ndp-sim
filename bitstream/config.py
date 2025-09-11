from abc import ABC, abstractmethod
from bitstream.bit import Bit

class ConfigModule(ABC):
    @abstractmethod
    def from_json(self, cfg: dict):
        pass
    
    @abstractmethod
    def to_bits(self) -> list[Bit]:
        pass
    
class NeighborStreamConfig(ConfigModule):
    def __init__(self):
        pass
    
    def from_json(self, cfg: dict):
        n2n = cfg.get("n2n", {})
        
        if n2n is None:
            raise NotImplementedError
        
        self.mem_loop = n2n.get("mem_loop", int)
        self.mode = n2n.get("mode", int)
        self.stream_id = n2n.get("stream_id", int)
        self.src_slice_sel = n2n.get("src_slice_sel", bool)
        self.dst_slice_sel = n2n.get("dst_slice_sel", bool)
        self.src_buf_idx = n2n.get("src_buf_idx", int)
        self.dst_buf_idx = n2n.get("dst_buf_idx", int)

    
    def to_bits(self) -> list[Bit]:
        return [
            Bit(self.mem_loop, 4),
            Bit(self.mode, 1),
            Bit(self.stream_id, 2),
            Bit(self.src_slice_sel, 1),
            Bit(self.dst_slice_sel, 1),
            Bit(self.src_buf_idx, 3),
            Bit(self.dst_buf_idx, 3)
        ]