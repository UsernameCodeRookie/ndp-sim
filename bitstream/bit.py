# Bit class implementation and demonstration tests
from dataclasses import dataclass
from typing import List

@dataclass(frozen=True)
class Bit:
    """
    Bit: an arbitrary-width bit-vector.
    
    - Indexing: bit[0] is the least-significant-bit (LSB).
    - Value is always stored modulo 2**width (wrap-around).
    - Bitwise operations between two Bit objects produce a Bit whose width is the maximum of the operand widths
      (operands are aligned on the LSB).
    
    Supported operations:
      - &, |, ^, ~ (bitwise)
      - <<, >> (logical shifts, width preserved)
      - + (modular addition, width preserved)
      - indexing (int -> 0/1), slicing -> Bit
      - int(b) -> unsigned integer value
      - to_signed() -> signed integer (two's complement)
      - concat(other) -> concatenate self (upper bits) with other (lower bits)
      - to_bytes(byteorder='little') / from_bytes
    """
    value: int
    width: int = 1

    def __post_init__(self):
        if self.width < 1:
            raise ValueError("width must be >= 1")
        # normalize value into allowed range
        mask = (1 << self.width) - 1
        object.__setattr__(self, "value", int(self.value) & mask)
        object.__setattr__(self, "mask", mask)

    @classmethod
    def from_bool(cls, val: bool) -> "Bit":
        """Create a 1-bit Bit from a boolean value."""
        return cls(1 if val else 0, 1)
    
    @classmethod
    def from_int(cls, val: int, width: int):
        return cls(val, width)

    @classmethod
    def from_bytes(cls, b: bytes, width: int, byteorder: str = "little"):
        """Create Bit from bytes. `width` controls how many bits to keep (excess bytes are accepted)."""
        int_val = int.from_bytes(b, byteorder=byteorder, signed=False)
        return cls(int_val, width)
    
    @classmethod
    def from_list(cls, vals: List[int], bits_per_elem: int = None) -> "Bit":
        """
        Create Bit from a list of integers (LSB = rightmost element).
        - If vals are 0/1 only, treat as bit-vector string.
        - Else, pack each integer into fixed-width fields.
        """
        if not vals:
            return cls(0, 1)

        if all(v in (0, 1) for v in vals):
            # interpret as binary digits
            bin_str = "".join(str(v) for v in vals)
            return cls(int(bin_str, 2), len(vals))

        # Otherwise: pack integers
        if bits_per_elem is None:
            # choose minimal width per element
            bits_per_elem = max(1, max(v.bit_length() for v in vals))
        width = bits_per_elem * len(vals)
        int_val = 0
        for v in vals:
            int_val = (int_val << bits_per_elem) | int(v)
        return cls(int_val, width)

    def to_bytes(self, byteorder: str = "little") -> bytes:
        nbytes = (self.width + 7) // 8
        return int(self.value).to_bytes(nbytes, byteorder=byteorder, signed=False)

    def __int__(self):
        return int(self.value)

    def to_signed(self) -> int:
        """Interpret the bitvector as two's complement signed integer."""
        if self.value & (1 << (self.width - 1)):
            # negative
            return self.value - (1 << self.width)
        return self.value

    def __len__(self):
        return self.width

    # Indexing: 0 = LSB
    def __getitem__(self, idx):
        if isinstance(idx, int):
            # support negative indices relative to width, like Python lists
            if idx < 0:
                idx += self.width
            if not (0 <= idx < self.width):
                raise IndexError("bit index out of range")
            return (self.value >> idx) & 1
        if isinstance(idx, slice):
            step = idx.step if idx.step is not None else 1
            if step != 1:
                # support only step==1 for simplicity
                raise ValueError("slicing with step != 1 is not supported")
            start = idx.start if idx.start is not None else 0
            stop = idx.stop if idx.stop is not None else self.width
            # allow negatives
            if start < 0:
                start += self.width
            if stop < 0:
                stop += self.width
            if not (0 <= start <= stop <= self.width):
                raise IndexError("slice out of range")
            new_width = stop - start
            new_val = (self.value >> start) & ((1 << new_width) - 1) if new_width > 0 else 0
            return Bit(new_val, new_width)
        raise TypeError("index must be int or slice")

    # Bitwise ops: align on LSB, result width = max(widths)
    def _align_other(self, other):
        if isinstance(other, Bit):
            other_val = other.value
            other_w = other.width
        elif isinstance(other, int):
            other_val = int(other)
            other_w = max(other_val.bit_length(), 1)
        else:
            return NotImplemented
        res_w = max(self.width, other_w)
        return other_val & ((1 << res_w) - 1), other_w, res_w

    def __and__(self, other):
        other_val, other_w, res_w = self._align_other(other)
        res = (self.value & other_val) & ((1 << res_w) - 1)
        return Bit(res, res_w)

    def __or__(self, other):
        other_val, other_w, res_w = self._align_other(other)
        res = (self.value | other_val) & ((1 << res_w) - 1)
        return Bit(res, res_w)

    def __xor__(self, other):
        other_val, other_w, res_w = self._align_other(other)
        res = (self.value ^ other_val) & ((1 << res_w) - 1)
        return Bit(res, res_w)

    def __invert__(self):
        return Bit((~self.value) & self.mask, self.width)

    # Logical shifts (width preserved)
    def __lshift__(self, bits: int):
        if bits < 0:
            return self >> (-bits)
        return Bit((self.value << bits) & self.mask, self.width)

    def __rshift__(self, bits: int):
        if bits < 0:
            return self << (-bits)
        return Bit((self.value >> bits) & self.mask, self.width)

    def arithmetic_rshift(self, bits: int):
        """Arithmetic (signed) right shift: sign bit replicated."""
        if bits < 0:
            return self << (-bits)
        if bits >= self.width:
            # result is all sign bits
            sign = 1 if (self.value >> (self.width - 1)) & 1 else 0
            return Bit(((1 << self.width) - 1) if sign else 0, self.width)
        # replicate sign on left
        sign = (self.value >> (self.width - 1)) & 1
        shifted = self.value >> bits
        if sign:
            mask = ((1 << (self.width - bits)) - 1) << (bits)
            shifted |= mask
        return Bit(shifted & self.mask, self.width)

    # modular addition (wraps within width)
    def __add__(self, other):
        if isinstance(other, Bit):
            other_val = other.value
        elif isinstance(other, int):
            other_val = int(other)
        else:
            return NotImplemented
        res = (self.value + other_val) & self.mask
        return Bit(res, self.width)

    # equality and hashing
    def __eq__(self, other):
        if isinstance(other, Bit):
            return self.width == other.width and self.value == other.value
        return False

    def __hash__(self):
        return hash((self.value, self.width))

    def concat(self, other: "Bit"):
        """Concatenate self (upper bits / MSB side) with other (lower bits / LSB side).
        Result width = self.width + other.width.
        Example: Bit(0b11,2).concat(Bit(0b01,2)) -> Bit(0b1101,4)
        """
        if not isinstance(other, Bit):
            raise TypeError("concat expects a Bit")
        new_w = self.width + other.width
        new_val = (self.value << other.width) | other.value
        return Bit(new_val, new_w)

    def __repr__(self):
        return f"Bit(0b{self.value:0{(self.width+3)//4}x}, width={self.width})"

    def __str__(self):
        return f"{self.value} (0b{self.value:0{self.width}b}, width={self.width})"

