import signal
from unittest.mock import patch

import pytest

import pybennu.executables.groundtruth_monitor as gtm


def test_sigterm_exits_cleanly():
    """SIGTERM must end Subscriber.run with SystemExit(0), not AttributeError."""
    mon = object.__new__(gtm.GroundTruthMonitor)
    handlers = {}

    def run():
        handlers[signal.SIGTERM](signal.SIGTERM, None)  # signal arrives mid-recv

    with (
        patch.object(gtm.signal, "signal", lambda s, h: handlers.__setitem__(s, h)),
        patch.object(gtm.Subscriber, "run", lambda self: run(), create=True),
    ):
        with pytest.raises(SystemExit) as exc:
            mon._run()
    assert exc.value.code == 0
    assert signal.SIGINT in handlers
