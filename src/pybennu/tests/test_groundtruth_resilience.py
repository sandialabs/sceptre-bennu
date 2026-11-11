import threading
import time
import types
from datetime import datetime
from unittest.mock import patch

import pytest

import pybennu.executables.groundtruth_monitor as gtm


@pytest.fixture(autouse=True)
def _no_swig(monkeypatch):
    """Endpoint is a compiled swig type; the handler only needs its string form."""
    monkeypatch.setattr(
        gtm, "E", types.SimpleNamespace(Endpoint_str_get=lambda ep: "udp://test:1")
    )


def make_monitor(max_pending=1000):
    """A monitor with no daemon, subscriber or elasticsearch; the caller runs the pusher."""
    mon = object.__new__(gtm.GroundTruthMonitor)
    mon.MAX_PENDING = max_pending
    mon.publish_endpoint = "mock_endpoint"
    mon._filter = "/"
    mon.time_last_send = datetime.now()
    mon.new_data = []
    mon._GroundTruthMonitor__lock = threading.Lock()
    mon._GroundTruthMonitor__es = None
    return mon


def run_pusher(mon):
    t = threading.Thread(target=mon.push_updates_to_elastic, daemon=True)
    t.start()
    return t


def wait_for(cond, timeout=5):
    end = time.time() + timeout
    while time.time() < end:
        if cond():
            return True
        time.sleep(0.01)
    return False


def feed(mon, n, start=0):
    for i in range(start, start + n):
        mon._subscription_handler(f"dev-{i}.v:{i}.0,")


def test_pending_queue_is_bounded_and_keeps_newest():
    mon = make_monitor(max_pending=10)
    feed(mon, 25)
    assert len(mon.new_data) == 10
    assert next(d["device"] for d in mon.new_data) == "dev-15"


def test_failed_bulk_is_retried_and_nothing_is_lost(monkeypatch):
    monkeypatch.setattr(gtm.GroundTruthMonitor, "RETRY_MIN", 0.01)
    mon = make_monitor()
    sent, calls = [], []

    def bulk(_es, actions, **kw):
        calls.append(1)
        if len(calls) <= 2:
            raise ConnectionError("elasticsearch is down")
        sent.extend(a["_source"]["device"] for a in actions)
        return len(actions), []

    with patch.object(gtm.helpers, "bulk", bulk):
        feed(mon, 3)
        run_pusher(mon)
        assert wait_for(lambda: len(sent) == 3)
        feed(mon, 2, start=3)
        assert wait_for(lambda: len(sent) == 5)
    assert sent == [f"dev-{i}" for i in range(5)]  # in order, no duplicates
    assert len(calls) >= 3


def test_points_arriving_during_an_outage_are_kept_in_order(monkeypatch):
    monkeypatch.setattr(gtm.GroundTruthMonitor, "RETRY_MIN", 0.05)
    mon = make_monitor()
    sent, down = [], [True]

    def bulk(_es, actions, **kw):
        if down[0]:
            raise ConnectionError("elasticsearch is down")
        sent.extend(a["_source"]["device"] for a in actions)
        return len(actions), []

    with patch.object(gtm.helpers, "bulk", bulk):
        feed(mon, 2)
        run_pusher(mon)
        time.sleep(0.1)
        feed(mon, 2, start=2)
        down[0] = False
        assert wait_for(lambda: len(sent) == 4)
    assert sent == [f"dev-{i}" for i in range(4)]


def test_rejected_documents_are_not_retried():
    mon = make_monitor()
    calls = []

    def bulk(_es, actions, **kw):
        calls.append(1)
        return 0, [{"index": {"status": 400}}]

    with patch.object(gtm.helpers, "bulk", bulk):
        feed(mon, 1)
        run_pusher(mon)
        assert wait_for(lambda: calls)
        time.sleep(0.2)
    assert len(calls) == 1
    assert not mon.new_data


def test_backoff_doubles_up_to_the_maximum(monkeypatch):
    monkeypatch.setattr(gtm.GroundTruthMonitor, "RETRY_MIN", 1)
    monkeypatch.setattr(gtm.GroundTruthMonitor, "RETRY_MAX", 4)
    mon = make_monitor()
    sleeps = []

    class Stop(Exception):
        pass

    def sleep(s):
        sleeps.append(s)
        if len(sleeps) == 5:
            raise Stop

    def bulk(_es, actions, **kw):
        raise ConnectionError

    feed(mon, 1)
    with (
        patch.object(gtm.helpers, "bulk", bulk),
        patch.object(gtm.time, "sleep", sleep),
    ):
        with pytest.raises(Stop):
            mon.push_updates_to_elastic()
    assert sleeps == [1, 2, 4, 4, 4]


def test_retry_after_a_partial_bulk_does_not_duplicate_documents(monkeypatch):
    """A bulk that dies after its first chunk is retried whole; docs must overwrite by _id."""
    monkeypatch.setattr(gtm.GroundTruthMonitor, "RETRY_MIN", 0.01)
    mon = make_monitor()
    store, calls, chunk = {}, [], 2

    def bulk(_es, actions, **kw):
        calls.append(1)
        for i, a in enumerate(actions):
            if len(calls) == 1 and i == chunk:  # second chunk fails on the first try
                raise ConnectionError("died mid-bulk")
            store[a["_id"]] = a["_source"]  # an index action with the same _id replaces
        return len(actions), []

    with patch.object(gtm.helpers, "bulk", bulk):
        feed(mon, 4)
        run_pusher(mon)
        assert wait_for(lambda: len(calls) >= 2)
        time.sleep(0.1)
    assert len(calls) == 2
    assert sorted(d["device"] for d in store.values()) == [f"dev-{i}" for i in range(4)]
