"""Helpers for pybennu distributed messaging (no zmq/swig imports)."""


def chunk_message(msg, mtu):
    """Split a comma-delimited message into "part," chunks shorter than mtu.

    Empty parts are skipped; a single part longer than mtu is returned alone.
    """
    chunks = []
    chunk = ""
    for p in msg.split(","):
        if not p:
            continue
        part = p + ","
        if chunk and len(chunk) + len(part) >= mtu:
            chunks.append(chunk)
            chunk = ""
        chunk += part
    if chunk:
        chunks.append(chunk)
    return chunks


def build_write_payload(points):
    """Format {tag: value} as "a:1,b:true" (bools as true/false, like C++ Client::buildWritePayload)."""
    return ",".join(
        f"{t}:{('true' if v else 'false') if isinstance(v, bool) else v}"
        for t, v in points.items()
    )
