import threading
import types
from datetime import datetime

import pytest

import pybennu.executables.groundtruth_monitor as gtm


@pytest.fixture(autouse=True)
def _no_swig(monkeypatch):
    monkeypatch.setattr(
        gtm, "E", types.SimpleNamespace(Endpoint_str_get=lambda ep: "udp://test:1")
    )


def make_monitor(filter_):
    mon = object.__new__(gtm.GroundTruthMonitor)
    mon.publish_endpoint = "mock_endpoint"
    mon._filter = filter_
    mon.time_last_send = datetime.now()
    mon.new_data = []
    mon._GroundTruthMonitor__lock = threading.Lock()
    return mon


def test_filter_is_parsed_once_not_per_message():
    mon = make_monitor("/udp/bus-1/voltage /udp/bus-2/")
    mon._subscription_handler("bus-1.voltage:1.0,")
    devices = mon.devices
    assert devices == ["bus-1", "bus-2"]
    assert mon.fields == ["voltage", ""]
    mon._subscription_handler("bus-3.voltage:2.0,bus-2.x:3,")
    assert mon.devices is devices  # not rebuilt
    assert [d["device"] for d in mon.new_data] == ["bus-1", "bus-2"]


def test_a_changed_filter_is_parsed_again():
    mon = make_monitor("/udp/bus-1/")
    mon._subscription_handler("bus-1.v:1,bus-2.v:1,")
    mon._filter = "/udp/bus-2/"
    mon._subscription_handler("bus-1.v:1,bus-2.v:1,")
    assert [d["device"] for d in mon.new_data] == ["bus-1", "bus-2"]
