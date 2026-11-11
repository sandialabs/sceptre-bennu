import threading
import types
from datetime import datetime, timedelta, timezone
from unittest.mock import patch

import pytest

import pybennu.executables.groundtruth_monitor as gtm


@pytest.fixture(autouse=True)
def _no_swig(monkeypatch):
    """Endpoint is a compiled swig type; the handler only needs its string form."""
    monkeypatch.setattr(
        gtm, "E", types.SimpleNamespace(Endpoint_str_get=lambda ep: "udp://test:1")
    )


def make_monitor():
    """A monitor with no daemon, subscriber, elasticsearch or push thread."""
    mon = object.__new__(gtm.GroundTruthMonitor)
    mon.publish_endpoint = "mock_endpoint"
    mon._filter = "/"
    mon.new_data = []
    mon._GroundTruthMonitor__lock = threading.Lock()
    mon.time_last_send = datetime.now(timezone.utc)
    return mon


def fields(mon):
    return [(d["device"], d["field"]) for d in mon.new_data]


def test_rate_limited_alert_keeps_other_points():
    """An alert/batch point inside the 30 s window must not drop its neighbours."""
    mon = make_monitor()
    mon._subscription_handler("alert-1.state:1,bus-1.v:1.0,bus-2.v:2.0,")
    assert fields(mon) == [("bus-1", "v"), ("bus-2", "v")]


def test_alert_passes_after_window_and_resets_it():
    mon = make_monitor()
    mon.time_last_send = datetime.now(timezone.utc) - timedelta(seconds=31)
    mon._subscription_handler("alert-1.state:1,bus-1.v:1.0,")
    assert fields(mon) == [("alert-1", "state"), ("bus-1", "v")]
    # the window restarted: the next alert is limited, the plain point is not
    mon._subscription_handler("alert-1.state:1,bus-1.v:2.0,")
    assert fields(mon)[2:] == [("bus-1", "v")]


def test_rate_limit_window_longer_than_a_day():
    """timedelta.seconds wraps every 24 h; total_seconds does not."""
    mon = make_monitor()
    mon.time_last_send = datetime.now(timezone.utc) - timedelta(days=1, seconds=5)
    mon._subscription_handler("alert-1.state:1,")
    assert fields(mon) == [("alert-1", "state")]


def test_timestamp_is_utc():
    mon = make_monitor()
    mon._subscription_handler("bus-1.v:1.0,")
    ts = mon.new_data[0]["@timestamp"]
    assert ts.utcoffset() == timedelta(0)


def test_index_name_uses_utc_date():
    """Index is bennu-<UTC date> even when the local date differs."""
    mon = make_monitor()
    mon._subscription_handler("bus-1.v:1.0,")
    seen = {}

    def bulk(_es, actions, **kw):
        seen["actions"] = actions
        raise SystemExit  # leave the push loop

    mon._GroundTruthMonitor__es = None
    with patch.object(gtm.helpers, "bulk", bulk):
        try:
            mon.push_updates_to_elastic()
        except SystemExit:
            pass
    want = "bennu-" + datetime.now(timezone.utc).strftime("%Y.%m.%d")
    assert seen["actions"][0]["_index"] == want
