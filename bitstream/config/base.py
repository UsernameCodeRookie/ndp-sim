from abc import ABC, abstractmethod
from bitstream.bit import Bit
from typing import List, Union, Tuple, Callable


class ConfigModule(ABC):
    """Abstract base class for all config modules."""

    @abstractmethod
    def from_json(self, cfg: dict):
        pass

    @abstractmethod
    def to_bits(self) -> list[Bit]:
        pass


class BaseConfigModule(ConfigModule):
    """Base implementation using FIELD_MAP and inheritance.
    
    FIELD_MAP: list of tuples (field_name, bit_width[, mapper])
      - field_name: str
      - bit_width: int
      - mapper (optional): function to convert JSON value to int
    """

    FIELD_MAP: List[Union[Tuple[str, int], Tuple[str, int, Callable]]] = []

    def __init__(self):
        # Initialize all field values with default 0
        self.values = {entry[0]: 0 for entry in self.FIELD_MAP}

    def from_json(self, cfg: dict):
        """Populate values from a JSON dict."""
        for entry in self.FIELD_MAP:
            name = entry[0]
            if name in cfg:
                self.values[name] = cfg[name]

    def _encode_list(self, val: list, width: int) -> list[int]:
        """Encode a list of integers (or NodeIndex/NodeIndexFuture) into chunks of <=64-bit ints."""
        
        # Convert NodeIndex/NodeIndexFuture to int
        def to_int(x):
            if x is None:
                return 0
            return int(x)  # NodeIndex/__int__ or int

        val_ints = [to_int(v) for v in val]

        if width is None:
            raise ValueError("List encoding requires width")
        
        bits_per_elem = max(1, width // len(val_ints))
        bin_str = "".join(format(v, f"0{bits_per_elem}b") for v in val_ints)

        # Split into 64-bit words
        chunks = []
        for i in range(0, len(bin_str), 64):
            sub = bin_str[i:i+64]
            chunks.append(int(sub, 2))
        return chunks

    def _to_list_ints(self, val, mapper: Callable = None, width: int = None) -> List[int]:
        """Convert a field value into a list of ints (each <=64-bit)."""
        if mapper:
            argc = mapper.__code__.co_argcount
            if argc == 2:
                val = mapper(self, val)
            else:
                val = mapper(val)

        if val is None:
            return [0]
        if isinstance(val, (int, bool)) or hasattr(val, "__int__"):
            return [int(val)]
        if isinstance(val, list):
            return self._encode_list(val, width)

        raise TypeError(f"Cannot convert value {val} of type {type(val)}")

    def to_bits(self) -> List[Bit]:
        """Convert fields to a list of Bit objects."""
        bits: List[Bit] = []
        for entry in self.FIELD_MAP:
            name, width, *rest = entry
            mapper = rest[0] if rest else None
            ints = self._to_list_ints(self.values[name], mapper, width)
            for word in ints:
                bits.append(Bit(word, min(width, 64)))
        return bits
    
    def dump(self, indent: int = 0):
        """
        Print field values and their binary encoding.
        If the module has submodules, recursively dump them.
        """
        prefix = " " * indent
        print(f"{prefix}=== Dump: {self.__class__.__name__} ===")

        if hasattr(self, "submodules") and self.submodules:
            # This is a composite module
            for sm in self.submodules:
                sm.dump(indent + 2)
        else:
            # Leaf module with FIELD_MAP
            for entry in self.FIELD_MAP:
                name, width, *rest = entry
                mapper = rest[0] if rest else None
                val = self.values.get(name, None)
                ints = self._to_list_ints(val, mapper, width)

                encoded = [
                    bin(i)[2:].zfill(width) if width <= 64 else hex(i)
                    for i in ints
                ]
                print(f"{prefix}{name:<30} | value={str(val):<35} | encoded={encoded}")