import importlib
from pathlib import Path
from unittest.mock import patch

import pybennu.executables.groundtruth_monitor as gtm


def make_args(tmp_path, elastic_ip="10.0.0.7"):
    ini = tmp_path / "config.ini"
    ini.write_text(f"[groundtruth-monitor]\nelasticsearch-ip = {elastic_ip}\n")
    args = type("Args", (), {})()
    args.config_file = str(ini)
    args.env = "test"
    args.operation = "status"
    return args


def test_client_is_built_with_a_scheme(tmp_path):
    """elasticsearch-py 8 rejects node dicts without a scheme (TypeError)."""
    with (
        patch.object(gtm, "Elasticsearch") as es,
        patch.object(gtm.Subscriber, "__init__", lambda *a, **k: None),
        patch.object(gtm.Daemon, "__init__", lambda *a, **k: None),
        patch.object(gtm.threading.Thread, "start"),
    ):
        gtm.GroundTruthMonitor(make_args(tmp_path))
    es.assert_called_once_with("http://10.0.0.7:9200")


def test_console_script_target_exists():
    """pyproject's pybennu-groundtruth-monitor entry point must import."""
    pyproject = (Path(gtm.__file__).parents[2] / "pyproject.toml").read_text()
    line = next(
        ln
        for ln in pyproject.splitlines()
        if ln.startswith("pybennu-groundtruth-monitor")
    )
    module, func = line.split('"')[1].split(":")
    assert callable(getattr(importlib.import_module(module), func))
