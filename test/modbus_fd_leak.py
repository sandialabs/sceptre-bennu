#!/usr/bin/env python3
"""Regression test: Modbus/TCP server must release the socket of every closed client.

Starts bennu-field-device with the example modbus server config, opens and closes
N connections (one holding-register read each), then fails if socket fds grew or
the server stopped answering. Linux only (/proc).
"""
import os, socket, struct, subprocess, sys, tempfile, time

N = int(sys.argv[1]) if len(sys.argv) > 1 else 2000
PORT = 25502
CFG = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "data", "configs", "ep", "modbus-server.xml")
REQ = struct.pack(">HHHBBHH", 1, 0, 6, 1, 3, 40000, 1)  # read 1 holding register


def sockets(pid):
    n = 0
    for f in os.listdir(f"/proc/{pid}/fd"):
        try:
            n += os.readlink(f"/proc/{pid}/fd/{f}").startswith("socket:")
        except OSError:
            pass
    return n


def poll():
    with socket.create_connection(("127.0.0.1", PORT), timeout=5) as s:
        s.sendall(REQ)
        return len(s.recv(256)) > 0


with tempfile.TemporaryDirectory() as d:
    cfg = os.path.join(d, "modbus-server.xml")
    with open(CFG) as f, open(cfg, "w") as g:
        g.write(f.read().replace("127.0.0.1:5502", f"127.0.0.1:{PORT}"))
    log = open(os.path.join(d, "fd.out"), "w+")
    dev = subprocess.Popen(["bennu-field-device", "--file", cfg], cwd=d, stdout=log, stderr=log)
    try:
        for _ in range(50):
            try:
                assert poll()
                break
            except OSError:
                time.sleep(0.2)
        else:
            sys.exit("FAIL: server never answered")
        before = sockets(dev.pid)
        for i in range(N):
            try:
                poll()
            except OSError as e:
                sys.exit(f"FAIL: connection {i} (sockets={sockets(dev.pid)}): {e}")
        time.sleep(1)
        after = sockets(dev.pid)
        logsize = os.path.getsize(log.name)
        print(f"sockets before={before} after={after} cycles={N} log_bytes={logsize}")
        if dev.poll() is not None:
            sys.exit("FAIL: server died")
        if after - before > 5:
            sys.exit(f"FAIL: leaked {after - before} socket fds")
        if not poll():
            sys.exit("FAIL: server stopped answering")
        if logsize > 100 * N:
            sys.exit("FAIL: excessive logging")
    finally:
        dev.kill()
        dev.wait()
print("PASS")
