import ast
import logging
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
