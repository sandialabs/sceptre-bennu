"""Tests for the Speedgoat provider that need no SSH or MATLAB.

Tag handling is exercised on an instance built without __init__ (which would
prompt for credentials and connect). The startup guard is tested directly: the
provider must refuse to start without a terminal, since credentials are
interactive by design.
"""

import io
import sys
from unittest.mock import MagicMock

import pytest

# paramiko is a runtime dependency; the CI unit-test step installs only pytest,
# pyzmq and loguru, so stub it like the other optional solver dependencies.
sys.modules.setdefault("paramiko", MagicMock())

from pybennu.providers.power.solvers.speedgoat import speedgoat  # noqa: E402
from pybennu.providers.power.solvers.speedgoat.speedgoat import (  # noqa: E402
    SpeedgoatProvider,
)


class FakeStdin(io.StringIO):
    def __init__(self, tty):
        super().__init__()
        self._tty = tty

    def isatty(self):
        return self._tty


@pytest.fixture
def provider():
    """A SpeedgoatProvider with no SSH session; MATLAB replies are scripted."""

    p = object.__new__(SpeedgoatProvider)
    p.matlab_host = "10.0.0.50"
    p.publish_points = []
    p.current_values = {}
    p.debug = False
    p.replies = []
    p.sent = []

    def send(cmd, timeout=10):
        p.sent.append(cmd)
        reply = p.replies.pop(0)
        if isinstance(reply, Exception):
            raise reply
        return reply

    p.send_matlab_command = send
    return p


class TestPublishPoints:
    def test_skips_blank_lines_and_comments(self, provider, tmp_path):
        points = tmp_path / "publishPoints.txt"
        points.write_text("# header\nbus1_voltage\n\n  gen1_mw  \n")

        provider.load_publish_points(str(points))

        assert provider.publish_points == ["bus1_voltage", "gen1_mw"]

    @pytest.mark.parametrize("path", [None, "/does/not/exist.txt"])
    def test_missing_file_loads_nothing(self, provider, path):
        provider.load_publish_points(path)
        assert provider.publish_points == []


class TestQuery:
    def test_lists_publish_points(self, provider):
        provider.publish_points = ["a", "b"]
        assert provider.query() == "ACK=a,b"

    def test_errors_without_points(self, provider):
        assert provider.query().startswith("ERR=")


class TestRead:
    @pytest.mark.parametrize(
        ("reply", "expected"),
        [
            ("ACK=1.5", "ACK=1.5"),
            ("ERR=no such tag", "ERR=no such tag"),
            ("  1.5 \n", "ACK=1.5"),  # raw value from the listener gets wrapped
        ],
    )
    def test_reply_handling(self, provider, reply, expected):
        provider.replies = [reply]
        assert provider.read("bus1_voltage") == expected
        assert provider.sent == ["READ bus1_voltage"]

    def test_failure_is_an_err(self, provider):
        provider.replies = [RuntimeError("ssh dropped")]
        assert provider.read("bus1_voltage").startswith("ERR=")


class TestWrite:
    def test_writes_every_tag(self, provider):
        provider.replies = ["ACK=", "ACK="]
        assert provider.write({"a": "true", "b": 2.5}) == "ACK=Wrote 2 tags"
        assert provider.sent == ["WRITE a true", "WRITE b 2.5"]

    def test_stops_at_the_first_failure(self, provider):
        provider.replies = ["ERR=read only", "ACK="]
        assert provider.write({"a": 1, "b": 2}).startswith("ERR=Write failed for tag a")
        assert provider.sent == ["WRITE a 1"]

    def test_exception_is_an_err(self, provider):
        provider.replies = [RuntimeError("ssh dropped")]
        assert provider.write({"a": 1}).startswith("ERR=Write exception")


class TestInteractiveStartup:
    """Credentials are typed by an operator on purpose; see speedgoat.py."""

    def test_refuses_to_start_without_a_terminal(self, provider, monkeypatch):
        monkeypatch.setattr(speedgoat.sys, "stdin", FakeStdin(tty=False))

        with pytest.raises(RuntimeError, match="start it manually"):
            provider._require_interactive_terminal()

    def test_starts_with_a_terminal(self, provider, monkeypatch):
        monkeypatch.setattr(speedgoat.sys, "stdin", FakeStdin(tty=True))
        provider._require_interactive_terminal()

    def test_daemon_start_never_prompts_or_connects(self, monkeypatch, tmp_path):
        """As a daemon (stdin on /dev/null) it fails before any prompt or SSH."""

        monkeypatch.setattr(speedgoat.sys, "stdin", FakeStdin(tty=False))

        def forbidden(*_a, **_k):
            raise AssertionError("must not prompt or connect without a terminal")

        monkeypatch.setattr("builtins.input", forbidden)
        monkeypatch.setattr(speedgoat.getpass, "getpass", forbidden)
        monkeypatch.setattr(speedgoat.paramiko, "SSHClient", forbidden)

        points = tmp_path / "publishPoints.txt"
        points.write_text("a\n")

        with pytest.raises(RuntimeError, match="start it manually"):
            SpeedgoatProvider("tcp://x", "udp://y", "10.0.0.50", "/m.slx", str(points))

    def test_password_prompt_gives_up_after_three_tries(self, provider, monkeypatch):
        provider.username = "operator"
        monkeypatch.setattr(speedgoat.getpass, "getpass", lambda _prompt: "")

        with pytest.raises(RuntimeError, match="after 3 attempts"):
            provider._prompt_for_password()
