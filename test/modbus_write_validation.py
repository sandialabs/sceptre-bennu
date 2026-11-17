#!/usr/bin/env python3
"""Regression test: Modbus/TCP server must reject FC15/FC16 requests whose data bytes are
missing or whose quantity is out of range, and must not update any tag for them.

A well-formed FC16 write must still succeed."""
import os, socket, struct, subprocess, sys, tempfile, time

PORT = 25504
CFG = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "data", "configs", "ep", "modbus-server.xml")


def txn(pdu):
    req = struct.pack(">HHHB", 1, 0, len(pdu) + 1, 1) + pdu
    with socket.create_connection(("127.0.0.1", PORT), timeout=5) as s:
        s.sendall(req)
        s.settimeout(3)
        try:
            return s.recv(512)
        except socket.timeout:
            return b""


def read_hr():
    r = txn(struct.pack(">BHH", 3, 40000, 1))
    return r[7:] if len(r) > 7 else r


with tempfile.TemporaryDirectory() as d:
    cfg = os.path.join(d, "modbus-server.xml")
    with open(CFG) as f, open(cfg, "w") as g:
        g.write(f.read().replace("127.0.0.1:5502", f"127.0.0.1:{PORT}"))
    log = open(os.path.join(d, "o"), "w+")
    dev = subprocess.Popen(["bennu-field-device", "--file", cfg], cwd=d, stdout=log, stderr=log)
    bugs = []
    try:
        for _ in range(50):
            try:
                read_hr()
                break
            except OSError:
                time.sleep(0.2)
        before = read_hr()
        bad = {
            "FC16 no data bytes": struct.pack(">BHHB", 16, 40000, 1, 2),
            "FC15 no data bytes": struct.pack(">BHHB", 15, 0, 1, 1),
            "FC16 PDU ends before byte count": struct.pack(">BHH", 16, 40000, 1),
            "FC16 qty 128 byte count 0 (8-bit wrap)": struct.pack(">BHHB", 16, 40000, 128, 0),
            "FC16 qty 124 (> 123), no data": struct.pack(">BHHB", 16, 40000, 124, 248),
            "FC15 qty 1969 (> 1968), no data": struct.pack(">BHHB", 15, 0, 1969, 247),
            "FC16 short data": struct.pack(">BHHB", 16, 40000, 2, 4) + b"\x12\x34",
        }
        for name, pdu in bad.items():
            r = txn(pdu)
            print(name, "->", r.hex())
            if len(r) < 9 or r[7] != pdu[0] | 0x80 or r[8] != 3:
                bugs.append(f"{name}: expected exception 03, got {r.hex()}")
        if read_hr() != before:
            bugs.append("a rejected write changed the holding register")
        r = txn(struct.pack(">BHHBH", 16, 40000, 1, 2, 0x1234))
        if len(r) < 8 or r[7] != 16:
            bugs.append(f"valid FC16 rejected: {r.hex()}")
        if dev.poll() is not None:
            bugs.append("server died")
    finally:
        dev.kill()
        dev.wait()
        print(open(os.path.join(d, "o")).read()[-400:])
if bugs:
    sys.exit("FAIL: " + "; ".join(bugs))
print("PASS")
