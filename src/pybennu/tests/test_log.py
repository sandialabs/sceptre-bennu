import ast
import inspect
import logging
import os
import pathlib

import pytest
from loguru import logger

from pybennu import log

PKG = pathlib.Path(__file__).parents[1] / "pybennu"
LOG_METHODS = {
    "trace",
    "debug",
    "info",
    "success",
    "warning",
    "error",
    "exception",
    "critical",
}


@pytest.mark.parametrize(
    ("raw", "want"),
    [
        ("", "INFO"),
        ("error", "ERROR"),
        ("0", "ERROR"),
        ("Warn", "WARNING"),
        ("1", "WARNING"),
        ("info", "INFO"),
        ("2", "INFO"),
        ("DEBUG", "DEBUG"),
        ("3", "DEBUG"),
    ],
)
def test_log_level(monkeypatch, raw, want):
    monkeypatch.setenv("BENNU_LOG_LEVEL", raw)
    assert log.log_level() == want


def test_log_level_invalid_and_debug(monkeypatch):
    monkeypatch.setenv("BENNU_LOG_LEVEL", "loud")
    with pytest.raises(ValueError, match="BENNU_LOG_LEVEL"):
        log.log_level()
    assert log.log_level(debug=True) == "DEBUG"


def test_log_file(monkeypatch):
    monkeypatch.delenv("BENNU_LOG_FILE", raising=False)
    assert log.log_file("p", True) == "/var/log/bennu/p.log"
    assert log.log_file("p", False) is None
    monkeypatch.setenv("BENNU_LOG_FILE", "")
    assert log.log_file("p", True) is None
    monkeypatch.setenv("BENNU_LOG_FILE", "/tmp/x.log")
    assert log.log_file("p", False) == "/tmp/x.log"


def test_file_sink_ctx_intercept_and_enable_debug(monkeypatch, tmp_path):
    path = tmp_path / "sub" / "p.log"  # the sink creates missing directories
    monkeypatch.setenv("BENNU_LOG_FILE", str(path))
    monkeypatch.delenv("BENNU_LOG_LEVEL", raising=False)
    log._config.clear()
    log.setup_logging("p")
    try:
        logger.bind(ctx="Modbus 10.0.0.1:502").info("reply {}", 7)
        logger.debug("hidden at INFO")
        logging.getLogger("vendored").warning("stdlib %s", "routed")
        log.enable_debug()
        logger.debug("shown after enable_debug")
    finally:
        logger.remove()
        log._config.clear()
    text = path.read_text()
    assert "[Modbus 10.0.0.1:502] reply 7" in text
    assert "hidden at INFO" not in text
    assert "stdlib routed" in text
    assert "shown after enable_debug" in text


def test_no_stdlib_logging_or_percent_style_in_pybennu():
    """Guards the loguru migration: a stdlib logger or a %-style message in a
    loguru call (which prints the %s literally) must not come back via a merge."""
    bad = []
    for p in PKG.rglob("*.py"):
        rel = p.relative_to(PKG).as_posix()
        if rel.startswith(("pypmu/", "siren/test/")):
            continue
        for n in ast.walk(ast.parse(p.read_text())):
            if not (isinstance(n, ast.Call) and isinstance(n.func, ast.Attribute)):
                continue
            if n.func.attr == "basicConfig" and rel != "log.py":
                bad.append(f"{rel}:{n.lineno} basicConfig")
            first = n.args[0] if n.args else None
            if (
                n.func.attr in LOG_METHODS
                and len(n.args) > 1
                and isinstance(first, ast.Constant)
                and isinstance(first.value, str)
                and "%" in first.value
            ):
                bad.append(f"{rel}:{n.lineno} %-style message")
        for line_no, line in enumerate(p.read_text().splitlines(), 1):
            if line.startswith("logger = logging.getLogger"):
                bad.append(f"{rel}:{line_no} stdlib module logger")
    assert not bad, bad


def test_intercept_attributes_stdlib_records_to_their_caller(monkeypatch):
    monkeypatch.setenv("BENNU_LOG_FILE", "")
    log._config.clear()
    log.setup_logging("p")
    seen = []
    logger.add(lambda m: seen.append(m.record), level="INFO")
    try:
        logging.getLogger("vendored").warning("where am I")
        line = inspect.currentframe().f_lineno - 1
    finally:
        logger.remove()
        log._config.clear()
    rec = seen[-1]
    assert (rec["file"].name, rec["line"]) == ("test_log.py", line)
    assert rec["function"] == "test_intercept_attributes_stdlib_records_to_their_caller"


def test_relative_log_file_stays_put_after_chdir(monkeypatch, tmp_path):
    first, second = tmp_path / "a", tmp_path / "b"
    first.mkdir()
    second.mkdir()
    monkeypatch.setenv("BENNU_LOG_FILE", "rel.log")
    log._config.clear()
    monkeypatch.chdir(first)
    log.setup_logging("p")
    monkeypatch.chdir(second)  # a daemon chdir('/') in miniature
    try:
        log.enable_debug()
        logger.info("after rerun")
    finally:
        logger.remove()
        log._config.clear()
    assert "after rerun" in (first / "rel.log").read_text()
    assert not (second / "rel.log").exists()


def test_siren_device_level_is_per_device(monkeypatch, tmp_path):
    from types import SimpleNamespace

    from pybennu.siren.hil_device.hil_device import HIL_Device

    path = tmp_path / "s.log"
    monkeypatch.setenv("BENNU_LOG_FILE", str(path))
    monkeypatch.delenv("BENNU_LOG_LEVEL", raising=False)
    log._config.clear()
    log.setup_logging("p")
    loud, quiet = (
        SimpleNamespace(device_name="loud"),
        SimpleNamespace(device_name="quiet"),
    )
    try:
        HIL_Device.setup_logger(loud, "debug")
        HIL_Device.setup_logger(quiet, "error")
        loud.logger.debug("loud debug")
        quiet.logger.warning("quiet warning")
        quiet.logger.error("quiet error")
    finally:
        logger.remove()
        log._config.clear()
        logging.getLogger("loud").setLevel(logging.NOTSET)
        logging.getLogger("quiet").setLevel(logging.NOTSET)
    text = path.read_text()
    assert "loud debug" in text
    assert "quiet warning" not in text
    assert "quiet error" in text


def test_daemonized_process_reaches_the_file_sink(tmp_path):
    """After the double fork and stdio redirect, the file sink must still get INFO."""
    import subprocess
    import sys
    import textwrap
    import time

    path = tmp_path / "d.log"
    script = textwrap.dedent(f"""
        from pybennu.log import setup_logging
        from pybennu.providers.utils.daemon import Daemon

        class D(Daemon):
            def run(self):
                pass

        setup_logging("p")
        D("{tmp_path}/d.pid", "/dev/null", "{tmp_path}/d.out", "{tmp_path}/d.err").start()
    """)
    env = {
        **os.environ,
        "BENNU_LOG_FILE": str(path),
        "PYTHONPATH": str(PKG.parent),
    }
    env.pop("BENNU_LOG_LEVEL", None)
    subprocess.run(
        [sys.executable, "-c", script],
        env=env,
        check=True,
        timeout=30,
        capture_output=True,
    )
    for _ in range(50):  # the grandchild logs after the parent returns
        if path.exists() and path.read_text():
            break
        time.sleep(0.1)
    assert "Daemonized as PID" in path.read_text()
    assert "Daemonized as PID" in (tmp_path / "d.err").read_text()
