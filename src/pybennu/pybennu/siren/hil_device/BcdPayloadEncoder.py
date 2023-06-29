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


def convert_to_bcd(decimal):
    """ Converts a decimal value to a bcd value
    :param value: The decimal value to to pack into bcd
    :returns: The number in bcd form
    """
    place, bcd = 0, 0
    while decimal > 0:
        type_d = type(decimal)
        nibble = decimal % 10
        bcd += nibble << place
        decimal = int(decimal / 10)
        place += 4
    return bcd


def count_bcd_digits(bcd):
    """ Count the number of digits in a bcd value
    :param bcd: The bcd number to count the digits of
    :returns: The number of digits in the bcd string
    """
    count = 0
    while bcd > 0:
        count += 1
        bcd >>= 4
    return count


class BcdPayloadEncoder(IPayloadBuilder):
    """
    A utility that helps build binary coded decimal payload
    messages to be written with the various modbus messages.
    example::
    builder = BcdPayloadEncoder()
    builder.add_number(1)
    builder.add_number(int(2.234 * 1000))
    payload = builder.build()
    """
    def __init__(self, payload=None, endian=Endian.Little):
        """ Initialize a new instance of the payload builder
        :param payload: Raw payload data to initialize with
        :param endian: The endianess of the payload
        """
        self._payload = payload or []
        self._endian = endian

    def __str__(self):
        """ Return the payload buffer as a string
        :returns: The payload buffer as a string
        """
        return (b''.join(self._payload)).decode('utf-8')

    def reset(self):
        """ Reset the payload buffer
        """
        self._payload = []

    def build(self):
        """ Return the payload buffer as a list
        This list is two bytes per element and can
        thus be treated as a list of registers.
        :returns: The payload buffer as a list
        """
        string = str(self)
        length = len(string)
        string = string + ('\x00' * (length % 2))
        return [string[i:i+2] for i in range(0, length, 2)]

    def add_bits(self, values):
        """ Adds a collection of bits to be encoded
        If these are less than a multiple of eight,
        they will be left padded with 0 bits to make
        it so.
        :param value: The value to add to the buffer
        """
        value = pack_bitstring(values)
        self._payload.append(value)

    def add_number(self, value, size=None):
        """ Adds any 8bit numeric type to the buffer
        :param value: The value to add to the buffer
        """
        encoded = []
        value = convert_to_bcd(int(value))
        
        size = size or count_bcd_digits(value)
        print(f'Value: {value}; Size {size}')
        while size > 0:
            nibble = value & 0xf
            encoded.append(pack('B', nibble))
            value >>= 4
            size -= 1
        self._payload.extend(encoded)

    def add_string(self, value):
        """ Adds a string to the buffer
        :param value: The value to add to the buffer
        """
        self._payload.append(value)


# --------------------------------------------------------------------------- #
# Exported Identifiers
# --------------------------------------------------------------------------- #
__all__ = ["BcdPayloadEncoder"]