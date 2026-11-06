"""
Modbus BCD Payload Builder
-----------------------------------------------------------
This is an example of building a custom payload builder
that can be used in the pymodbus library. Below is a
simple binary coded decimal builder and decoder.
"""

from struct import pack

from pymodbus.constants import Endian
from pymodbus.exceptions import ParameterException
from pymodbus.payload import BinaryPayloadDecoder

try:
    from pymodbus.payload import pack_bitstring, unpack_bitstring
except ImportError:
    from pymodbus.utilities import pack_bitstring, unpack_bitstring

if not hasattr(Endian, "Little") and hasattr(Endian, "LITTLE"):
    Endian.Little = Endian.LITTLE
    Endian.Big = Endian.BIG


def convert_from_bcd(payload):
    if not payload:
        return 0
    place = 10 ** (len(payload) * 2 - 1)
    decimal = 0
    for byte_val in payload:
        shift = 4
        for _x in range(2):
            nibble = (byte_val >> shift) & 0xF
            decimal += nibble * place
            shift = 0
            place //= 10
    return int(decimal)


class BcdPayloadDecoder:
    """
    A utility that helps decode binary coded decimal payload
    messages from a modbus response message. What follows is
    a simple example::

        decoder = BcdPayloadDecoder(payload)
        first = decoder.decode_int(2)
        second = decoder.decode_int(5) / 100
    """

    def __init__(self, payload):
        """Initialize a new payload decoder

        :param payload: The payload to decode with
        """
        self._payload = payload
        self._pointer = 0x00

    @staticmethod
    def fromRegisters(registers, endian=Endian.Little):
        """Initialize a payload decoder with the result of
        reading a collection of registers from a modbus device.

        The registers are treated as a list of 2 byte values.
        We have to do this because of how the data has already
        been decoded by the rest of the library.

        :param registers: The register results to initialize with
        :param endian: The endianness of the payload
        :returns: An initialized PayloadDecoder
        """
        if isinstance(registers, list):  # repack into flat binary
            payload = b"".join(pack("!H", x) for x in registers)
            type(payload)
            return BinaryPayloadDecoder(payload, endian)
        raise ParameterException("Invalid collection of registers supplied")

    @staticmethod
    def fromCoils(coils, endian=Endian.Little):
        """Initialize a payload decoder with the result of
        reading a collection of coils from a modbus device.

        The coils are treated as a list of bit(boolean) values.

        :param coils: The coil results to initialize with
        :param endian: The endianness of the payload
        :returns: An initialized PayloadDecoder
        """
        if isinstance(coils, list):
            payload = pack_bitstring(coils)
            return BinaryPayloadDecoder(payload, endian)
        raise ParameterException("Invalid collection of coils supplied")

    def reset(self):
        """Reset the decoder pointer back to the start"""
        self._pointer = 0x00

    def decode_int(self, size=1):
        """Decodes a int or long from the buffer"""
        self._pointer += size
        handle = self._payload[self._pointer - size : self._pointer]
        return convert_from_bcd(handle)

    def decode_bits(self):
        """Decodes a byte worth of bits from the buffer"""
        self._pointer += 1
        handle = self._payload[self._pointer - 1 : self._pointer]
        return unpack_bitstring(handle)

    def decode_string(self, size=1):
        """Decodes a string from the buffer

        :param size: The size of the string to decode
        """
        self._pointer += size
        return self._payload[self._pointer - size : self._pointer]


# --------------------------------------------------------------------------- #
# Exported Identifiers
# --------------------------------------------------------------------------- #
__all__ = ["BcdPayloadDecoder"]
