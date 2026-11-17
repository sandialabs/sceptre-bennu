#!/usr/bin/env python3
"""Regression test: the 104 client must keep updating its binary tags from the server's periodic
(reverse poll) binary ASDUs, not only from the one-off interrogation response.

The double-point server reverse-polls each binary point as M_DP_TB_1 (type 31), the single-point
server as M_SP_NA_1 (type 1). The client used to decode only M_DP_NA_1 (type 3), so after the
interrogation its binary tags never changed again. The client prints "IOA: <n> value: <v>" for every
decoded binary point; with a 1 s reverse poll several seconds must yield several lines for IOA 100.
"""
import os, subprocess, sys, tempfile, time

CONFIGS = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "data", "configs", "ep")
PORT = 25407


def run(subtype):
    with tempfile.TemporaryDirectory() as d:
        srv = open(CONFIGS + "/iec60870-5-104-server.xml").read().replace("2404", str(PORT))
        srv = srv.replace("<subtype>double</subtype>", f"<subtype>{subtype}</subtype>")
        open(d + "/s.xml", "w").write(srv)
        open(d + "/c.xml", "w").write(open(CONFIGS + "/iec60870-5-104-client.xml").read().replace("2404", str(PORT)))
        so = open(d + "/so", "w+")
        co = open(d + "/co", "w+")
        sp = subprocess.Popen(["stdbuf", "-oL", "bennu-field-device", "--file", d + "/s.xml"], cwd=d, stdout=so, stderr=so)
        time.sleep(2)
        cp = subprocess.Popen(["stdbuf", "-oL", "bennu-field-device", "--file", d + "/c.xml"], cwd=d, stdout=co, stderr=co)
        time.sleep(8)
        sp.kill(); cp.kill(); sp.wait(); cp.wait()
        co.seek(0)
        out = co.read()
    types = sorted({l for l in out.splitlines() if l.startswith("RECVD ASDU")})
    n = sum(1 for l in out.splitlines() if l.strip().startswith("IOA: 100 value:"))
    print(f"subtype={subtype}: binary IOA 100 decoded {n} times; ASDU types seen:")
    print("\n".join("  " + t for t in types))
    return n


bad = [f"{st}: client decoded binary IOA 100 only {n} time(s) in 8 s" for st in ("double", "single") if (n := run(st)) < 4]
if bad:
    sys.exit("FAIL:\n  " + "\n  ".join(bad))
print("PASS")
