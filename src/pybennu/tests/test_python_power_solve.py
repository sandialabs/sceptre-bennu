import sys
from unittest.mock import MagicMock

sys.modules.setdefault("pypower", MagicMock())
sys.modules.setdefault("pypower.api", MagicMock())

import pybennu.providers.power.solvers.python_power as pp  # noqa: E402


def solve_options(monkeypatch, debug):
    seen = {}

    def runpf(system, options):
        seen.update(options)
        return system, False

    monkeypatch.setattr(pp, "ppoption", lambda **kw: dict(kw))
    monkeypatch.setattr(pp, "runpf", runpf)
    solver = pp.PyPower.__new__(pp.PyPower)
    solver.debug = debug
    solver.system = {}
    solver.elements = {"bus": {"bus-alert": {"active": False}}}
    solver._PyPower__solve()
    return seen


def test_solve_report_only_when_debugging(monkeypatch):
    assert solve_options(monkeypatch, debug=False)["OUT_ALL"] == 0
    assert solve_options(monkeypatch, debug=True)["OUT_ALL"] == 1
