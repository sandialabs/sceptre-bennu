"""Logging setup for pybennu entry points.

Library modules log with ``from loguru import logger``. pybennu disables its
own loguru output on import, so an application that embeds a provider sees
nothing until it calls ``logger.enable("pybennu")`` or ``setup_logging``.

Environment:

- ``BENNU_LOG_LEVEL``: ``error``, ``warn``, ``info``, ``debug`` or ``0``-``3``,
  case-insensitive. Shared with the C++ tools. Unset or empty means INFO.
- ``BENNU_LOG_FILE``: path of the log file. Empty disables the file sink. Unset
  means ``/var/log/bennu/<prog>.log`` for daemons and no file for CLI tools.
"""

import logging
import os
import sys

from loguru import logger

LOG_DIR = "/var/log/bennu"
FORMAT = (
    "{time:YYYY-MM-DD HH:mm:ss.SSS} | {level: <8} | {name}:{line} | "
    "{extra[prefix]}{message}"
)
_LEVELS = {
    "error": "ERROR",
    "0": "ERROR",
    "warn": "WARNING",
    "warning": "WARNING",
    "1": "WARNING",
    "info": "INFO",
    "2": "INFO",
    "debug": "DEBUG",
    "3": "DEBUG",
}
_config: dict = {}


class InterceptHandler(logging.Handler):
    """Route stdlib ``logging`` records (third-party and vendored code) to loguru."""

    def emit(self, record: logging.LogRecord) -> None:
        try:
            level: str | int = logger.level(record.levelname).name
        except ValueError:
            level = record.levelno
        # Walk past the logging module's frames so loguru reports the caller.
        frame, depth = logging.currentframe(), 2
        while frame and frame.f_code.co_filename == logging.__file__:
            frame = frame.f_back
            depth += 1
        logger.opt(depth=depth, exception=record.exc_info).log(
            level, record.getMessage()
        )


def _prefix(record) -> None:
    # Components bind ctx (e.g. logger.bind(ctx="Modbus 10.0.0.1:502")) to tag lines.
    ctx = record["extra"].get("ctx")
    record["extra"]["prefix"] = f"[{ctx}] " if ctx else ""


def log_level(debug: bool = False) -> str:
    """Return the loguru level name; ``debug`` forces DEBUG."""
    if debug:
        return "DEBUG"
    raw = os.environ.get("BENNU_LOG_LEVEL", "").strip().lower()
    if not raw:
        return "INFO"
    if raw not in _LEVELS:
        raise ValueError(
            f"invalid BENNU_LOG_LEVEL {raw!r}: use error, warn, info, debug or 0-3"
        )
    return _LEVELS[raw]


def log_file(prog: str, file_by_default: bool) -> str | None:
    """Return the log file path for ``prog``, or None for no file sink."""
    if "BENNU_LOG_FILE" in os.environ:
        return os.environ["BENNU_LOG_FILE"].strip() or None
    return f"{LOG_DIR}/{prog}.log" if file_by_default else None


def setup_logging(prog: str, debug: bool = False, file_by_default: bool = True) -> None:
    """Send pybennu and stdlib logging to stderr and, if configured, a file.

    Call once from an entry point, before any daemonizing fork. Raises if the
    level is invalid or the log file cannot be opened.
    """
    _config.update(prog=prog, debug=debug, file_by_default=file_by_default)
    level = log_level(debug)
    path = log_file(prog, file_by_default)
    logger.remove()
    logger.configure(patcher=_prefix)
    # diagnose=False keeps local variables (credentials in settings) out of tracebacks.
    logger.add(sys.stderr, level=level, format=FORMAT, diagnose=False)
    if path:
        logger.add(path, level=level, format=FORMAT, diagnose=False)
    logger.enable("pybennu")
    logging.basicConfig(handlers=[InterceptHandler()], level=level, force=True)


def enable_debug() -> None:
    """Re-apply the entry point's logging at DEBUG, e.g. for a config ``debug`` flag.

    Does nothing if ``setup_logging`` was never called (an embedding application
    owns its logging).
    """
    if _config:
        setup_logging(**{**_config, "debug": True})
