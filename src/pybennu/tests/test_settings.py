from ipaddress import IPv4Address

import pytest
from pydantic import ValidationError

from pybennu.settings import (
    CsvSettings,
    ElasticSettings,
    GtnetSktSettings,
    GtnetSktTag,
    ModbusRegister,
    ModbusSettings,
    PmuDevice,
    PmuSettings,
    PybennuSettings,
    SirenSettings,
    SirenTag,
    load_settings_yaml,
)

# =============================================================================
# GtnetSktTag Tests
# =============================================================================


def test_gtnet_skt_tag_valid_names():
    t1 = GtnetSktTag(name="G1CB1.closed", type="int", initial_value=1)
    assert t1.name == "G1CB1.closed"
    assert t1.type == "int"
    assert t1.initial_value == 1

    t2 = GtnetSktTag(name="DL5shed.value", type="float", initial_value=60.0)
    assert t2.name == "DL5shed.value"
    assert t2.type == "float"
    assert t2.initial_value == 60.0


def test_gtnet_skt_tag_invalid_names():
    with pytest.raises(ValidationError, match="name must end with one of"):
        GtnetSktTag(name="G1CB1.status", type="int", initial_value=0)

    with pytest.raises(ValidationError, match="name must end with one of"):
        GtnetSktTag(name="G1CB1", type="int", initial_value=0)


def test_gtnet_skt_tag_type_restrictions():
    with pytest.raises(ValidationError):
        GtnetSktTag(name="tag.closed", type="str", initial_value=1)

    with pytest.raises(ValidationError):
        GtnetSktTag(name="tag.value", type="bool", initial_value=1)


def test_gtnet_skt_tag_initial_value_coercion():
    tag_int = GtnetSktTag(name="tag.closed", type="int", initial_value=5)
    assert tag_int.initial_value == 5

    tag_float = GtnetSktTag(name="tag.value", type="float", initial_value=3.14)
    assert tag_float.initial_value == pytest.approx(3.14)


# =============================================================================
# SirenTag & SirenSettings Tests
# =============================================================================


def test_siren_tag():
    t_int = SirenTag(name="tag1", type="int", initial_value=10, publish_rms=True)
    assert t_int.name == "tag1"
    assert t_int.type == "int"
    assert t_int.initial_value == 10
    assert t_int.publish_rms is True

    t_bool = SirenTag(name="tag2", type="bool", initial_value=False)
    assert t_bool.type == "bool"
    assert t_bool.initial_value is False
    assert t_bool.publish_rms is False


def test_siren_tag_invalid_type():
    with pytest.raises(ValidationError):
        SirenTag(name="tag1", type="invalid", initial_value=1)


def test_siren_settings_defaults():
    s = SirenSettings()
    assert s.enabled is False
    assert s.siren_json == "/etc/sceptre/siren.json"
    assert s.publish_rate == 1.0
    assert s.rms_buffer_size == 100
    assert s.tags == []


def test_siren_settings_rate_constraints():
    with pytest.raises(ValidationError):
        SirenSettings(publish_rate=0.0)

    with pytest.raises(ValidationError):
        SirenSettings(publish_rate=-0.5)

    s = SirenSettings(publish_rate=0.01)
    assert s.publish_rate == 0.01


def test_siren_settings_buffer_size_constraints():
    with pytest.raises(ValidationError):
        SirenSettings(rms_buffer_size=0)

    with pytest.raises(ValidationError):
        SirenSettings(rms_buffer_size=-10)

    s = SirenSettings(rms_buffer_size=50)
    assert s.rms_buffer_size == 50


def test_siren_settings_tags_list():
    tag = SirenTag(name="IA.real", type="float", initial_value=12.5)
    s = SirenSettings(tags=[tag])
    assert len(s.tags) == 1
    assert s.tags[0].name == "IA.real"


# =============================================================================
# PmuDevice & PmuSettings Tests
# =============================================================================


def test_pmu_device_valid():
    dev = PmuDevice(
        name="PMU1",
        label="BUS6",
        pdc_id=1,
        port=4712,
        ip=IPv4Address("192.168.1.100"),
        protocol="tcp",
    )
    assert dev.name == "PMU1"
    assert dev.label == "BUS6"
    assert dev.pdc_id == 1
    assert dev.port == 4712
    assert dev.ip == IPv4Address("192.168.1.100")
    assert dev.protocol == "tcp"


