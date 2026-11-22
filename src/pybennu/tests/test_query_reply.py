import sys
import threading
from unittest.mock import MagicMock

sys.modules.setdefault("pypower", MagicMock())
sys.modules.setdefault("pypower.api", MagicMock())

import pybennu.providers.power.solvers.python_power as pp  # noqa: E402
from pybennu.executables.pybennu_test_ep_server import ElectricPowerService  # noqa: E402
from pybennu.providers.power.solvers.generic_python.generic_python import (  # noqa: E402
    GenericPython,
)
from pybennu.providers.power.solvers.pwds.power_world_dynamics import PowerWorldDynamics  # noqa: E402


def ask_query(provider):
    return provider._Provider__message_handler("QUERY=")


def test_python_power_query_has_no_trailing_comma():
    solver = pp.PyPower.__new__(pp.PyPower)
    solver.debug = False
    solver._PyPower__lock = threading.Lock()
    solver.elements = {
        "bus": {"bus-1": {"v": 1, "ang": 0}},
        "gen": {"gen-2": {"mw": 5}},
    }

    assert ask_query(solver) == "ACK=bus-1.v,bus-1.ang,gen-2.mw"


def test_generic_python_query_has_no_trailing_comma():
    solver = GenericPython.__new__(GenericPython)
    solver.debug = False
    solver._GenericPython__lock = threading.Lock()
    solver.elements = {
        "inputs_to_fd": {"analog": {"a": 1}, "binary": {}},
        "outputs_from_fd": {"analog": {"b": 2}, "binary": {"c": True}},
    }

    assert ask_query(solver) == "ACK=a,b,c"


def test_test_ep_server_query_has_no_trailing_comma():
    svc = ElectricPowerService.__new__(ElectricPowerService)
    svc._ElectricPowerService__debug = False
    svc.mPS = {"a.x": "1", "b.y": "2"}

    assert ask_query(svc) == "ACK=a.x,b.y"


def test_pwds_query_has_no_trailing_comma():
    svc = PowerWorldDynamics.__new__(PowerWorldDynamics)
    svc._PowerWorldDynamics__debug = False
    svc.tags = ["bus-1.number", "bus-1.active"]

    assert ask_query(svc) == "ACK=bus-1.number,bus-1.active"
