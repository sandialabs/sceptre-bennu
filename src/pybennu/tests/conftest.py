"""Stub the zmq/swig-backed modules so Provider can be imported without compiled extensions."""

import sys
import types

for name in ("pybennu.distributed.server", "pybennu.distributed.publisher"):
    mod = types.ModuleType(name)
    mod.Server = type(
        "Server", (), {"__init__": lambda self, *a, **k: None, "run": lambda self: None}
    )
    mod.Publisher = type(
        "Publisher",
        (),
        {"__init__": lambda self, *a, **k: None, "publish": lambda self, m: None},
    )
    sys.modules[name] = mod
