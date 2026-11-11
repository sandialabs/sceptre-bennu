import sys
from unittest.mock import MagicMock, patch

import pytest

import pybennu.executables.groundtruth_monitor as gtm
import pybennu.executables.pybennu_probe as probe
import pybennu.executables.pybennu_test_ep_server as ep_server

# =============================================================================
# pybennu_test_ep_server Tests
# =============================================================================


def test_ep_server_help_option(monkeypatch, capsys):
    """Verify pybennu_test_ep_server --help prints usage and exits with code 0."""
    monkeypatch.setattr(sys, "argv", ["pybennu-test-ep-server", "--help"])
    with pytest.raises(SystemExit) as exc_info:
        ep_server.main()

    assert exc_info.value.code == 0
    captured = capsys.readouterr()
    assert "Electric power test work service" in captured.out
    assert "--server-endpoint" in captured.out
    assert "--publish-endpoint" in captured.out


def test_ep_server_short_help_option(monkeypatch, capsys):
    """Verify pybennu_test_ep_server -h exits with code 0."""
    monkeypatch.setattr(sys, "argv", ["pybennu-test-ep-server", "-h"])
    with pytest.raises(SystemExit) as exc_info:
        ep_server.main()

    assert exc_info.value.code == 0
    captured = capsys.readouterr()
    assert "Electric power test work service" in captured.out


def test_ep_server_invalid_argument(monkeypatch, capsys):
    """Verify pybennu_test_ep_server with invalid arguments exits with code 2."""
    monkeypatch.setattr(
        sys, "argv", ["pybennu-test-ep-server", "--unrecognized-option"]
    )
    with pytest.raises(SystemExit) as exc_info:
        ep_server.main()

    assert exc_info.value.code != 0
    captured = capsys.readouterr()
    assert "unrecognized arguments" in captured.err


@patch("pybennu.executables.pybennu_test_ep_server.ElectricPowerService")
def test_ep_server_custom_options(mock_service_cls, monkeypatch):
    """Verify custom command-line options configure server and publish endpoints."""
    mock_instance = MagicMock()
    mock_service_cls.return_value = mock_instance

    args = [
        "pybennu-test-ep-server",
        "-s",
        "tcp://10.0.0.1:5555",
        "-p",
        "udp://239.0.0.2:40001",
        "-d",
    ]
    monkeypatch.setattr(sys, "argv", args)

    ep_server.main()

    mock_service_cls.assert_called_once()
    assert mock_instance.run.called


# =============================================================================
# pybennu_probe Tests
# =============================================================================


def test_probe_help_option(monkeypatch, capsys):
    """Verify pybennu_probe --help prints usage and exits with code 0."""
    monkeypatch.setattr(sys, "argv", ["pybennu-probe", "--help"])
    with pytest.raises(SystemExit) as exc_info:
        probe.main()

    assert exc_info.value.code == 0
    captured = capsys.readouterr()
    assert "--endpoint" in captured.out
    assert "--command" in captured.out
    assert "--tag" in captured.out


def test_probe_invalid_argument(monkeypatch, capsys):
    """Verify pybennu_probe with invalid argument exits with non-zero code."""
    monkeypatch.setattr(sys, "argv", ["pybennu-probe", "--nonexistent-option"])
    with pytest.raises(SystemExit) as exc_info:
        probe.main()

    assert exc_info.value.code != 0
    captured = capsys.readouterr()
    assert "unrecognized arguments" in captured.err


def test_probe_invalid_command_choice(monkeypatch, capsys):
    """Verify pybennu_probe with invalid command choice exits with non-zero code."""
    monkeypatch.setattr(sys, "argv", ["pybennu-probe", "-c", "invalid_action"])
    with pytest.raises(SystemExit) as exc_info:
        probe.main()

    assert exc_info.value.code != 0
    captured = capsys.readouterr()
    assert "invalid choice: 'invalid_action'" in captured.err


def test_probe_mutually_exclusive_value_and_status(monkeypatch, capsys):
    """Verify passing both --value and --status violates mutual exclusion."""
    monkeypatch.setattr(
        sys,
        "argv",
        ["pybennu-probe", "-t", "tag1", "-v", "10", "-s", "true"],
    )
    with pytest.raises(SystemExit) as exc_info:
        probe.main()

    assert exc_info.value.code != 0
    captured = capsys.readouterr()
    assert "not allowed with argument" in captured.err


