"""Helpers for pybennu distributed messaging (no zmq/swig imports)."""

import os

# libzmq sends a RADIO message over UDP as [group length (1 B)][group][body], at most
# 8192 B, inside 28 B of IPv4 and UDP headers. A datagram larger than the link MTU is
# IP-fragmented, and losing either fragment loses the whole datagram.
IP_UDP_HEADERS = 28
MAX_UDP_MSG = 8192


def publish_link_mtu():
    """Link MTU for publish datagrams from env BENNU_PUBLISH_MTU (default 1500).

    Raises ValueError if it is not an integer from 576 to 8220.
    """
    raw = os.environ.get("BENNU_PUBLISH_MTU", "")
    if not raw:
        return 1500
    if (
        not (raw.isascii() and raw.isdigit())
        or not 576 <= int(raw) <= MAX_UDP_MSG + IP_UDP_HEADERS
    ):
        raise ValueError(
            f"BENNU_PUBLISH_MTU must be an integer link MTU from 576 to 8220, got {raw!r}"
        )
    return int(raw)


def publish_body_limit(link_mtu, group_length):
    """Largest datagram body, in bytes, that fits link_mtu for this group."""
    return link_mtu - IP_UDP_HEADERS - 1 - group_length


def chunk_message(msg, mtu):
    """Split a comma-delimited message into "part," chunks shorter than mtu bytes.

    Sizes are UTF-8 bytes, as sent on the wire; parts are never split.

    Empty parts are skipped; a single part longer than mtu is returned alone.
    """
    chunks = []
    chunk, size = "", 0
    for p in msg.split(","):
        if not p:
            continue
        part = p + ","
        n = len(part.encode())
        if chunk and size + n >= mtu:
            chunks.append(chunk)
            chunk, size = "", 0
        chunk += part
        size += n
    if chunk:
        chunks.append(chunk)
    return chunks


def build_write_payload(points):
    """Format {tag: value} as "a:1,b:true" (bools as true/false, like C++ Client::buildWritePayload)."""
    return ",".join(
        f"{t}:{('true' if v else 'false') if isinstance(v, bool) else v}"
        for t, v in points.items()
    )