def test_pmu_device_number_to_str_coercion():
    dev = PmuDevice(
        name=123,
        label=456,
        pdc_id=2,
        port=5000,
        ip="10.0.0.1",
        protocol="udp",
    )
    assert dev.name == "123"
    assert dev.label == "456"


def test_pmu_device_ipv4_validation():
    with pytest.raises(ValidationError):
        PmuDevice(
            name="PMU1",
            label="BUS1",
            pdc_id=1,
            port=4712,
            ip="999.999.999.999",
            protocol="tcp",
        )

    with pytest.raises(ValidationError):
        PmuDevice(
            name="PMU1",
            label="BUS1",
            pdc_id=1,
            port=4712,
            ip="not-an-ip",
            protocol="tcp",
        )


def test_pmu_device_protocol_validation():
    for proto in ("tcp", "udp", "multicast"):
        dev = PmuDevice(
            name="PMU1",
            label="BUS1",
            pdc_id=1,
            port=4712,
            ip="127.0.0.1",
            protocol=proto,
        )
        assert dev.protocol == proto

    with pytest.raises(ValidationError):
        PmuDevice(
            name="PMU1",
            label="BUS1",
            pdc_id=1,
            port=4712,
            ip="127.0.0.1",
            protocol="http",
        )


def test_pmu_device_port_and_pdc_id_gt_zero():
    with pytest.raises(ValidationError):
        PmuDevice(
            name="PMU1",
            label="BUS1",
            pdc_id=1,
            port=0,
            ip="127.0.0.1",
            protocol="tcp",
        )

    with pytest.raises(ValidationError):
        PmuDevice(
            name="PMU1",
            label="BUS1",
            pdc_id=0,
            port=4712,
            ip="127.0.0.1",
            protocol="tcp",
        )


def test_pmu_settings():
    s = PmuSettings()
    assert s.enabled is False
    assert s.retry_delay == 5.0
    assert s.retry_attempts == 3
    assert s.bind_ip == IPv4Address("0.0.0.0")
    assert s.pmus == []

    with pytest.raises(ValidationError):
        PmuSettings(retry_delay=0.0)


# =============================================================================
# ModbusRegister Tests
# =============================================================================


def test_modbus_register_append_sceptre_suffix():
    # 1. Retains existing sceptre_tag if set
    r1 = ModbusRegister(
        num=0,
        name="custom_reg",
        reg_type="holding",
        access="r",
        data_type="float32",
        sceptre_tag="explicit.tag.name",
    )
    assert r1.sceptre_tag == "explicit.tag.name"

    # 2. If unset, uses name as-is if ending in .closed or .value
    r2 = ModbusRegister(
        num=1,
        name="breaker.closed",
        reg_type="coil",
        access="rw",
        data_type="bool",
    )
    assert r2.sceptre_tag == "breaker.closed"

    r3 = ModbusRegister(
        num=2,
        name="voltage.value",
        reg_type="holding",
        access="r",
        data_type="float32",
    )
    assert r3.sceptre_tag == "voltage.value"

    # 3. If data_type == 'bool', appends .closed
    r4 = ModbusRegister(
        num=3,
        name="generator_switch",
        reg_type="coil",
        access="rw",
        data_type="bool",
    )
    assert r4.sceptre_tag == "generator_switch.closed"

    # 4. Otherwise appends .value
    r5 = ModbusRegister(
        num=4,
        name="generator_freq",
        reg_type="holding",
        access="r",
        data_type="float32",
    )
    assert r5.sceptre_tag == "generator_freq.value"


def test_modbus_register_humanize():
    r_kw = ModbusRegister(
        num=1,
        name="active_power",
        reg_type="holding",
        access="r",
        data_type="float32",
        unit_type="kW",
    )
    assert r_kw.humanize(4.05) == "4.05kW"

    r_pct = ModbusRegister(
        num=2,
        name="battery_soc",
        reg_type="holding",
        access="r",
        data_type="float32",
        unit_type="%",
    )
    assert r_pct.humanize(60.0) == "60.0%"

    r_none = ModbusRegister(
        num=3,
        name="raw_count",
        reg_type="holding",
        access="r",
        data_type="float32",
    )
    assert r_none.humanize(100) == "100"


