#!/usr/bin/env python3
"""Regression test: a holding register and an input register mapped at the same address
must each be scaled with their own min/max range (they are separate Modbus address spaces)."""
import os, socket, struct, subprocess, sys, tempfile, time

PORT = 25505
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


def read(fc, addr):
    r = txn(struct.pack(">BHH", fc, addr, 1))
    return struct.unpack(">H", r[9:11])[0] if len(r) >= 11 and r[7] == fc else r.hex()


with tempfile.TemporaryDirectory() as d:
    x = open(CFG).read().replace("127.0.0.1:5502", f"127.0.0.1:{PORT}")
    x = x.replace("<internal-tag>\n                <name>counter</name>",
        "<internal-tag><name>hA</name><value>500.0</value></internal-tag>"
        "<internal-tag><name>iB</name><value>50.0</value></internal-tag>"
        "<internal-tag>\n                <name>counter</name>", 1)
    x = x.replace("</modbus-server>",
        "<holding-register><address>100</address><max-value>1000</max-value><min-value>0</min-value><tag>hA</tag></holding-register>"
        "<input-register><address>100</address><max-value>100</max-value><min-value>0</min-value><tag>iB</tag></input-register>"
        "</modbus-server>")
    cfg = os.path.join(d, "m.xml")
    open(cfg, "w").write(x)
    log = open(os.path.join(d, "o"), "w+")
    dev = subprocess.Popen(["bennu-field-device", "--file", cfg], cwd=d, stdout=log, stderr=log)
    bugs = []
    try:
        for _ in range(50):
            try:
                txn(struct.pack(">BHH", 3, 40000, 1))
                break
            except OSError:
                time.sleep(0.2)
        hv, iv = read(3, 100), read(4, 100)
        print("holding@100 (500 of 0..1000) ->", hv, " input@100 (50 of 0..100) ->", iv, "(want 32767 both, +-1)")
        for name, v in (("holding", hv), ("input", iv)):
            if not isinstance(v, int) or abs(v - 32767) > 1:
                bugs.append(f"{name}@100 read {v}, want 32767")
    finally:
        dev.kill()
        dev.wait()
        print(open(os.path.join(d, "o")).read()[-300:])
if bugs:
    sys.exit("FAIL: " + "; ".join(bugs))
print("PASS")
