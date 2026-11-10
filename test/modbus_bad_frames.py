#!/usr/bin/env python3
"""Regression test: Modbus/TCP server must drop truncated and oversized requests.

Starts bennu-field-device with the example modbus server config and sends
  1. a request cut short mid-body (client half-closes): must get no response, only EOF
  2. MBAP length fields larger than the 1024 B receive buffer (and one of 0): connection must
     be closed without the server dying
After each case the server must still answer a valid holding-register read.
"""
import os, socket, struct, subprocess, sys, tempfile, time

PORT = 25503
CFG = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "data", "configs", "ep", "modbus-server.xml")
REQ = struct.pack(">HHHBBHH", 1, 0, 6, 1, 3, 40000, 1)  # read 1 holding register


def poll():
    with socket.create_connection(("127.0.0.1", PORT), timeout=5) as s:
        s.sendall(REQ)
        return len(s.recv(256)) > 0


def drain(s):
    """Everything the server sends until it closes (or 3 s pass)."""
    s.settimeout(3)
    got = b""
    try:
        while True:
            d = s.recv(4096)
            if not d:
                return got, True
            got += d
    except socket.timeout:
        return got, False
    except ConnectionResetError:
        return got, True


with tempfile.TemporaryDirectory() as d:
    cfg = os.path.join(d, "modbus-server.xml")
    with open(CFG) as f, open(cfg, "w") as g:
        g.write(f.read().replace("127.0.0.1:5502", f"127.0.0.1:{PORT}"))
    log = open(os.path.join(d, "bad.out"), "w+")
    dev = subprocess.Popen(["bennu-field-device", "--file", cfg], cwd=d, stdout=log, stderr=log)

    def check(what):
        if dev.poll() is not None:
            sys.exit(f"FAIL: server died after {what}")
        try:
            if not poll():
                sys.exit(f"FAIL: server stopped answering after {what}")
        except OSError as e:
            sys.exit(f"FAIL: server stopped answering after {what}: {e}")

    try:
        for _ in range(50):
            try:
                assert poll()
                break
            except OSError:
                time.sleep(0.2)
        else:
            sys.exit("FAIL: server never answered")

        # 1. truncated request. A complete valid request first, so the server's receive buffer
        # holds a plausible stale request; then the same request cut after the function code.
        with socket.create_connection(("127.0.0.1", PORT), timeout=5) as s:
            s.sendall(REQ)
            if len(s.recv(256)) == 0:
                sys.exit("FAIL: no response to valid request")
            s.sendall(REQ[:8])
            s.shutdown(socket.SHUT_WR)
            got, closed = drain(s)
        if got:
            sys.exit(f"FAIL: server answered a truncated request: {got.hex()}")
        if not closed:
            sys.exit("FAIL: server did not close the connection after a truncated request")
        check("truncated request")
        print("truncated request: dropped, connection closed")

        # 2. MBAP length larger than the receive buffer (and an impossible length of 0)
        for length in (0xFFFF, 4000, 0):
            with socket.create_connection(("127.0.0.1", PORT), timeout=5) as s:
                s.sendall(struct.pack(">HHH", 1, 0, length))
                try:
                    s.sendall(b"\x41" * (length if length else 8))
                except OSError:
                    pass  # server already closed on us: fine
                got, closed = drain(s)
            if dev.poll() is not None:
                sys.exit(f"FAIL: server died on MBAP length {length}")
            if not closed:
                sys.exit(f"FAIL: connection with MBAP length {length} left open")
            check(f"MBAP length {length}")
            print(f"MBAP length {length}: connection closed, server alive")
    finally:
        dev.kill()
        dev.wait()
print("PASS")
