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
        self._is_empty = False
    
    def is_empty(self) -> bool:
        """Check if this module is empty (all fields are None or 0)."""
        if hasattr(self, '_is_empty') and self._is_empty:
            return True
        
        # Check submodules first if they exist
        if hasattr(self, 'submodules') and self.submodules:
            return all(sm.is_empty() for sm in self.submodules)
        
        # For leaf modules, check if all values are None or 0
        if not self.values:
            return True
        return all(v is None or v == 0 for v in self.values.values())
    
    def mark_empty(self):
        """Mark this module as empty."""
        self._is_empty = True
    
    def register_to_mapper(self):
        """Register this module to the mapper after resource allocation."""
        from bitstream.config.mapper import NodeGraph
        
        if not hasattr(self, 'id') or not self.id:
            return
        
        # Skip empty modules
        if self.is_empty():
            return
        
        mapper = NodeGraph.get().mapping
        node_name = self.id.node_name
        resource = mapper.get(node_name)
        
        if resource:
            mapper.register_module(resource, self)

    def from_json(self, cfg: dict):
        """Populate values from a JSON dict.
        
        Note: Mapper functions are NOT applied here. They will be applied
        during encoding (to_bits/dump). This preserves the original JSON values.
        
        Exception: Mappers that create special objects (like Connect) that need
        'self' context must be applied here. These are identified by taking 2 arguments.
        """
        for entry in self.FIELD_MAP:
            name, width, *rest = entry
            mapper = rest[0] if rest else None
            if name in cfg:
                val = cfg[name]
                # Only apply mapper if it requires 'self' context (2 args)
                # These typically create Connect objects or need module context
                if mapper and mapper.__code__.co_argcount == 2:
                    val = mapper(self, val)
                # For simple mappers (1 arg), preserve the original value
                # They will be applied during encoding
                self.values[name] = val

    def _encode_list(self, val: list, width: int) -> list[tuple[int, int]]:
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

        # Split into 128-bit words
        chunks: list[tuple[int, int]] = []
        for i in range(0, len(bin_str), 128):
            sub = bin_str[i:i+128]
            chunks.append((int(sub, 2), len(sub)))
        return chunks

    def _encode_field(self, val, mapper: Callable = None, width: int = None) -> List[tuple[int, int]]:
        """Convert a field value into one or more (value,width) tuples."""
        # Import Connect here to avoid circular import
        from bitstream.index import Connect
        
        if mapper:
            argc = mapper.__code__.co_argcount
            # Only apply mapper if value hasn't been processed yet
            # If it's already a Connect object, it was processed in from_json()
            if argc == 2 and not isinstance(val, Connect):
                val = mapper(self, val)
            elif argc == 1:
                val = mapper(val)

        if val is None:
            return [(0, width if width is not None else 1)]
        if isinstance(val, (int, bool)) or hasattr(val, "__int__"):
            return [(int(val), width if width is not None else max(int(val).bit_length(), 1))]
        if isinstance(val, list):
            return self._encode_list(val, width)

        raise TypeError(f"Cannot convert value {val} of type {type(val)}")

    def _encode_field_for_dump(self, val, mapper: Callable = None, width: int = None) -> List[tuple[int, int]]:
        """Encode field value for display in dump, applying mapper to get the actual encoded value.
        
        This is different from _encode_field in that it always applies the mapper,
        showing both the original value and the encoded result separately.
        """
        # Import Connect here to avoid circular import
        from bitstream.index import Connect
        
        encoded_val = val
        if mapper:
            argc = mapper.__code__.co_argcount
            # Always apply mapper for encoding display, unless it's already a Connect
            if argc == 2 and not isinstance(val, Connect):
                encoded_val = mapper(self, val)
            elif argc == 1:
                encoded_val = mapper(val)
        
        # Now encode the mapped value
        if encoded_val is None:
            return [(0, width if width is not None else 1)]
        if isinstance(encoded_val, (int, bool)) or hasattr(encoded_val, "__int__"):
            return [(int(encoded_val), width if width is not None else max(int(encoded_val).bit_length(), 1))]
        if isinstance(encoded_val, list):
            return self._encode_list(encoded_val, width)

        raise TypeError(f"Cannot convert value {encoded_val} of type {type(encoded_val)}")

    def to_bits(self) -> List[Bit]:
        """Convert fields to a list of Bit objects."""
        bits: List[Bit] = []
        for entry in self.FIELD_MAP:
            name, width, *rest = entry
            mapper = rest[0] if rest else None
            encoded_parts = self._encode_field(self.values[name], mapper, width)
            for word, part_width in encoded_parts:
                bits.append(Bit(word, part_width))
        return bits
    
    def dump(self, indent: int = 0) -> bool:
        """
        Print field values and their binary encoding.
        If the module has submodules, recursively dump them.
        Skips empty modules (all None or 0).
        Returns True if any content was actually printed.
        """
        # Skip empty modules
        if self.is_empty():
            return False
        
        prefix = " " * indent
        print(f"{prefix}=== Dump: {self.__class__.__name__} ===")

        if hasattr(self, "submodules") and self.submodules:
            # This is a composite module
            has_content = False
            for sm in self.submodules:
                if sm.dump(indent + 2):
                    has_content = True
            return has_content or True  # Parent header was printed
        else:
            # Leaf module with FIELD_MAP
            for entry in self.FIELD_MAP:
                name, width, *rest = entry
                mapper = rest[0] if rest else None
                val = self.values.get(name, None)
                
                # Display original value (without mapper applied)
                display_val = str(val)
                
                # Encode with mapper applied for the encoded result
                encoded_parts = self._encode_field_for_dump(val, mapper, width)

                encoded = [
                    bin(value)[2:].zfill(part_width) if part_width <= 64 else hex(value)
                    for value, part_width in encoded_parts
                ]
                print(f"{prefix}{name:<30} | value={display_val:<35} | encoded={encoded}")
            return True