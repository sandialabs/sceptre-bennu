import importlib.util
import pathlib
import sys
import types

import pytest


def _load_server(monkeypatch):
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
    spec = importlib.util.spec_from_file_location("server_real", path)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


def test_handler_exception_replies_err_and_keeps_serving(monkeypatch):
    mod = _load_server(monkeypatch)
    sent = []

    class Sock:
        reqs = iter(["boom", "ok"])

        def recv_string(self):
            try:
                return next(self.reqs)
            except StopIteration:
                raise mod.zmq.ZMQError from None

        def send_string(self, s):
            sent.append(s)

    srv = mod.Server.__new__(mod.Server)
    srv._Server__socket = Sock()

    def handler(r):
        if r == "boom":
            raise ValueError
        return "ACK="

    srv.request_handler = handler
    with pytest.raises(mod.zmq.ZMQError):
        srv.run()
    assert sent == ["ERR=Internal error\x00", "ACK=\x00"]
