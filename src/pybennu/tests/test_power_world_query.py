import sys
from unittest.mock import MagicMock

for name in ("pythoncom", "win32com", "win32com.client"):
    sys.modules.setdefault(name, MagicMock())

from pybennu.providers.power.solvers.power_world import PowerWorld  # noqa: E402


def test_query_separates_elements():
    solver = PowerWorld.__new__(PowerWorld)
    solver.debug = False
    solver.need = [("bus-1", "bus"), ("gen-2", "gen")]
    solver.scada_fields = {"bus": ["v", "ang"], "gen": ["mw"]}

    assert solver.query() == "ACK=bus-1.v,bus-1.ang,gen-2.mw"
