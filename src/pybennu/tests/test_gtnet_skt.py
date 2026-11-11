import socket
import struct
from unittest.mock import MagicMock, patch

import pytest
from pydantic import ValidationError

from pybennu.gtnet_skt import GtnetSkt
from pybennu.settings import GtnetSktSettings, GtnetSktTag, PybennuSettings

# =============================================================================
# Helper Fixtures & Builders
# =============================================================================


def make_tag(name: str, tag_type: str, initial_val: int | float) -> GtnetSktTag:
    suffix = ".closed" if tag_type == "int" else ".value"
    full_name = name if name.endswith((".closed", ".value")) else f"{name}{suffix}"
    return GtnetSktTag(name=full_name, type=tag_type, initial_value=initial_val)


def make_settings(
    tags: list[GtnetSktTag],
    protocol: str = "tcp",
    ip: str = "127.0.0.1",
    port: int = 7000,
    tcp_retry_delay: float = 0.05,
    udp_write_rate: int = 30,
) -> PybennuSettings:
    return PybennuSettings(
        gtnet_skt=GtnetSktSettings(
            enabled=True,
            ip=ip,
            port=port,
            protocol=protocol,
            tcp_retry_delay=tcp_retry_delay,
            udp_write_rate=udp_write_rate,
            tags=tags,
        )
    )


# =============================================================================
# Struct Formatting & Multi-Point Channel Packing Tests
# =============================================================================


def test_struct_format_generation_and_payload_packing():
    """Verify 32-bit big-endian integer and float serialization matches RTDS GTNET-SKT format."""
    tags = [
        make_tag("BRK1", "int", 1),
        make_tag("VOLT1", "float", 120.5),
        make_tag("BRK2", "int", 0),
        make_tag("FREQ1", "float", 60.0),
    ]
    conf = make_settings(tags)
    gtnet = GtnetSkt(conf)

    # Format string must be '!ifif' (Network byte order / Big-endian)
    assert gtnet.struct_format_string == "!ifif"

    payload = gtnet.build_payload()
    # 4 points * 4 bytes each = 16 bytes
    assert len(payload) == 16

    # Unpack with standard struct '!ifif' and verify exact values
    unpacked = struct.unpack("!ifif", payload)
    assert unpacked[0] == 1
    assert unpacked[1] == pytest.approx(120.5, rel=1e-5)
    assert unpacked[2] == 0
    assert unpacked[3] == pytest.approx(60.0, rel=1e-5)


def test_all_integers_channel_packing():
    """Verify contiguous packing of all integer tags."""
    tags = [make_tag(f"BRK_{i}", "int", i * 10) for i in range(5)]
    conf = make_settings(tags)
    gtnet = GtnetSkt(conf)

    assert gtnet.struct_format_string == "!iiiii"
    payload = gtnet.build_payload()
    assert len(payload) == 20
    assert struct.unpack("!iiiii", payload) == (0, 10, 20, 30, 40)


def test_all_floats_channel_packing():
    """Verify contiguous packing of all IEEE-754 single precision float tags."""
    vals = [1.5, -42.25, 0.0, 3.14159, 1000.0]
    tags = [make_tag(f"ANALOG_{i}", "float", v) for i, v in enumerate(vals)]
    conf = make_settings(tags)
    gtnet = GtnetSkt(conf)

    assert gtnet.struct_format_string == "!fffff"
    payload = gtnet.build_payload()
    assert len(payload) == 20
    unpacked = struct.unpack("!fffff", payload)
    for expected, actual in zip(vals, unpacked):
        assert actual == pytest.approx(expected, rel=1e-5)


# =============================================================================
# Partial Write Handling & State In-Memory Dictionary Tests
# =============================================================================


def test_partial_write_updates_state_and_preserves_order():
    """Verify that updating a single tag updates in-memory state and re-packs full packet in configured order."""
    tags = [
        make_tag("BRK1", "int", 0),
        make_tag("GEN_MW", "float", 50.0),
        make_tag("BRK2", "int", 1),
    ]
    conf = make_settings(tags)
    gtnet = GtnetSkt(conf)

    # Initial state
    initial_payload = gtnet.build_payload()
    assert struct.unpack("!ifi", initial_payload) == (0, pytest.approx(50.0), 1)

    # Simulate writing only tag GEN_MW
    typecasted = gtnet.typecast_values({"GEN_MW.value": 99.5})
    with gtnet.lock:
        gtnet.state.update(typecasted)

    # Re-pack and assert only the modified point changed
    updated_payload = gtnet.build_payload()
    unpacked = struct.unpack("!ifi", updated_payload)
    assert unpacked[0] == 0  # unchanged
    assert unpacked[1] == pytest.approx(99.5, rel=1e-5)  # updated
    assert unpacked[2] == 1  # unchanged


