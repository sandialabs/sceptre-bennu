from unittest.mock import MagicMock, patch

import pytest
from pymodbus.exceptions import ModbusException

from pybennu.modbus import ModbusWrapper
from pybennu.settings import ModbusRegister

# =============================================================================
# Float32 Encoding / Decoding Tests
# =============================================================================


@pytest.mark.parametrize(
    ("val", "expected_regs"),
    [
        (0.0, [0, 0]),
        (1.0, [16256, 0]),
        (-1.0, [49024, 0]),
        (3.14159, [16457, 4048]),
        (12345.678, [17984, 59062]),
    ],
)
def test_float32_known_vectors(val, expected_regs):
    encoded = ModbusWrapper.encode_float(val)
    assert encoded == expected_regs

    decoded = ModbusWrapper.decode_float(encoded)
    assert decoded == pytest.approx(val, rel=1e-5)


def test_float32_roundtrip():
    test_values = [0.0, -0.0, 1.5, -42.25, 100000.0, 0.0001234]
    for val in test_values:
        encoded = ModbusWrapper.encode_float(val)
        decoded = ModbusWrapper.decode_float(encoded)
        assert decoded == pytest.approx(val, rel=1e-5)


# =============================================================================
# Read Register Tests
# =============================================================================


@pytest.fixture
def wrapper(monkeypatch):
    # Mock ModbusTcpClient during init so no real network sockets are opened
    with patch("pybennu.modbus.ModbusTcpClient") as mock_client_cls:
        mock_instance = MagicMock()
        mock_client_cls.return_value = mock_instance
        w = ModbusWrapper(ip="127.0.0.1", port=502, timeout=2.0)
        w.client = mock_instance
        return w


def test_read_register_holding(wrapper):
    r = ModbusRegister(
        num=100, name="freq", reg_type="holding", access="r", data_type="float32"
    )
    wrapper.client.read_holding_registers.return_value = MagicMock(
        registers=ModbusWrapper.encode_float(60.0)
    )

    val = wrapper.read_register(r)
    wrapper.client.read_holding_registers.assert_called_once_with(100, count=2)
    assert val == pytest.approx(60.0)


def test_read_register_input(wrapper):
    r = ModbusRegister(
        num=200, name="voltage", reg_type="input", access="r", data_type="float32"
    )
    wrapper.client.read_input_registers.return_value = MagicMock(
        registers=ModbusWrapper.encode_float(120.0)
    )

    val = wrapper.read_register(r)
    wrapper.client.read_input_registers.assert_called_once_with(200, count=2)
    assert val == pytest.approx(120.0)


def test_read_register_coil(wrapper):
    r = ModbusRegister(
        num=10, name="relay", reg_type="coil", access="rw", data_type="bool"
    )
    wrapper.client.read_coils.return_value = MagicMock(bits=[True])

    val = wrapper.read_register(r)
    wrapper.client.read_coils.assert_called_once_with(10, count=1)
    assert val is True


def test_read_register_discrete(wrapper):
    r = ModbusRegister(
        num=20, name="switch", reg_type="discrete", access="r", data_type="bool"
    )
    wrapper.client.read_discrete_inputs.return_value = MagicMock(bits=[False])

    val = wrapper.read_register(r)
    wrapper.client.read_discrete_inputs.assert_called_once_with(20, count=1)
    assert val is False


def test_read_register_permissions_check(wrapper):
    r = ModbusRegister(
        num=30, name="cmd", reg_type="holding", access="w", data_type="float32"
    )
    with pytest.raises(ValueError, match="Attempt to read write-only register"):
        wrapper.read_register(r)


def test_read_register_invalid_reg_type(wrapper):
    r = ModbusRegister(
        num=40, name="bad", reg_type="holding", access="r", data_type="float32"
    )
    r.reg_type = "invalid_type"
    with pytest.raises(TypeError, match="Unknown reg_type"):
        wrapper.read_register(r)


# =============================================================================
# Write Register Tests
# =============================================================================


def test_write_register_holding(wrapper):
    r = ModbusRegister(
        num=105, name="setpoint", reg_type="holding", access="w", data_type="float32"
    )
    wrapper.write_register(r, 1.0)
    # 1.0 is [16256, 0]
    wrapper.client.write_registers.assert_called_once_with(105, [16256, 0])


def test_write_register_coil_bool_and_str(wrapper):
    r = ModbusRegister(
        num=15, name="breaker", reg_type="coil", access="rw", data_type="bool"
    )

    # Boolean True
    wrapper.write_register(r, True)
    wrapper.client.write_coil.assert_called_with(15, True)

    # String "true" / "1" / "false"
    wrapper.write_register(r, "true")
    wrapper.client.write_coil.assert_called_with(15, True)

    wrapper.write_register(r, "false")
    wrapper.client.write_coil.assert_called_with(15, False)


def test_write_register_discrete_and_input_unsupported(wrapper):
    r_disc = ModbusRegister(
        num=1, name="disc", reg_type="discrete", access="rw", data_type="bool"
    )
    with pytest.raises(NotImplementedError, match="not supported for writing"):
        wrapper.write_register(r_disc, True)

    r_inp = ModbusRegister(
        num=2, name="inp", reg_type="input", access="rw", data_type="float32"
    )
    with pytest.raises(NotImplementedError, match="not supported for writing"):
        wrapper.write_register(r_inp, 10.0)


def test_write_register_permissions_check(wrapper):
    r = ModbusRegister(
        num=50, name="read_only", reg_type="holding", access="r", data_type="float32"
    )
    with pytest.raises(ValueError, match="Attempt to write to read-only register"):
        wrapper.write_register(r, 42.0)


def test_write_register_invalid_reg_type(wrapper):
    r = ModbusRegister(
        num=60, name="bad_write", reg_type="holding", access="w", data_type="float32"
    )
    r.reg_type = "non_existent"
    with pytest.raises(TypeError, match="Unknown reg_type"):
        wrapper.write_register(r, 12.0)


# =============================================================================
# Connection Management Tests
# =============================================================================


def test_connection_management_methods(wrapper):
    assert repr(wrapper) == "ModbusWrapper(ip=127.0.0.1, port=502, timeout=2.0)"
    assert str(wrapper) == "127.0.0.1:502"

    wrapper.client.connect.return_value = True
    assert wrapper.connect() is True
    wrapper.client.connect.assert_called_once()

    wrapper.disconnect()
    wrapper.client.close.assert_called_once()


def test_retry_until_connected(wrapper):
    # First attempt: ModbusException
    # Second attempt: returns False
    # Third attempt: returns True
    wrapper.client.connect.side_effect = [
        ModbusException("connection refused"),
        False,
        True,
    ]

    with patch("time.sleep") as mock_sleep:
        wrapper.retry_until_connected(delay=1.5)
        assert wrapper.client.connect.call_count == 3
        assert mock_sleep.call_count == 2
        mock_sleep.assert_called_with(1.5)
