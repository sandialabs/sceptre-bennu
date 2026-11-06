import pytest
from pymodbus.constants import Endian
from pymodbus.exceptions import ParameterException

from pybennu.siren.hil_device.BcdPayloadDecoder import (
    BcdPayloadDecoder,
    convert_from_bcd,
)
from pybennu.siren.hil_device.BcdPayloadEncoder import (
    BcdPayloadEncoder,
    convert_to_bcd,
    count_bcd_digits,
)

# =============================================================================
# convert_from_bcd Tests
# =============================================================================


@pytest.mark.parametrize(
    ("payload", "expected"),
    [
        ([0x12, 0x34], 1234),
        ([0x98, 0x76], 9876),
        ([0x00, 0x00], 0),
        ([0x00], 0),
        ([0x42], 42),
        ([0x05], 5),
        ([0x99, 0x99, 0x99, 0x99], 99999999),
        ([0x12, 0x34, 0x56, 0x78], 12345678),
        ([0x01, 0x02, 0x03, 0x04], 1020304),
        (
            [0x12, 0x34, 0x56, 0x78, 0x90, 0x12, 0x34, 0x56],
            1234567890123456,
        ),
        (b"\x12\x34", 1234),
        (b"\x98\x76", 9876),
        ([], 0),
        (b"", 0),
    ],
)
def test_convert_from_bcd_known_vectors(payload, expected):
    """Verify nibble extraction and place-value arithmetic for known BCD vectors."""
    assert convert_from_bcd(payload) == expected


def test_convert_from_bcd_nibble_extraction():
    """Verify that each individual nibble contributes to its exact decimal place."""
    # 0x07 -> high nibble 0, low nibble 7 -> 7
    assert convert_from_bcd([0x07]) == 7
    # 0x70 -> high nibble 7, low nibble 0 -> 70
    assert convert_from_bcd([0x70]) == 70
    # 0x00, 0x07 -> 0007 -> 7
    assert convert_from_bcd([0x00, 0x07]) == 7
    # 0x07, 0x00 -> 0700 -> 700
    assert convert_from_bcd([0x07, 0x00]) == 700


# =============================================================================
# BcdPayloadDecoder.fromRegisters Tests
# =============================================================================


def test_from_registers_16bit():
    """Verify 16-bit register decoding with Endian.Big and Endian.Little."""
    registers = [0x1234]
    # Big Endian
    decoder_big = BcdPayloadDecoder.fromRegisters(registers, endian=Endian.Big)
    assert decoder_big.decode_16bit_uint() == 0x1234

    # Little Endian
    decoder_little = BcdPayloadDecoder.fromRegisters(registers, endian=Endian.Little)
    # 0x1234 packed in big-endian bytes is [0x12, 0x34].
    # Interpreted as little-endian 16-bit uint: (0x34 << 8) | 0x12 = 0x3412
    assert decoder_little.decode_16bit_uint() == 0x3412


def test_from_registers_32bit():
    """Verify 32-bit register decoding (integers and floats)."""
    # 0x1234 and 0x5678
    registers = [0x1234, 0x5678]
    decoder = BcdPayloadDecoder.fromRegisters(registers, endian=Endian.Big)
    assert decoder.decode_32bit_uint() == 0x12345678

    # Float decoding: 100.0f in IEEE 754 is 0x42C80000 -> registers [0x42C8, 0x0000]
    registers_float = [0x42C8, 0x0000]
    decoder_float = BcdPayloadDecoder.fromRegisters(registers_float, endian=Endian.Big)
    assert decoder_float.decode_32bit_float() == pytest.approx(100.0)


def test_from_registers_64bit():
    """Verify 64-bit register decoding."""
    registers = [0x1234, 0x5678, 0x9ABC, 0xDEF0]
    decoder = BcdPayloadDecoder.fromRegisters(registers, endian=Endian.Big)
    assert decoder.decode_64bit_uint() == 0x123456789ABCDEF0


def test_from_registers_invalid_input():
    """Verify ParameterException is raised when registers argument is not a list."""
    with pytest.raises(ParameterException, match="Invalid collection of registers"):
        BcdPayloadDecoder.fromRegisters("not-a-list")

    with pytest.raises(ParameterException, match="Invalid collection of registers"):
        BcdPayloadDecoder.fromRegisters(1234)

    with pytest.raises(ParameterException, match="Invalid collection of registers"):
        BcdPayloadDecoder.fromRegisters(None)

    with pytest.raises(ParameterException, match="Invalid collection of registers"):
        BcdPayloadDecoder.fromRegisters({"reg": 1})


