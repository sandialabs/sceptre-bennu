import importlib.util
import pathlib
import sys
import types
from unittest.mock import MagicMock

import pytest

from pybennu.distributed.provider import Provider

# =============================================================================
# Helper Provider Stub
# =============================================================================


class ResilientStubProvider(Provider):
    def __init__(self):
        super().__init__("tcp://127.0.0.1:5555", "udp://*;239.0.0.1:40000")
        self.writes = []

    def query(self):
        return "ACK=ok"

    def read(self, tag):
        return f"ACK={tag}:1"

    def write(self, tags):
        self.writes.append(tags)
        return "ACK=ok"

    def periodic_publish(self):
        pass


@pytest.fixture
def provider():
    p = ResilientStubProvider()
    return p, p._Provider__message_handler


# =============================================================================
# 1. Large Batched Writes (Commit 62822a1 & Batch Scaling)
# =============================================================================


def test_large_batched_write_800_tags(provider):
    p, handle = provider
    tag_count = 800
    expected_tags = {f"device_{i}_val": f"{i * 0.125:.3f}" for i in range(tag_count)}

    # Build payload with trailing comma
    payload_pairs = [f"{k}:{v}" for k, v in expected_tags.items()]
    request = "WRITE=" + ",".join(payload_pairs) + ","

    resp = handle(request)
    assert resp == "ACK=ok"
    assert len(p.writes) == 1
    received = p.writes[0]

    assert len(received) == tag_count
    for k, v in expected_tags.items():
        assert k in received
        assert received[k] == v


def test_batched_write_opendss_applies_all_devices(monkeypatch):
    """Regression test for commit d0774bb: verify all elements are updated before solve."""
    # Ensure opendssdirect modules are stubbed
    for name in ("opendssdirect", "opendssdirect.utils"):
        sys.modules.setdefault(name, MagicMock())

    import pybennu.providers.power.solvers.opendss as opendss
    from pybennu.providers.power.solvers.opendss import models

    dss = MagicMock()
    monkeypatch.setattr(opendss, "dss", dss)

    solver = opendss.OpenDSS.__new__(opendss.OpenDSS)
    solver._OpenDSS__debug = False
    solver._OpenDSS__compile_circuit = MagicMock()
    solver._OpenDSS__solve = MagicMock()
    solver.elements = {}

    tags_to_write = {}
    for i in range(10):
        name = f"load_{i}"
        load = models.LoadData.__new__(models.LoadData)
        load.name, load.active = name, True
        solver.elements[name] = load
        tags_to_write[f"{name}.active"] = "false"

    resp = solver.write(tags_to_write)
    assert resp.startswith("ACK=")

    # Verify every element was updated
    for i in range(10):
        dss.Circuit.SetActiveElement.assert_any_call(str(i))
    assert dss.Circuit.Disable.call_count == 10
    solver._OpenDSS__solve.assert_called_once()


# =============================================================================
# 2. Complex Tag Values with Special Characters
# =============================================================================


def test_complex_tag_values_special_characters(provider):
    p, handle = provider
    complex_tags = {
        "formula": "x=y=z",
        "nested_eq": "a=1=b=2",
        "url_endpoint": "http://10.0.0.1:8080/api/v1/resource",
        "ratio": "16:9:1",
        "filepath": "/var/log/bennu/daemon.log",
        "sci_pos": "1.23e-4",
        "sci_neg": "-5.67E-10",
        "json_like": "{key:value}",
    }

    pairs = [f"{k}:{v}" for k, v in complex_tags.items()]
    request = "WRITE=" + ",".join(pairs)

    resp = handle(request)
    assert resp == "ACK=ok"
    assert len(p.writes) == 1
    received = p.writes[0]

    for k, v in complex_tags.items():
        assert k in received
        assert received[k] == v


# =============================================================================
# 3. Server Error Recovery & Isolation
# =============================================================================


def _load_real_server(monkeypatch):
    class ZMQError(Exception):
        pass

    zmq = types.ModuleType("zmq")
    zmq.ZMQError, zmq.REP = ZMQError, 0
    swig = types.ModuleType("pybennu.distributed.swig._Endpoint")
    for m in (zmq, swig):
        monkeypatch.setitem(sys.modules, m.__name__, m)
    parent = types.ModuleType("pybennu.distributed.swig")
    parent._Endpoint = swig
    monkeypatch.setitem(sys.modules, "pybennu.distributed.swig", parent)
    path = pathlib.Path(__file__).parents[1] / "pybennu/distributed/server.py"
    spec = importlib.util.spec_from_file_location("server_real_resilience", path)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


def test_server_error_recovery_keeps_serving(monkeypatch):
    mod = _load_real_server(monkeypatch)
    sent_replies = []

    class MockSocket:
        requests = iter(["bad_1", "good_1", "bad_2", "good_2"])

        def recv_string(self):
            try:
                return next(self.requests)
            except StopIteration:
                raise mod.zmq.ZMQError from None

        def send_string(self, s):
            sent_replies.append(s)

    srv = mod.Server.__new__(mod.Server)
    srv._Server__socket = MockSocket()

    def resilient_handler(req):
        if req == "bad_1":
            raise ValueError("simulated bad value")
        if req == "bad_2":
            raise RuntimeError("unexpected handler failure")
        return f"ACK={req}"

    srv.request_handler = resilient_handler

    with pytest.raises(mod.zmq.ZMQError):
        srv.run()

    expected = [
        "ERR=Internal error\x00",
        "ACK=good_1\x00",
        "ERR=Internal error\x00",
        "ACK=good_2\x00",
    ]
    assert sent_replies == expected
