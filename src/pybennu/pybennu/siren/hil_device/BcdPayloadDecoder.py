"""
Modbus BCD Payload Builder
-----------------------------------------------------------
This is an example of building a custom payload builder
that can be used in the pymodbus library. Below is a
simple binary coded decimal builder and decoder.
"""
from struct import pack, unpack
from pymodbus.constants import Endian
from pymodbus.interfaces import IPayloadBuilder
from pymodbus.utilities import pack_bitstring
from pymodbus.utilities import unpack_bitstring
from pymodbus.exceptions import ParameterException
from pymodbus.payload import BinaryPayloadDecoder

'''
def convert_from_bcd(bcd):
    """ Converts a bcd value to a decimal value
    :param value: The value to unpack from bcd
    :returns: The number in decimal form
    """
    zero = b'0'
    four = b'4'
    full = b'1111'
    place, decimal = 1, 0
    while bcd > zero:
        nibble = bcd & full # 0xf
        decimal += nibble * place
        bcd >>= 4
        place *= 10
    return decimal
'''
'''
def convert_from_bcd(bcd):
    """ Converts a bcd value to a decimal value

    :param value: The value to unpack from bcd
    :returns: The number in decimal form
    """
    zero = b'0'
    full = b'15'
    place, decimal = 1, 0
    while bcd > zero:
        nibble = bcd & full #0xf
        decimal += nibble * place
        bcd >>= 4
        place *= 10
    return decimal
'''

def convert_from_bcd(payload):
    place, decimal = 10000000, 0
    for nibble in payload:
        shift = 4
        for x in range(2):
            nibble_2 = (nibble >> shift) & 0xf
            decimal += nibble_2 * place
            #nibble >>= 4
            shift = 0
            place /= 10
    return int(decimal)


class BcdPayloadDecoder(object):
    """
    A utility that helps decode binary coded decimal payload
    messages from a modbus reponse message. What follows is
    a simple example::

        decoder = BcdPayloadDecoder(payload)
        first   = decoder.decode_int(2)
        second  = decoder.decode_int(5) / 100
    """

    def __init__(self, payload):
        """ Initialize a new payload decoder

        :param payload: The payload to decode with
        """
        self._payload = payload
        self._pointer = 0x00

    @staticmethod
    def fromRegisters(registers, endian=Endian.Little):
        """ Initialize a payload decoder with the result of
        reading a collection of registers from a modbus device.

        The registers are treated as a list of 2 byte values.
        We have to do this because of how the data has already
        been decoded by the rest of the library.

        :param registers: The register results to initialize with
        :param endian: The endianess of the payload
        :returns: An initialized PayloadDecoder
        """
        if isinstance(registers, list): # repack into flat binary
            payload = b''.join(pack('!H', x) for x in registers)
            p_type = type(payload)
            return BinaryPayloadDecoder(payload, endian)
        raise ParameterException('Invalid collection of registers supplied')

    @staticmethod
    def fromCoils(coils, endian=Endian.Little):
        """ Initialize a payload decoder with the result of
        reading a collection of coils from a modbus device.

        The coils are treated as a list of bit(boolean) values.

        :param coils: The coil results to initialize with
        :param endian: The endianess of the payload
        :returns: An initialized PayloadDecoder
        """
        if isinstance(coils, list):
            payload = pack_bitstring(coils)
            return BinaryPayloadDecoder(payload, endian)
        raise ParameterException('Invalid collection of coils supplied')

    def reset(self):
        """ Reset the decoder pointer back to the start
        """
        self._pointer = 0x00

    def decode_int(self, size=1):
        """ Decodes a int or long from the buffer
        """
        self._pointer += size
        handle = self._payload[self._pointer - size:self._pointer]
        return convert_from_bcd(handle)

    def decode_bits(self):
        """ Decodes a byte worth of bits from the buffer
        """
        self._pointer += 1
        handle = self._payload[self._pointer - 1:self._pointer]
        return unpack_bitstring(handle)

    def decode_string(self, size=1):
        """ Decodes a string from the buffer

        :param size: The size of the string to decode
        """
        self._pointer += size
        return self._payload[self._pointer - size:self._pointer]

# --------------------------------------------------------------------------- #
# Exported Identifiers
# --------------------------------------------------------------------------- #
__all__ = ["BcdPayloadDecoder"]