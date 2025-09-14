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
        self.values = {}
        for entry in self.FIELD_MAP:
            name = entry[0]
            self.values[name] = 0

    def from_json(self, cfg: dict):
        """Populate values from a JSON dict."""
        for entry in self.FIELD_MAP:
            name = entry[0]
            if name in cfg:
                self.values[name] = cfg[name]

    def _to_int(self, val, mapper: Callable = None, width: int = None):
        """Convert field value to int, supporting scalars, bools, and lists."""
        if mapper:
            return int(mapper(val))
        if val is None:
            return 0  # treat missing/None as 0
        if isinstance(val, int):
            return val
        if isinstance(val, bool):
            return int(val)

        if isinstance(val, list):
            # If all elements are 0/1, treat as bit vector
            if all(v in (0, 1) for v in val):
                bin_str = "".join(str(v) for v in val)
                return int(bin_str, 2)

            # Otherwise, pack integers into fixed-width chunks
            if width is None:
                raise ValueError("List encoding requires a width")
            bits_per_elem = max(1, width // len(val))
            bin_str = "".join(format(v, f"0{bits_per_elem}b") for v in val)
            return int(bin_str, 2)

        raise TypeError(f"Cannot convert value {val} of type {type(val)} to Bit")


    def to_bits(self) -> List[Bit]:
        bits: List[Bit] = []
        for entry in self.FIELD_MAP:
            name = entry[0]
            width = entry[1]
            mapper = entry[2] if len(entry) > 2 else None
            bits.append(Bit(self._to_int(self.values[name], mapper, width), width))
        return bits