import importlib.util
import pathlib

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
