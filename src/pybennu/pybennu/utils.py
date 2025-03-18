"""
Shared utility functions used throughout pybennu.
"""

import atexit
import logging
import os
import platform
import re
from datetime import datetime, timezone
from pathlib import Path
from subprocess import check_output
from io import TextIOWrapper
from typing import List, Optional

BENNU_VERSION = None


def str_to_bool(val: str) -> bool:
    """
    Convert a string representation of truth to True or False.

    True values are: 'y', 'yes', 't', 'true', 'on', '1', 'enable', 'enabled'

    False values are: 'n', 'no', 'f', 'false', 'off', '0', 'disable', 'disabled'

    Args:
        val: String to evaluate

    Returns:
        The boolean representation of the string

    Raises:
        ValueError: val is anything other than a boolean value
    """
    val = val.strip().lower()
    if val in ("y", "yes", "t", "true", "on", "1", "enable", "enabled"):
        return True
    elif val in ("n", "no", "f", "false", "off", "0", "disable", "disabled"):
        return False
    else:
        raise ValueError(f"invalid bool string {val}")


def utc_now() -> datetime:
    """
    datetime.utcnow() returns a naive datetime objects (no timezone info).
    When .timestamp() is called on these objects, they get interpreted
    as local time, not UTC time, which leads to incorrect timestamps.

    Further reading: https://blog.miguelgrinberg.com/post/it-s-time-for-a-change-datetime-utcnow-is-now-deprecated
    """
    return datetime.now(timezone.utc)


def utc_now_formatted() -> str:
    return utc_now().strftime("%d-%m-%Y_%H-%M-%S")


def get_bennu_version() -> str:
    """
    The version of Bennu installed.
    This is retrieved from the apt package metadata on Linux.
    """
    # Cache the result
    global BENNU_VERSION
    if BENNU_VERSION:
        return BENNU_VERSION

    if platform.system().lower() == "linux":
        # Get bennu version by inspecting apt package metadata
        # This won't work on Windows
        try:
            apt_result = check_output("apt-cache show bennu", shell=True).decode()
            if not apt_result:
                BENNU_VERSION = "unknown-version-failed-check"

            BENNU_VERSION = re.search(r"Version: ([\w\.]+)\s", apt_result).groups()[0]
        except Exception as ex:
            print(f"Failed to get bennu version: {ex}", flush=True)
            BENNU_VERSION = "unknown-version-failed-check"
    else:
        BENNU_VERSION = "unknown-version-non-linux"

    return BENNU_VERSION


class RotatingCSVWriter:
    """
    Writes data to CSV files, creating a new file when a limit is reached.
    """

    def __init__(
        self,
        name: str,
        csv_dir: Path,
        header: List[str],
        filename_base: str = "data",
        rows_per_file: int = 1000000,
        max_files: int = 0
    ) -> None:
        self.name: str = name
        self.csv_dir: Path = csv_dir
        self.header: List[str] = header
        self.filename_base: str = filename_base
        self.max_rows: int = rows_per_file
        self.max_files: int = max_files
        self.files_written: List[Path] = []
        self.rows_written: int = 0
        self.current_path: Optional[Path] = None
        self.fp: Optional[TextIOWrapper] = None

        self.log: logging.Logger = logging.getLogger(f"{self.__class__.__name__} [{self.name}]")
        self.log.setLevel(logging.DEBUG)

        if not self.csv_dir.exists():
            self.csv_dir.mkdir(parents=True, exist_ok=True, mode=0o666)  # rw-rw-rw-

        self.log.info(f"CSV output directory: {self.csv_dir}")
        self.log.info(f"CSV header: {self.header}")

        # ensure data is written on exit
        atexit.register(self._close_file)

        # initial file rotation
        self.rotate()

    def _close_file(self) -> None:
        if self.fp and not self.fp.closed:
            self.fp.flush()
            os.fsync(self.fp.fileno())  # ensure data is written to disk
            self.fp.close()

    def rotate(self) -> None:
        self._close_file()  # close current CSV before starting new one
        if self.current_path:
            self.log.debug(f"Wrote {self.rows_written} rows and {self.current_path.stat().st_size} bytes to {self.current_path}")
            self.files_written.append(self.current_path)

        timestamp = utc_now_formatted()
        filename = f"{self.filename_base}_{timestamp}.csv"
        self.current_path = Path(self.csv_dir, filename)
        self.log.info(f"Rotating CSV file to {self.current_path}")
        if self.current_path.exists():
            self.log.error(f"{self.current_path} already exists! Overwriting...")

        # Set file permissions: User/Group/World Read/Write (rw-rw-rw-)
        self.current_path.touch(mode=0o666, exist_ok=True)
        self.fp = self.current_path.open("w", encoding="utf-8", newline="\n")
        self._emit(self.header)  # Write CSV header
        self.rows_written = 0  # Reset row counter

        if self.max_files and len(self.files_written) > self.max_files:
            oldest = self.files_written.pop(0)  # oldest file is first in list
            self.log.info(f"Removing CSV file {oldest}")
            oldest.unlink()  # delete the file

    def _emit(self, data: list) -> None:
        """Write comma-separated list of values."""
        for i, column in enumerate(data):
            self.fp.write(str(column))
            if i < len(data) - 1:
                self.fp.write(",")
        self.fp.write("\n")

    def write(self, data: list) -> None:
        """Write data to CSV file."""
        if self.rows_written == self.max_rows:
            self.rotate()

        if len(data) != len(self.header):
            raise RuntimeError(f"length of CSV data ({len(data)}) does not match length of CSV header ({len(self.header)})")
        assert len(data) == len(self.header)

        self._emit(data)
        self.rows_written += 1
