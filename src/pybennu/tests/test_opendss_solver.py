"""End-to-end check of the OpenDSS solver against the installed opendssdirect.py.

The solver uses the legacy callable API (dss.Generators.kW(x), dss.Transformers.Wdg(n), ...), which
opendssdirect.py 0.8/0.9 deprecate/reshape, so a version bump must prove this still works.
"""

import pytest

pytest.importorskip("opendssdirect")

try:  # the SWIG endpoint extension is only present in a built tree; the solver never uses it here
    import pybennu.distributed.swig._Endpoint  # noqa: F401
except ImportError:
    import sys
    from unittest import mock

    sys.modules["pybennu.distributed.swig._Endpoint"] = mock.MagicMock()

from pybennu.distributed.provider import Provider
from pybennu.providers.power.solvers.opendss import OpenDSS

CIRCUIT = """\
clear
New Circuit.t basekv=138 bus1=src pu=1.02 R1=1.2 X1=7 R0=2 X0=10
New Transformer.sub phases=3 windings=2 buses=(src.1.2.3,lv.1.2.3.0) conns=(delta,wye) kvs=(138,12.47) kvas=(20000,20000) xhl=7.5 numtaps=16
New Line.l1 phases=3 bus1=lv bus2=ld length=1 units=km r1=0.3 x1=0.15 r0=1 x0=0.5 c1=0 c0=0
New Generator.g1 phases=3 bus1=ld kv=12.47 kw=1000 pf=1
New Load.ld1 phases=3 bus1=ld kv=12.47 kw=500 pf=0.95
New Capacitor.cap1 phases=3 bus1=ld kv=12.47 kvar=300
set voltagebases=[138 12.47]
calcv
"""


@pytest.fixture
def solver(tmp_path, monkeypatch):
    monkeypatch.setattr(Provider, "__init__", lambda self, *a, **k: None)  # no sockets
    dss_file = tmp_path / "ckt.dss"
    dss_file.write_text(CIRCUIT)
    return OpenDSS(
        "", "", str(dss_file), debug=False, clear_circuit="disabled", jitter="disabled"
    )


def _ack(reply):
    assert reply.startswith("ACK="), reply
    return reply[4:]


def _val(solver, tag):
    return _ack(solver.read(tag))


def test_create_and_query(solver):
    tags = _ack(solver.query()).split(",")
    assert any(t.startswith("generator_g1.") for t in tags)
    assert any(t.startswith("load_ld1.") for t in tags)
    assert any(t.startswith("transformer_sub.") for t in tags)
    assert any(t.startswith("shunt_cap1.") for t in tags)
    assert any(t.startswith("branch_l1.") for t in tags)


def test_write_resolves_and_reads_back(solver):
    assert solver.write({"generator_g1.active_power_output": "2000"}).startswith("ACK")
    assert float(_val(solver, "generator_g1.active_power_output")) == pytest.approx(
        2000
    )
    assert solver.write({"load_ld1.active": "false"}).startswith("ACK")
    assert _val(solver, "load_ld1.active") == "False"
    assert solver.write({"transformer_sub.tap_setting_wdg2": "1.05"}).startswith("ACK")
    assert float(_val(solver, "transformer_sub.tap_setting_wdg2")) == pytest.approx(
        1.05
    )
    assert solver.write({"shunt_cap1.active": "false"}).startswith("ACK")
    assert _val(solver, "shunt_cap1.active") == "False"
    assert solver.write({"branch_l1.active": "false"}).startswith("ACK")
    assert _val(solver, "branch_l1.active") == "False"
