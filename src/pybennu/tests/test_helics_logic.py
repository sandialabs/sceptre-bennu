import sys
from unittest.mock import MagicMock

sys.modules.setdefault("helics", MagicMock())

from py_expression_eval import Parser  # noqa: E402

from pybennu.providers.utils.helics_helper import evaluate_logic  # noqa: E402

TAGS = {"load-1_bus-1.active", "bus-1", "bus-1.voltage", "gen1.active", "x"}


def _eval(exp, **state):
    return evaluate_logic(Parser(), exp, TAGS, lambda tag: state[tag])


def test_logic_without_dashes_still_works():
    assert _eval("gen1.active", **{"gen1.active": 3}) == 3
    assert _eval("x * 2 + 1", x=4) == 9


def test_logic_with_dashes_in_tag_names():
    state = {"load-1_bus-1.active": 1, "bus-1.voltage": 2.0, "bus-1": 5}
    assert _eval("load-1_bus-1.active", **state) == 1
    assert _eval("bus-1.voltage * 2", **state) == 4.0
    # the longer tag wins over its prefix, and plain subtraction still works
    assert _eval("bus-1 - 1", **state) == 4
    assert _eval("bus-1.voltage - bus-1", **state) == -3.0