def test_typecast_values_variants():
    """Verify typecast_values handles booleans, strings, and numeric type variations."""
    tags = [
        make_tag("BRK1", "int", 0),
        make_tag("VOLT1", "float", 0.0),
    ]
    conf = make_settings(tags)
    gtnet = GtnetSkt(conf)

    # Boolean values to integer 0/1
    assert gtnet.typecast_values({"BRK1.closed": True}) == {"BRK1.closed": 1}
    assert gtnet.typecast_values({"BRK1.closed": False}) == {"BRK1.closed": 0}
    assert gtnet.typecast_values({"BRK1.closed": "true"}) == {"BRK1.closed": 1}
    assert gtnet.typecast_values({"BRK1.closed": "false"}) == {"BRK1.closed": 0}

    # String numeric values (as received from CLI pybennu-probe)
    assert gtnet.typecast_values({"BRK1.closed": "42"}) == {"BRK1.closed": 42}
    assert gtnet.typecast_values({"BRK1.closed": "42.0"}) == {"BRK1.closed": 42}
    assert gtnet.typecast_values({"VOLT1.value": "123.45"}) == {
        "VOLT1.value": pytest.approx(123.45)
    }


def test_typecast_values_malformed_input_raises_error():
    """Verify malformed types, non-numeric values, and unknown tags raise appropriate exceptions."""
    tags = [
        make_tag("BRK1", "int", 0),
        make_tag("VOLT1", "float", 0.0),
    ]
    conf = make_settings(tags)
    gtnet = GtnetSkt(conf)

    # Unknown tag name raises KeyError
    with pytest.raises(KeyError, match=r"Unknown tag 'NONEXISTENT\.closed'"):
        gtnet.typecast_values({"NONEXISTENT.closed": 1})

    # Malformed integer value raises ValueError
    with pytest.raises(ValueError, match="Cannot convert value 'not_a_number' to int"):
        gtnet.typecast_values({"BRK1.closed": "not_a_number"})

    # Malformed float value raises ValueError
    with pytest.raises(ValueError, match="Cannot convert value 'bad_float' to float"):
        gtnet.typecast_values({"VOLT1.value": "bad_float"})

    # Non-scalar malformed value raises ValueError
    with pytest.raises(ValueError, match="Cannot convert value"):
        gtnet.typecast_values({"BRK1.closed": [1, 2, 3]})


def test_gtnet_skt_malformed_tag_type_in_config():
    """Verify invalid tag type in config raises ValueError during initialization."""
    conf = PybennuSettings()
    # Create tag bypassing pydantic validation with unsupported type
    bad_tag = GtnetSktTag.model_construct(
        name="BAD.closed", type="boolean", initial_value=1
    )
    conf.gtnet_skt.tags = [bad_tag]
    with pytest.raises(
        ValueError, match=r"invalid type boolean for GTNET-SKT tag BAD\.closed"
    ):
        GtnetSkt(conf)


def test_gtnet_skt_stop_and_clean_shutdown():
    """Verify stop() safely signals thread loop and closes socket without lingering."""
    tags = [make_tag("BRK1", "int", 1)]
    conf = make_settings(tags, protocol="udp")
    gtnet = GtnetSkt(conf)
    gtnet.start()
    assert gtnet.running is True
    assert gtnet.udp_write_thread.is_alive()

    gtnet.stop()
    assert gtnet.running is False
    assert not gtnet.udp_write_thread.is_alive()
    assert gtnet.socket is None


# =============================================================================
# Channel Limits (30 Points Per Channel) Tests
# =============================================================================


def test_channel_limit_maximum_30_points_success():
    """Verify configuring exactly 30 data points succeeds."""
    tags = [make_tag(f"TAG_{i}", "int", 0) for i in range(30)]
    conf = make_settings(tags)
    gtnet = GtnetSkt(conf)
    assert len(gtnet.tags) == 30
    assert len(gtnet.state) == 30
    assert len(gtnet.build_payload()) == 120  # 30 * 4 bytes