# =============================================================================
# BcdPayloadDecoder.fromCoils Tests
# =============================================================================


def test_from_coils_packing():
    """Verify bitstring packing into binary payload and bit decoding."""
    coils = [True, False, True, True, False, False, True, False]
    decoder = BcdPayloadDecoder.fromCoils(coils, endian=Endian.Big)
    # decode_bits returns unpacked bool list
    bits = decoder.decode_bits()
    assert bits[:8] == coils


def test_from_coils_invalid_input():
    """Verify ParameterException is raised when coils argument is not a list."""
    with pytest.raises(ParameterException, match="Invalid collection of coils"):
        BcdPayloadDecoder.fromCoils("invalid-coils")

    with pytest.raises(ParameterException, match="Invalid collection of coils"):
        BcdPayloadDecoder.fromCoils(None)

    with pytest.raises(ParameterException, match="Invalid collection of coils"):
        BcdPayloadDecoder.fromCoils(42)


# =============================================================================
# BcdPayloadDecoder Instance Methods Tests
# =============================================================================


def test_decoder_decode_int_and_reset():
    """Verify decode_int consumes slices of payload and reset restores pointer."""
    # Buffer contains two 2-byte BCD numbers: 1234 (0x12, 0x34) and 5678 (0x56, 0x78)
    buffer = bytes([0x12, 0x34, 0x56, 0x78])
    decoder = BcdPayloadDecoder(buffer)

    first = decoder.decode_int(2)
    assert first == 1234
    assert decoder._pointer == 2

    second = decoder.decode_int(2)
    assert second == 5678
    assert decoder._pointer == 4

    # Test reset
    decoder.reset()
    assert decoder._pointer == 0
    assert decoder.decode_int(2) == 1234


def test_decoder_decode_string():
    """Verify decode_string extracts raw byte slices and advances pointer."""
    buffer = b"SCEPTRE_TEST"
    decoder = BcdPayloadDecoder(buffer)
    assert decoder.decode_string(7) == b"SCEPTRE"
    assert decoder._pointer == 7
    assert decoder.decode_string(5) == b"_TEST"
    assert decoder._pointer == 12


def test_decoder_decode_bits():
    """Verify decode_bits extracts a byte of bits and advances pointer by 1."""
    buffer = bytes([0xAA])  # 10101010
    decoder = BcdPayloadDecoder(buffer)
    bits = decoder.decode_bits()
    assert decoder._pointer == 1
    assert len(bits) == 8


# =============================================================================
# BcdPayloadEncoder Tests
# =============================================================================


@pytest.mark.parametrize(
    ("decimal", "expected_bcd"),
    [
        (0, 0),
        (1, 0x1),
        (9, 0x9),
        (10, 0x10),
        (1234, 0x1234),
        (9876, 0x9876),
        (54321, 0x54321),
    ],
)
def test_convert_to_bcd(decimal, expected_bcd):
    """Verify decimal integer conversion to BCD bit representation."""
    assert convert_to_bcd(decimal) == expected_bcd


@pytest.mark.parametrize(
    ("bcd", "expected_count"),
    [
        (0, 0),
        (0x5, 1),
        (0x42, 2),
        (0x1234, 4),
        (0x98765, 5),
        (0x12345678, 8),
    ],
)
def test_count_bcd_digits(bcd, expected_count):
    """Verify digit counting in BCD values."""
    assert count_bcd_digits(bcd) == expected_count


def test_encoder_add_number():
    """Verify add_number packs BCD nibbles into bytes."""
    encoder = BcdPayloadEncoder()
    encoder.add_number(12)  # 0x12 -> nibbles 2, 1
    assert len(encoder._payload) == 2
    assert encoder._payload == [b"\x02", b"\x01"]


def test_encoder_add_bits():
    """Verify add_bits packs boolean values into byte buffer."""
    encoder = BcdPayloadEncoder()
    bits = [True, False, True, False, False, False, False, False]
    encoder.add_bits(bits)
    assert len(encoder._payload) == 1
    assert isinstance(encoder._payload[0], bytes)


def test_encoder_add_string_and_build():
    """Verify add_string, build register padding and emission."""
    encoder = BcdPayloadEncoder()
    encoder.add_string(b"ABCD")
    regs = encoder.build()
    # 4 bytes -> two 2-byte registers
    assert regs == ["AB", "CD"]

    # Odd length string: 3 bytes "ABC" -> padded to 4 bytes "ABC\x00"
    encoder.reset()
    assert encoder._payload == []
    encoder.add_string("ABC")
    regs_padded = encoder.build()
    assert regs_padded == ["AB", "C\x00"]
