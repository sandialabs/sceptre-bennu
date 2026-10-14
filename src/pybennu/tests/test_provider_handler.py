import pytest

from pybennu.distributed.provider import Provider


class Stub(Provider):
    def __init__(self):
        super().__init__("tcp://x", "udp://y")
        self.writes = []

    def query(self):
        return "ACK=a,b"

    def read(self, tag):
        return f"ACK={tag}:1"

    def write(self, tags):
        self.writes.append(tags)
        return "ACK=ok"

    def periodic_publish(self):
        pass


@pytest.fixture
def h():
    s = Stub()
    return s, s._Provider__message_handler


def test_query_read(h):
    _, handle = h
    assert handle("QUERY=") == "ACK=a,b"
    assert handle("READ=brkr") == "ACK=brkr:1"


def test_write_single_and_batch(h):
    s, handle = h
    assert handle("WRITE=a:true") == "ACK=ok"
    assert handle("WRITE=a:true,b:2.5,") == "ACK=ok"
    assert s.writes == [{"a": "true"}, {"a": "true", "b": "2.5"}]


def test_write_equals_in_value(h):
    s, handle = h
    handle("WRITE=a:x=y")
    assert s.writes == [{"a": "x=y"}]


def test_errors(h):
    s, handle = h
    assert handle("") == "ERR=Message empty"
    assert handle("nonsense") == "ERR=Invalid message request"
    assert handle("BOGUS=1") == "ERR=Unknown command type 'BOGUS'"
    assert handle("WRITE=a:1,novalue").startswith("ERR=")
    assert s.writes == []
