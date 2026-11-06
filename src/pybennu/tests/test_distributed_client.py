from unittest.mock import MagicMock, patch

import pytest

# conftest.py stubs the swig module when the compiled extension can't be imported;
# either way (real or stub), the fixture below patches client.E so these tests
# don't depend on which one is loaded.
from pybennu.distributed.client import Client


@pytest.fixture
def mock_client():
    with (
        patch("zmq.Context") as mock_ctx_cls,
        patch("pybennu.distributed.client.E") as mock_e,
    ):
        mock_e.Endpoint_str_get.side_effect = lambda ep: ep
        mock_ctx = MagicMock()
        mock_socket = MagicMock()
        mock_ctx.socket.return_value = mock_socket
        mock_ctx_cls.return_value = mock_ctx

        client = Client("tcp://127.0.0.1:5555")
        yield client, mock_socket


# =============================================================================
# Client Connection & Send Tests
# =============================================================================


def test_client_connect(mock_client):
    _client, mock_socket = mock_client
    mock_socket.setsockopt.assert_called_once()
    mock_socket.connect.assert_called_once_with("tcp://127.0.0.1:5555")


def test_client_send_ack(mock_client):
    client, mock_socket = mock_client
    mock_socket.recv_string.return_value = "ACK=result_data"

    handler = MagicMock()
    client.reply_handler = handler

    client.send("QUERY=")

    mock_socket.send_string.assert_called_once_with("QUERY=\0")
    handler.assert_called_once_with("result_data")


def test_client_send_err(mock_client):
    client, mock_socket = mock_client
    mock_socket.recv_string.return_value = "ERR=Something went wrong"

    handler = MagicMock()
    client.reply_handler = handler

    client.send("BAD=")

    mock_socket.send_string.assert_called_once_with("BAD=\0")
    handler.assert_not_called()


def test_client_send_malformed_no_equals(mock_client):
    client, mock_socket = mock_client
    mock_socket.recv_string.return_value = "MALFORMED_RESPONSE_WITHOUT_EQUALS"

    handler = MagicMock()
    client.reply_handler = handler

    client.send("TEST=")
    handler.assert_not_called()


def test_client_default_reply_handler(mock_client):
    client, mock_socket = mock_client
    mock_socket.recv_string.return_value = "ACK=data"
    # Default handler logs without raising any exception
    client.send("TEST=")
    mock_socket.send_string.assert_called_once_with("TEST=\0")


# =============================================================================
# Write Point Methods Tests
# =============================================================================


def test_write_analog_point(mock_client):
    client, mock_socket = mock_client
    mock_socket.recv_string.return_value = "ACK="

    client.write_analog_point("load-mw", 400.5)
    mock_socket.send_string.assert_called_once_with("WRITE=load-mw:400.5\0")


def test_write_digital_point(mock_client):
    client, mock_socket = mock_client
    mock_socket.recv_string.return_value = "ACK="

    # True -> "true"
    client.write_digital_point("brkr", True)
    mock_socket.send_string.assert_called_with("WRITE=brkr:true\0")

    # False -> "false"
    client.write_digital_point("brkr", False)
    mock_socket.send_string.assert_called_with("WRITE=brkr:false\0")


def test_write_points_batch(mock_client):
    client, mock_socket = mock_client
    mock_socket.recv_string.return_value = "ACK="

    points = {
        "brkr1": True,
        "brkr2": False,
        "power": 12.5,
    }
    client.write_points(points)

    mock_socket.send_string.assert_called_once()
    sent_msg = mock_socket.send_string.call_args[0][0]
    assert sent_msg.startswith("WRITE=")
    assert sent_msg.endswith("\0")

    payload = sent_msg[len("WRITE=") : -1]
    tags = dict(item.split(":") for item in payload.split(","))
    assert tags["brkr1"] == "true"
    assert tags["brkr2"] == "false"
    assert tags["power"] == "12.5"


def test_write_points_empty(mock_client):
    client, mock_socket = mock_client

    client.write_points({})
    mock_socket.send_string.assert_not_called()
