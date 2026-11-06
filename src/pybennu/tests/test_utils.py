import re
from datetime import datetime, timezone
from unittest.mock import MagicMock, patch

import pytest

from pybennu.utils import (
    RotatingCSVWriter,
    str_to_bool,
    utc_now,
    utc_now_formatted,
)

# =============================================================================
# str_to_bool Tests
# =============================================================================


@pytest.mark.parametrize(
    "truthy_str",
    ["y", "yes", "t", "true", "on", "1", "enable", "enabled"],
)
def test_str_to_bool_truthy(truthy_str):
    assert str_to_bool(truthy_str) is True
    # Test uppercase and whitespace
    assert str_to_bool(f"  {truthy_str.upper()}  ") is True
    assert str_to_bool(f"{truthy_str.capitalize()} ") is True


@pytest.mark.parametrize(
    "falsy_str",
    ["n", "no", "f", "false", "off", "0", "disable", "disabled"],
)
def test_str_to_bool_falsy(falsy_str):
    assert str_to_bool(falsy_str) is False
    # Test uppercase and whitespace
    assert str_to_bool(f"  {falsy_str.upper()}  ") is False
    assert str_to_bool(f"{falsy_str.capitalize()} ") is False


@pytest.mark.parametrize(
    "invalid_val",
    ["", "   ", "maybe", "2", "-1", "none", "truee", "disabled!"],
)
def test_str_to_bool_invalid(invalid_val):
    with pytest.raises(ValueError, match="invalid bool string"):
        str_to_bool(invalid_val)


# =============================================================================
# UTC Datetime Tests
# =============================================================================


def test_utc_now():
    now = utc_now()
    assert isinstance(now, datetime)
    assert now.tzinfo is not None
    assert now.tzinfo == timezone.utc


def test_utc_now_formatted():
    formatted = utc_now_formatted()
    # Format: %d-%m-%Y_%H-%M-%S (e.g. 06-11-2026_03-15-30)
    pattern = r"^\d{2}-\d{2}-\d{4}_\d{2}-\d{2}-\d{2}$"
    assert re.match(pattern, formatted) is not None

    # Ensure it roundtrips back into a datetime with expected format
    parsed = datetime.strptime(formatted, "%d-%m-%Y_%H-%M-%S")
    assert parsed is not None


# =============================================================================
# RotatingCSVWriter Tests
# =============================================================================


def test_rotating_csv_writer_creates_dir_and_header(tmp_path):
    output_dir = tmp_path / "nested" / "csv_output"
    assert not output_dir.exists()

    header = ["timestamp", "tag1", "tag2"]
    writer = RotatingCSVWriter(
        name="test_writer",
        csv_dir=output_dir,
        header=header,
        filename_base="measurements",
        rows_per_file=10,
    )

    assert output_dir.exists()
    assert writer.current_path.exists()

    writer._close_file()

    # Verify header is written
    content = writer.current_path.read_text(encoding="utf-8")
    assert content == "timestamp,tag1,tag2\n"


def test_rotating_csv_writer_append_rows(tmp_path):
    header = ["time", "val"]
    writer = RotatingCSVWriter(
        name="test_append",
        csv_dir=tmp_path,
        header=header,
        rows_per_file=10,
    )

    writer.write(["1.0", "42"])
    writer.write(["2.0", "43"])
    writer._close_file()

    content = writer.current_path.read_text(encoding="utf-8")
    expected = "time,val\n1.0,42\n2.0,43\n"
    assert content == expected


def test_rotating_csv_writer_length_mismatch(tmp_path):
    header = ["col1", "col2"]
    writer = RotatingCSVWriter(
        name="test_mismatch",
        csv_dir=tmp_path,
        header=header,
    )

    with pytest.raises(RuntimeError, match="length of CSV data"):
        writer.write(["single_value"])

    with pytest.raises(RuntimeError, match="length of CSV data"):
        writer.write(["val1", "val2", "val3"])

    writer._close_file()


def test_rotating_csv_writer_rotates_on_limit(tmp_path):
    header = ["seq"]
    timestamps = [
        "01-01-2026_00-00-01",
        "01-01-2026_00-00-02",
        "01-01-2026_00-00-03",
    ]

    with patch(
        "pybennu.utils.utc_now_formatted",
        side_effect=timestamps,
    ):
        writer = RotatingCSVWriter(
            name="test_rotation",
            csv_dir=tmp_path,
            header=header,
            filename_base="stream",
            rows_per_file=2,
            max_files=0,
        )
        first_file = writer.current_path
        assert first_file.name == "stream_01-01-2026_00-00-01.csv"

        # Write 2 rows to fill first file
        writer.write([1])
        writer.write([2])
        assert writer.rows_written == 2

        # 3rd row triggers rotation
        writer.write([3])
        second_file = writer.current_path
        assert second_file.name == "stream_01-01-2026_00-00-02.csv"
        assert second_file != first_file
        assert writer.rows_written == 1

        writer._close_file()

    assert first_file.read_text(encoding="utf-8") == "seq\n1\n2\n"
    assert second_file.read_text(encoding="utf-8") == "seq\n3\n"


def test_rotating_csv_writer_max_files_eviction(tmp_path):
    header = ["idx"]
    timestamps = [f"01-01-2026_00-00-0{i}" for i in range(1, 8)]

    with patch(
        "pybennu.utils.utc_now_formatted",
        side_effect=timestamps,
    ):
        # max_files=2: at most 2 completed/archived files retained in files_written
        writer = RotatingCSVWriter(
            name="test_eviction",
            csv_dir=tmp_path,
            header=header,
            filename_base="evict",
            rows_per_file=1,
            max_files=2,
        )
        f1 = writer.current_path

        # Write row 1 (fills file 1)
        writer.write([1])
        # Write row 2 (triggers rotation to file 2, archives f1)
        writer.write([2])
        f2 = writer.current_path
        # Write row 3 (triggers rotation to file 3, archives f2)
        writer.write([3])
        f3 = writer.current_path
        # Write row 4 (triggers rotation to file 4; len(files_written) > 2 so f1 is deleted)
        writer.write([4])
        f4 = writer.current_path

        writer._close_file()

    # f1 should have been unlinked/deleted
    assert not f1.exists()
    # f2, f3, and f4 should exist
    assert f2.exists()
    assert f3.exists()
    assert f4.exists()


def test_rotating_csv_writer_close_and_flush(tmp_path):
    header = ["a", "b"]
    writer = RotatingCSVWriter(
        name="test_close",
        csv_dir=tmp_path,
        header=header,
    )

    mock_fp = MagicMock()
    mock_fp.closed = False
    mock_fp.fileno.return_value = 10

    def mock_close():
        mock_fp.closed = True

    mock_fp.close.side_effect = mock_close
    writer.fp = mock_fp

    with patch("os.fsync") as mock_fsync:
        writer._close_file()
        mock_fp.flush.assert_called_once()
        mock_fsync.assert_called_once_with(10)
        mock_fp.close.assert_called_once()
