#!/usr/bin/env python3
"""Regression test: the 104 server must not truncate the 24-bit IOA to 16 bits.

IOA 65536 (0x010000) used to be executed as IOA 0, a configured binary output: the master got a
positive ACT_CON and the output was written. It is not a configured point, so the server must
answer with a negative ACT_CON and not write. IOA 0 itself must still work (positive ACT_CON, one
write) so the test cannot pass by rejecting everything.
"""
import os, socket, struct, subprocess, sys, tempfile, time

PORT = 25405
CFG = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "data", "configs", "ep", "iec60870-5-104-server.xml")


def rd(s, secs):
    s.settimeout(0.3)
    end = time.time() + secs
    buf = b""
    frames = []
    while time.time() < end:
        try:
            d = s.recv(4096)
        except socket.timeout:
            continue
        if not d:
            break
        buf += d
        while len(buf) >= 2 and len(buf) >= buf[1] + 2:
            frames.append(buf[:buf[1] + 2])
            buf = buf[buf[1] + 2:]
    return frames


seen = 0  # I-frames received from the server, sent back as N(R)


def single_command(s, ioa, seq):
    """Send a direct-operate C_SC_NA_1 (ON) to ioa, return the ACT_CON negative flag (None if none)."""
    global seen
    asdu = bytes([45, 1, 6, 0, 1, 0]) + struct.pack("<I", ioa)[:3] + bytes([0x01])
    s.sendall(bytes([0x68, 4 + len(asdu)]) + struct.pack("<HH", seq << 1, seen << 1) + asdu)
    negative = None
    for f in rd(s, 2):
        if f[2] & 1 == 0:  # I-frame
            seen += 1
            if f[6] == 45 and (f[8] & 0x3f) == 7:  # ACT_CON
                negative = bool(f[8] & 0x40)
    return negative


with tempfile.TemporaryDirectory() as d:
    cfg = os.path.join(d, "s.xml")
    open(cfg, "w").write(open(CFG).read().replace("127.0.0.1:2404", f"127.0.0.1:{PORT}"))
    log = open(os.path.join(d, "o"), "w+")
    dev = subprocess.Popen(["stdbuf", "-oL", "bennu-field-device", "--file", cfg], cwd=d, stdout=log, stderr=log)
    try:
        for _ in range(50):
            try:
                s = socket.create_connection(("127.0.0.1", PORT), timeout=5)
                break
            except OSError:
                time.sleep(0.2)
        else:
            sys.exit("no server")
        s.sendall(bytes([0x68, 4, 7, 0, 0, 0]))  # STARTDT act
        rd(s, 1)
        alias = single_command(s, 65536, 0)  # aliases IOA 0
        valid = single_command(s, 0, 1)
        log.seek(0)
        writes = log.read().count("switch to")
        print(f"IOA 65536 ACT_CON negative={alias}; IOA 0 ACT_CON negative={valid}; writes={writes}")
        bad = []
        if alias is not True:
            bad.append("command to IOA 65536 not rejected with a negative ACT_CON")
        if valid is not False:
            bad.append("command to configured IOA 0 was not accepted")
        if writes != 1:
            bad.append(f"expected exactly 1 write (IOA 0), saw {writes}")
        if bad:
            sys.exit("FAIL:\n  " + "\n  ".join(bad))
    finally:
        dev.kill()
        dev.wait()
print("PASS")
