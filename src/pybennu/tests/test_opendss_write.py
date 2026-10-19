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
