import importlib.util
import pathlib

import pytest

# load utils.py directly: importing pybennu.distributed would pull in zmq/swig
_p = pathlib.Path(__file__).parents[1] / "pybennu" / "distributed" / "utils.py"
_spec = importlib.util.spec_from_file_location("bennu_utils", _p)
utils = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(utils)
chunk_message = utils.chunk_message

MTU = 1465


def tags(n, trailing):
    s = ",".join(f"tag{i}:{i}.5" for i in range(n))
    return s + "," if trailing else s


def delivered(chunks):
    return {p for c in chunks for p in c.split(",") if p}


def test_800_tags_all_delivered():
    for trailing in (False, True):
        chunks = chunk_message(tags(800, trailing), MTU)
        assert len(delivered(chunks)) == 800
        assert all(len(c) < MTU for c in chunks)


def test_edges():
    assert chunk_message("", 100) == []
    assert chunk_message(",,,", 100) == []
    assert chunk_message("a:1,b:2", 100) == ["a:1,b:2,"]
    assert chunk_message("a:1," + "x" * 50 + ",b:2", 20) == [
        "a:1,",
        "x" * 50 + ",",
        "b:2,",
    ]


def test_build_write_payload():
    f = utils.build_write_payload
    assert f({"a": 1, "b": True, "c": False, "d": 2.5}) == "a:1,b:true,c:false,d:2.5"
    assert f({}) == ""


def test_publish_datagrams_fit_link_mtu():
    # 15-char group, as Endpoint.hash() gives: 1 + 15 + body must fit 1500 - 28
    limit = utils.publish_body_limit(1500, 15)
    assert limit == 1456
    chunks = chunk_message(tags(800, True), limit + 1)
    assert len(delivered(chunks)) == 800
    assert max(1 + 15 + len(c) for c in chunks) <= 1472


def test_publish_link_mtu_env(monkeypatch):
    monkeypatch.delenv("BENNU_PUBLISH_MTU", raising=False)
    assert utils.publish_link_mtu() == 1500
    monkeypatch.setenv("BENNU_PUBLISH_MTU", "1450")
    assert utils.publish_link_mtu() == 1450
    for bad in (
        "abc",
        "-1",
        "1450.5",
        "575",
        "8221",
        " 1500",
        "+1500",
        "\u0661\u0665\u0660\u0660",
    ):
        monkeypatch.setenv("BENNU_PUBLISH_MTU", bad)
        with pytest.raises(ValueError, match="BENNU_PUBLISH_MTU"):
            utils.publish_link_mtu()


def test_chunk_message_counts_utf8_bytes():
    # 3-byte chars: 30 chars is 90 B, so a 60 B limit must split them
    msg = ",".join(["\u20ac" * 10] * 3)
    chunks = chunk_message(msg, 60)
    assert len(chunks) == 3
    assert all(len(c.encode()) < 60 for c in chunks)
    assert "".join(chunks).rstrip(",").split(",") == msg.split(",")