def test_modbus_register_comparisons():
    r1 = ModbusRegister(
        num=10, name="r1", reg_type="holding", access="r", data_type="float32"
    )
    r2 = ModbusRegister(
        num=20, name="r2", reg_type="holding", access="r", data_type="float32"
    )
    r3 = ModbusRegister(
        num=10, name="r3", reg_type="holding", access="w", data_type="float32"
    )

    assert r1 == r3
    assert r1 < r2
    assert r2 > r1
    assert not (r1 > r2)
    assert not (r2 < r1)
    assert sorted([r2, r1]) == [r1, r2]


# =============================================================================
# ModbusSettings, CsvSettings, ElasticSettings Tests
# =============================================================================


def test_modbus_settings():
    s = ModbusSettings()
    assert s.enabled is False
    assert s.ip == IPv4Address("0.0.0.0")
    assert s.port == 502
    assert s.timeout == 5.0

    with pytest.raises(ValidationError):
        ModbusSettings(port=0)

    with pytest.raises(ValidationError):
        ModbusSettings(timeout=-1.0)


def test_csv_settings():
    s = CsvSettings()
    assert s.enabled is False
    assert s.rows_per_file == 50000
    assert s.max_files == 3

    with pytest.raises(ValidationError):
        CsvSettings(rows_per_file=0)

    with pytest.raises(ValidationError):
        CsvSettings(max_files=-1)


def test_elastic_settings_http_url_validation():
    s = ElasticSettings(host="http://localhost:9200")
    assert s.host == "http://localhost:9200/"

    s2 = ElasticSettings(host="http://172.16.0.254:9200")
    assert s2.host == "http://172.16.0.254:9200/"

    with pytest.raises(ValidationError):
        ElasticSettings(host="invalid-url")

    with pytest.raises(ValidationError):
        ElasticSettings(host="ftp://localhost:9200")


# =============================================================================
# PybennuSettings & load_settings_yaml Tests
# =============================================================================


def test_pybennu_settings_defaults():
    cfg = PybennuSettings()
    assert cfg.server_endpoint == "tcp://172.16.1.2:5555"
    assert cfg.publish_endpoint == "udp://*;239.0.0.1:40000"
    assert cfg.debug is False
    assert isinstance(cfg.gtnet_skt, GtnetSktSettings)
    assert isinstance(cfg.pmu, PmuSettings)
    assert isinstance(cfg.modbus, ModbusSettings)
    assert isinstance(cfg.elastic, ElasticSettings)
    assert isinstance(cfg.csv, CsvSettings)
    assert isinstance(cfg.siren, SirenSettings)


def test_pybennu_settings_env_prefix_and_nested_delimiter(monkeypatch):
    monkeypatch.setenv("bennu_publish_rate", "3.5")
    monkeypatch.setenv("bennu_debug", "true")
    monkeypatch.setenv("bennu_modbus__port", "5020")
    monkeypatch.setenv("bennu_modbus__enabled", "true")
    monkeypatch.setenv("bennu_csv__rows_per_file", "25000")

    cfg = PybennuSettings()
    assert cfg.publish_rate == 3.5
    assert cfg.debug is True
    assert cfg.modbus.port == 5020
    assert cfg.modbus.enabled is True
    assert cfg.csv.rows_per_file == 25000


def test_load_settings_yaml_valid(tmp_path):
    yaml_content = """
server_endpoint: "tcp://10.0.0.5:5555"
publish_endpoint: "udp://*;239.0.0.1:40001"
publish_rate: 2.0
debug: true
modbus:
  enabled: true
  port: 1502
  registers:
    - num: 100
      name: "substation_power"
      reg_type: "holding"
      access: "r"
      data_type: "float32"
      unit_type: "MW"
"""
    yaml_file = tmp_path / "settings.yaml"
    yaml_file.write_text(yaml_content)

    cfg = load_settings_yaml(yaml_file)
    assert cfg.server_endpoint == "tcp://10.0.0.5:5555"
    assert cfg.publish_rate == 2.0
    assert cfg.debug is True
    assert cfg.modbus.enabled is True
    assert cfg.modbus.port == 1502
    assert len(cfg.modbus.registers) == 1
    assert cfg.modbus.registers[0].name == "substation_power"
    assert cfg.modbus.registers[0].sceptre_tag == "substation_power.value"
    assert cfg.modbus.registers[0].unit_type == "MW"


def test_load_settings_yaml_file_not_found(tmp_path):
    non_existent = tmp_path / "does_not_exist.yaml"
    with pytest.raises(FileNotFoundError, match="Non-existent or unreadable"):
        load_settings_yaml(non_existent)