@patch("pybennu.executables.pybennu_probe.client.Client")
def test_probe_query_execution(mock_client_cls, monkeypatch):
    """Verify default query command constructs QUERY= message and sends via Client."""
    mock_client = MagicMock()
    mock_client_cls.return_value = mock_client

    monkeypatch.setattr(
        sys, "argv", ["pybennu-probe", "-e", "tcp://127.0.0.1:1330", "-c", "query"]
    )
    probe.main()

    mock_client.send.assert_called_once_with("QUERY=")


@patch("pybennu.executables.pybennu_probe.client.Client")
def test_probe_read_execution(mock_client_cls, monkeypatch):
    """Verify read command constructs READ=<tag> message."""
    mock_client = MagicMock()
    mock_client_cls.return_value = mock_client

    monkeypatch.setattr(
        sys, "argv", ["pybennu-probe", "-c", "read", "-t", "bus1.active"]
    )
    probe.main()

    mock_client.send.assert_called_once_with("READ=bus1.active")


@patch("pybennu.executables.pybennu_probe.client.Client")
def test_probe_write_execution(mock_client_cls, monkeypatch):
    """Verify write command formats boolean status into true/false string."""
    mock_client = MagicMock()
    mock_client_cls.return_value = mock_client

    monkeypatch.setattr(
        sys,
        "argv",
        ["pybennu-probe", "-c", "write", "-t", "br1.closed", "-s", "1"],
    )
    probe.main()

    mock_client.send.assert_called_once_with("WRITE=br1.closed:true")


def test_probe_reply_handler(capsys):
    """Verify reply handler prints comma-separated tags or single tag cleanly."""
    probe.handler("tag1,tag2,tag3")
    captured = capsys.readouterr()
    assert "tag1" in captured.out
    assert "tag2" in captured.out
    assert "tag3" in captured.out

    probe.handler("single_value")
    captured_single = capsys.readouterr()
    assert "single_value" in captured_single.out


# =============================================================================
# groundtruth_monitor Tests
# =============================================================================


def test_groundtruth_monitor_help_option(monkeypatch, capsys):
    """Verify groundtruth_monitor --help prints usage and exits with code 0."""
    monkeypatch.setattr(sys, "argv", ["groundtruth_monitor", "--help"])
    with pytest.raises(SystemExit) as exc_info:
        gtm.main()

    assert exc_info.value.code == 0
    captured = capsys.readouterr()
    assert "SCEPTRE Ground Truth Monitor" in captured.out
    assert "OPERATION" in captured.out
    assert "Daemon operation" in captured.out


def test_groundtruth_monitor_missing_operation(monkeypatch, capsys):
    """Verify missing required OPERATION positional argument exits with non-zero code."""
    monkeypatch.setattr(sys, "argv", ["groundtruth_monitor"])
    with pytest.raises(SystemExit) as exc_info:
        gtm.main()

    assert exc_info.value.code != 0
    captured = capsys.readouterr()
    assert "the following arguments are required: OPERATION" in captured.err


def test_groundtruth_monitor_invalid_operation(monkeypatch, capsys):
    """Verify invalid operation choice exits with non-zero code."""
    monkeypatch.setattr(sys, "argv", ["groundtruth_monitor", "destroy"])
    with pytest.raises(SystemExit) as exc_info:
        gtm.main()

    assert exc_info.value.code != 0
    captured = capsys.readouterr()
    assert "invalid choice: 'destroy'" in captured.err


@patch("pybennu.executables.groundtruth_monitor.GroundTruthMonitor")
def test_groundtruth_monitor_status_execution(mock_gtm_cls, monkeypatch, capsys):
    """Verify status operation queries daemon pid."""
    mock_instance = MagicMock()
    mock_instance.get_pid.return_value = 12345
    mock_gtm_cls.return_value = mock_instance

    monkeypatch.setattr(
        sys, "argv", ["groundtruth_monitor", "-e", "test_env", "status"]
    )
    with pytest.raises(SystemExit) as exc_info:
        gtm.main()

    assert exc_info.value.code == 0
    captured = capsys.readouterr()
    assert "Daemon is running as PID 12345" in captured.out


@patch("pybennu.executables.groundtruth_monitor.GroundTruthMonitor")
def test_groundtruth_monitor_stop_execution(mock_gtm_cls, monkeypatch):
    """Verify stop operation calls daemon stop."""
    mock_instance = MagicMock()
    mock_gtm_cls.return_value = mock_instance

    monkeypatch.setattr(sys, "argv", ["groundtruth_monitor", "stop"])
    with pytest.raises(SystemExit) as exc_info:
        gtm.main()

    assert exc_info.value.code == 0
    mock_instance.stop.assert_called_once()
