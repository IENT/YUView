from enum import Enum, unique, auto

@unique
class Coding(Enum):
    FIXED_CODE = auto()        # f(x) - fixed code to assert
    UNSIGNED_FIXED = auto()    # u(x) - unsigned int with a fixed number of bits
    UNSIGNED_VARIABLE = auto() # u(v) - unsigned int with a variable number of bits
    UNSIGNED_EXP = auto()      # ue(v) - unsigned int wit exp golomb coding
    SIGNED_FIXED = auto()      # i(x) - signed int with a fixed number of bits using twos complement
    SIGNED_EXP = auto()        # se(v) - signed int with exp golomb coding
    BYTE = auto()              # b(8) - a byte (8 bits)
    STRING = auto()            # st(x) - null terminated string in ISO/IEC 10646 UCS
    UNKNOWN = auto()

def isCodingType(descriptor : str):
    try:
        CodingType(descriptor)
    except:
        return False
    return True

class CodingType():
    def __init__(self, descriptor : str):
        self.length = 0
        self.codingType = None
        self.parseCodingType(descriptor)

    def parseCodingType(self, descriptor : str):
        if descriptor.startswith("f("):
            self.codingType = Coding.FIXED_CODE
            self.length = int(descriptor[2:-1])
        elif descriptor.startswith("u(v)"):
            self.codingType = Coding.UNSIGNED_VARIABLE
        elif descriptor.startswith("u("):
            self.codingType = Coding.UNSIGNED_FIXED
            self.length = int(descriptor[2:-1])
        elif descriptor.startswith("ue(v)"):
            self.codingType = Coding.UNSIGNED_EXP
        elif descriptor.startswith("se(v)"):
            self.codingType = Coding.SIGNED_EXP
        elif descriptor.startswith("b(8)"):
            # Used in e.g. VSEI standard
            self.codingType = Coding.BYTE
        elif descriptor.startswith("i("):
            self.codingType = Coding.SIGNED_FIXED
            self.length = int(descriptor[2:-1])
        elif descriptor.startswith("st(v)"):
            self.codingType = Coding.STRING
        else:
            self.codingType = Coding.UNKNOWN

    def __str__(self):
        if self.codingType == Coding.FIXED_CODE:
            return f"f({self.length})"
        if self.codingType == Coding.UNSIGNED_VARIABLE:
            return "u(v)"
        if self.codingType == Coding.UNSIGNED_FIXED:
            return f"u({self.length})"
        if self.codingType == Coding.UNSIGNED_EXP:
            return "ue(v)"
        if self.codingType == Coding.SIGNED_EXP:
            return "se(v)"
        if self.codingType == Coding.BYTE:
            return "b(8)"
        if self.codingType == Coding.SIGNED_FIXED:
            return f"i({self.length})"
        if self.codingType == Coding.STRING:
            return "st(v)"
        if self.codingType == Coding.UNKNOWN:
            return "unknown"
        return "Err"
