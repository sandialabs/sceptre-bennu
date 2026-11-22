import base64
import dataclasses
import json
import sys
from unittest.mock import MagicMock

for name in ("opendssdirect", "opendssdirect.utils"):
    sys.modules.setdefault(name, MagicMock())

import pybennu.providers.power.solvers.opendss as opendss  # noqa: E402
from pybennu.providers.power.solvers.opendss import models  # noqa: E402


def test_batched_write_applies_every_device(monkeypatch):
    dss = MagicMock()
    monkeypatch.setattr(opendss, "dss", dss)

    solver = opendss.OpenDSS.__new__(opendss.OpenDSS)
    solver._OpenDSS__debug = False
    solver._OpenDSS__compile_circuit = MagicMock()
    solver._OpenDSS__solve = MagicMock()
    solver.elements = {}
    for n in ("load_a", "load_b"):
        load = models.LoadData.__new__(models.LoadData)
        load.name, load.active = n, True
        solver.elements[n] = load

    resp = solver.write({"load_a.active": "false", "load_b.active": "false"})

    assert resp.startswith("ACK=")
    dss.Circuit.SetActiveElement.assert_any_call("a")
    dss.Circuit.SetActiveElement.assert_any_call("b")
    assert dss.Circuit.Disable.call_count == 2
    solver._OpenDSS__solve.assert_called_once()


def _solver_with_breaker():
    solver = opendss.OpenDSS.__new__(opendss.OpenDSS)
    solver._OpenDSS__debug = False
    solver._OpenDSS__lock = __import__("threading").Lock()
    solver._OpenDSS__gen_rand_num = lambda: 1.0
    load = models.LoadData(
        **{
            f.name: ""
            for f in dataclasses.fields(models.LoadData)
            if f.default is dataclasses.MISSING
        }
    )
    load.name, load.active = "load_a", True
    load.phases = base64.b64encode(
        json.dumps(dict.fromkeys(["p1", "p2", "p3", "pn"], False)).encode()
    ).decode()
    solver.elements = {"load_a": load}
    return solver


def test_publish_and_read_use_lowercase_booleans():
    solver = _solver_with_breaker()

    assert "load_a.active:true" in solver._OpenDSS__pack_message()
    assert solver.read("load_a.active") == "ACK=true"
    solver.elements["load_a"].active = False
    assert solver.read("load_a.active") == "ACK=false"
