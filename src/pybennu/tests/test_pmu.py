from unittest.mock import MagicMock, patch

import pytest

from pybennu.pmu_wrapper import PMU
from pybennu.pypmu.synchrophasor.frame import (
    CommonFrame,
    ConfigFrame2,
    DataFrame,
    HeaderFrame,
)

# =============================================================================
# PMU Initialization & Formatting Tests
# =============================================================================


def test_pmu_initialization_protocols():
    for proto in ("tcp", "udp", "multicast"):
        with patch("pybennu.pmu_wrapper.Pdc"):
            pmu = PMU(ip="127.0.0.1", port=4712, pdc_id=1, protocol=proto)
            assert pmu.protocol == proto

    with pytest.raises(ValueError, match="PMU protocol must be"):
        with patch("pybennu.pmu_wrapper.Pdc"):
            PMU(ip="127.0.0.1", port=4712, pdc_id=1, protocol="invalid")


def test_pmu_str_and_repr():
    with patch("pybennu.pmu_wrapper.Pdc"):
        # Name and label set
        p1 = PMU(ip="10.0.0.1", port=4712, pdc_id=5, name="PMU1", label="BUS6")
        assert str(p1) == "PMU1_BUS6"
        assert repr(p1) == (
            "PMU(ip=10.0.0.1, port=4712, pdc_id=5, name=PMU1, label=BUS6, protocol=tcp)"
        )

        # Name only
        p2 = PMU(ip="10.0.0.1", port=4712, pdc_id=5, name="PMU1")
        assert str(p2) == "PMU1"

        # Neither name nor label
        p3 = PMU(ip="10.0.0.1", port=4712, pdc_id=5)
        assert str(p3) == "10.0.0.1:4712_5"


# =============================================================================
# Channel Name Cleaning & Config Parsing Tests
# =============================================================================


def test_channel_name_cleaning():
    with patch("pybennu.pmu_wrapper.Pdc") as mock_pdc_cls:
        mock_pdc = MagicMock()
        mock_pdc_cls.return_value = mock_pdc

        class MockConfig:
            def __init__(self):
                self._station_name = "  Substation Alpha  "
                self._channel_names = [
                    "PHASOR CH 1:VA  ",
                    "PHASOR CH 2:VB  ",
                    ["PHASOR CH 3:VC  ", "PHASOR CH 4:IA  "],
                    "PLAIN_CHANNEL",
                ]

        mock_pdc.get_config.return_value = MockConfig()

        pmu = PMU(ip="127.0.0.1", port=4712, pdc_id=1)
        pmu.run()

        assert pmu.station_name == "Substation Alpha"
        assert pmu.channel_names == ["VA", "VB", "VC", "IA", "PLAIN_CHANNEL"]


# =============================================================================
# Data Frame Handling Tests
# =============================================================================


def test_get_data_frame_valid_and_mismatch():
    with patch("pybennu.pmu_wrapper.Pdc") as mock_pdc_cls:
        mock_pdc = MagicMock()
        mock_pdc_cls.return_value = mock_pdc
        pmu = PMU(ip="127.0.0.1", port=4712, pdc_id=100)

        # 1. pdc.get() returns None
        mock_pdc.get.return_value = None
        assert pmu.get_data_frame() is None

        # 2. pdc.get() returns non-DataFrame
        mock_pdc.get.return_value = "not a dataframe"
        assert pmu.get_data_frame() is None

        # 3. pdc.get() returns DataFrame matching pdc_id
        mock_df = MagicMock(spec=DataFrame)
        mock_df.get_measurements.return_value = {
            "pmu_id": 100,
            "time": 123456789.0,
            "measurements": [{"stream_id": 100, "frequency": 60.0}],
        }
        mock_pdc.get.return_value = mock_df

        result = pmu.get_data_frame()
        assert result is not None
        assert result["pmu_id"] == 100
        assert result["measurements"][0]["frequency"] == 60.0

        # 4. pdc.get() returns DataFrame with mismatched pdc_id
        mock_df_mismatch = MagicMock(spec=DataFrame)
        mock_df_mismatch.get_measurements.return_value = {
            "pmu_id": 999,
            "time": 123456789.0,
            "measurements": [],
        }
        mock_pdc.get.return_value = mock_df_mismatch
        result_mismatch = pmu.get_data_frame()
        assert result_mismatch is not None
        assert result_mismatch["pmu_id"] == 999


# =============================================================================
# Quit / Cleanup Safety Tests
# =============================================================================


