"""Stub the zmq/swig-backed modules so Provider and CLI executables can be imported without compiled extensions."""

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

if "pybennu.distributed.swig._Endpoint" not in sys.modules:
    try:
        import pybennu.distributed.swig._Endpoint  # noqa: F401
    except Exception:
        swig_mod = types.ModuleType("pybennu.distributed.swig._Endpoint")
        swig_mod.new_Endpoint = lambda: "mock_endpoint"
        swig_mod.Endpoint_str_set = lambda ep, s: None
        swig_mod.Endpoint_str_get = lambda ep: (
            str(ep) if ep != "mock_endpoint" else "tcp://127.0.0.1:5555"
        )
        swig_mod.Endpoint_hash = lambda ep: "mock_hash"
        sys.modules["pybennu.distributed.swig._Endpoint"] = swig_mod
        parent = sys.modules.get("pybennu.distributed.swig") or types.ModuleType(
            "pybennu.distributed.swig"
        )
        parent._Endpoint = swig_mod
        sys.modules["pybennu.distributed.swig"] = parent

if "pybennu.distributed.subscriber" not in sys.modules:
    sub_mod = types.ModuleType("pybennu.distributed.subscriber")
    sub_mod.Subscriber = type(
        "Subscriber",
        (),
        {
            "__init__": lambda self, *a, **k: None,
            "run": lambda self: None,
            "disconnect": lambda self: None,
        },
    )
    sys.modules["pybennu.distributed.subscriber"] = sub_mod

if "elasticsearch" not in sys.modules:
    try:
        import elasticsearch  # noqa: F401
    except ImportError:
        es_mod = types.ModuleType("elasticsearch")

        class MockBulkIndexError(Exception):
            def __init__(self, message, errors=None):
                super().__init__(message)
                self.errors = errors or []

        helpers_mod = types.ModuleType("elasticsearch.helpers")
        helpers_mod.BulkIndexError = MockBulkIndexError
        helpers_mod.bulk = lambda *a, **k: (0, [])

        class MockElasticsearch:
            def __init__(self, *a, **k):
                self.indices = types.SimpleNamespace(
                    exists=lambda **k: True,
                    create=lambda **k: None,
                )

            def ping(self):
                return True

            def close(self):
                pass

        es_mod.Elasticsearch = MockElasticsearch
        es_mod.helpers = helpers_mod
        sys.modules["elasticsearch"] = es_mod
        sys.modules["elasticsearch.helpers"] = helpers_mod