def test_channel_limit_exceeded_raises_error():
    """Verify configuring > 30 data points is rejected by settings validation."""
    # 31 tags exceeds RTDS channel capacity
    tags_31 = [make_tag(f"TAG_{i}", "int", 0) for i in range(31)]

    # 1. Verification through Pydantic settings validation
    with pytest.raises(ValidationError) as exc_info:
        GtnetSktSettings(enabled=True, tags=tags_31)
    assert "maximum of 30 data points" in str(exc_info.value)


# =============================================================================
# Socket Mock Tests: TCP & Reconnection Backoff
# =============================================================================


@patch("socket.socket")
def test_tcp_write_state_sends_exact_binary_packet(mock_socket_cls):
    """Verify tcp_write_state connects and issues sendall with exact binary payload."""
    mock_sock = MagicMock()
    mock_socket_cls.return_value = mock_sock

    tags = [make_tag("BRK1", "int", 1), make_tag("ANALOG1", "float", 12.34)]
    conf = make_settings(tags, protocol="tcp", ip="10.0.0.1", port=7001)
    gtnet = GtnetSkt(conf)

    gtnet.tcp_write_state()

    # Verify socket creation, connection, and sendall
    mock_socket_cls.assert_called_with(socket.AF_INET, socket.SOCK_STREAM)
    mock_sock.connect.assert_called_with(("10.0.0.1", 7001))
    expected_payload = struct.pack("!if", 1, 12.34)
    mock_sock.sendall.assert_called_once_with(expected_payload)


@patch("pybennu.gtnet_skt.sleep")
@patch("socket.socket")
def test_tcp_reconnection_backoff_on_connection_failure(mock_socket_cls, mock_sleep):
    """Verify connection errors trigger socket close and backoff retry delay."""
    # First socket attempt fails on connect; second succeeds
    failing_sock = MagicMock()
    failing_sock.connect.side_effect = ConnectionRefusedError(
        "RTDS simulation not ready"
    )

    success_sock = MagicMock()

    mock_socket_cls.side_effect = [failing_sock, success_sock]

    tags = [make_tag("BRK1", "int", 1)]
    conf = make_settings(tags, protocol="tcp", tcp_retry_delay=0.1)
    gtnet = GtnetSkt(conf)

    # Call init_socket which loops until connection succeeds
    gtnet.init_socket()

    assert mock_socket_cls.call_count == 2
    failing_sock.close.assert_called()
    mock_sleep.assert_called_once_with(0.1)
    assert gtnet.socket == success_sock


@patch("pybennu.gtnet_skt.sleep")
@patch("socket.socket")
def test_tcp_send_failure_triggers_reconnect_and_resend(mock_socket_cls, mock_sleep):
    """Verify sendall failure resets socket, reconnects, and retries until success."""
    sock_fail = MagicMock()
    sock_fail.sendall.side_effect = BrokenPipeError("Socket disconnected")

    sock_ok = MagicMock()

    mock_socket_cls.side_effect = [sock_fail, sock_ok]

    tags = [make_tag("BRK1", "int", 1)]
    conf = make_settings(tags, protocol="tcp", tcp_retry_delay=0.05)
    gtnet = GtnetSkt(conf)

    gtnet.tcp_write_state()

    # sock_fail failed on sendall, then sock_ok succeeded
    assert sock_fail.sendall.call_count == 1
    assert sock_ok.sendall.call_count == 1
    assert gtnet.socket == sock_ok


# =============================================================================
# UDP Socket Tests
# =============================================================================


@patch("socket.socket")
def test_udp_socket_initialization(mock_socket_cls):
    """Verify UDP initialization creates SOCK_DGRAM socket without connect."""
    mock_sock = MagicMock()
    mock_socket_cls.return_value = mock_sock

    tags = [make_tag("BRK1", "int", 1)]
    conf = make_settings(tags, protocol="udp", ip="239.0.0.1", port=40000)
    gtnet = GtnetSkt(conf)
    gtnet.init_socket()

    mock_socket_cls.assert_called_with(socket.AF_INET, socket.SOCK_DGRAM)
    mock_sock.connect.assert_not_called()
    assert gtnet.socket == mock_sock


def test_close_socket_idempotence():
    """Verify close_socket safely cleans up open socket or does nothing if None."""
    tags = [make_tag("BRK1", "int", 1)]
    conf = make_settings(tags)
    gtnet = GtnetSkt(conf)

    # If None, should not raise
    gtnet.socket = None
    gtnet.close_socket()
    assert gtnet.socket is None

    # If active socket, close is called
    mock_sock = MagicMock()
    gtnet.socket = mock_sock
    gtnet.close_socket()
    mock_sock.close.assert_called_once()
    assert gtnet.socket is None