def test_quit_cleanup_safety():
    with patch("pybennu.pmu_wrapper.Pdc") as mock_pdc_cls:
        mock_pdc = MagicMock()
        mock_pdc_cls.return_value = mock_pdc
        pmu = PMU(ip="127.0.0.1", port=4712, pdc_id=1)

        # Not started: quit() simply calls pdc.quit()
        pmu.started = False
        pmu.quit()
        mock_pdc.stop.assert_not_called()
        mock_pdc.quit.assert_called_once()

        mock_pdc.reset_mock()

        # Started and connected: stop() called then quit()
        pmu.started = True
        mock_pdc.pmu_socket = MagicMock()
        assert pmu.connected is True

        pmu.quit()
        mock_pdc.stop.assert_called_once()
        mock_pdc.quit.assert_called_once()
        assert pmu.started is False

        mock_pdc.reset_mock()

        # Started and connected, but stop() raises an exception
        pmu.started = True
        mock_pdc.stop.side_effect = RuntimeError("stop error")
        pmu.quit()  # Should not raise
        mock_pdc.quit.assert_called_once()
        assert pmu.started is False


def test_rebuild_tcp_connection_retries_and_succeeds():
    with patch("pybennu.pmu_wrapper.Pdc") as mock_pdc_cls:
        mock_pdc = MagicMock()
        mock_pdc_cls.return_value = mock_pdc
        pmu = PMU(ip="127.0.0.1", port=4712, pdc_id=1)
        pmu.retry_delay = 0.5

        run_calls = []

        def mock_run():
            if len(run_calls) < 2:
                run_calls.append("fail")
                raise ConnectionRefusedError("connection refused")
            run_calls.append("ok")

        pmu.run = MagicMock(side_effect=mock_run)
        pmu.start = MagicMock()
        pmu.quit = MagicMock()

        with patch("time.sleep") as mock_sleep:
            pmu.rebuild_tcp_connection()

            assert pmu.run.call_count == 3
            pmu.start.assert_called_once()
            assert mock_sleep.call_count == 2
            mock_sleep.assert_called_with(0.5)


# =============================================================================
# Synchrophasor Frame Encoding / Decoding Tests
# =============================================================================


def test_synchrophasor_header_frame_roundtrip():
    header = HeaderFrame(
        pmu_id_code=42,
        header="Bennu PMU Header",
        soc=1600000000,
        frasec=0,
    )
    raw_bytes = header.convert2bytes()
    assert isinstance(raw_bytes, bytes)
    assert len(raw_bytes) > 0

    decoded = HeaderFrame.convert2frame(raw_bytes)
    assert decoded.get_id_code() == 42
    assert "Bennu PMU Header" in decoded.get_header()

    # Also test via CommonFrame.convert2frame
    cf_decoded = CommonFrame.convert2frame(raw_bytes)
    assert isinstance(cf_decoded, HeaderFrame)
    assert cf_decoded.get_id_code() == 42


def test_synchrophasor_data_frame_roundtrip():
    channel_names = ["VA", "VB", "VC", "I1", "AN1", "AN2", "AN3"] + [
        f"DIG{i}" for i in range(16)
    ]
    cfg = ConfigFrame2(
        pmu_id_code=7734,
        time_base=1000000,
        num_pmu=1,
        station_name="Station A",
        id_code=7734,
        data_format=(False, False, True, False),
        phasor_num=4,
        analog_num=3,
        digital_num=1,
        channel_names=channel_names,
        ph_units=[(915527, "v"), (915527, "v"), (915527, "v"), (45776, "i")],
        an_units=[(1, "pow"), (1, "rms"), (1, "peak")],
        dig_units=[(0x0000, 0xFFFF)],
        f_nom=60,
        cfg_count=22,
        data_rate=30,
        soc=1600000000,
        frasec=0,
    )

    data = DataFrame(
        pmu_id_code=7734,
        stat=("ok", True, "timestamp", False, False, False, 0, "<10", 0),
        phasors=[(14635, 0), (-7318, -12676), (-7318, 12675), (1092, 0)],
        freq=2500,
        dfreq=0,
        analog=[100.0, 1000.0, 10000.0],
        digital=[0x3C12],
        cfg=cfg,
        soc=1600000000,
        frasec=0,
    )

    raw_bytes = data.convert2bytes()
    assert isinstance(raw_bytes, bytes)

    decoded = DataFrame.convert2frame(raw_bytes, cfg)
    assert decoded.get_id_code() == 7734

    measurements = decoded.get_measurements()
    assert measurements["pmu_id"] == 7734
    assert len(measurements["measurements"]) == 1
    m0 = measurements["measurements"][0]
    assert m0["stream_id"] == 7734
    assert len(m0["phasors"]) == 4
    assert len(m0["analog"]) == 3
