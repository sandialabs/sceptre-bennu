#!/usr/bin/env python3
"""Regression test: IEC-104 server must keep reverse-polling the active master when other
clients connect or disconnect.

Starts bennu-field-device with the example 104 server config (reverse poll every 1 s). Master A
connects and sends STARTDT; a second client B then connects (not activated) and later closes.
A must keep receiving I-frames the whole time, and (if the binary is a sanitizer build) the
server must report no ThreadSanitizer race in its own Server.cpp and no AddressSanitizer error. Before the fix the server tracked a single
connection: B's connect replaced A as the poll target and B's close stopped polling altogether.
"""
import os, socket, struct, subprocess, sys, tempfile, time


def sanitizer_findings(out):
    """TSan data races whose top frame is in a comms Server.cpp, and any ASan error. Races inside the
    vendored lib60870 (cs104_slave.c) are a known, separate issue and are not counted here."""
    bad = [l for l in out.splitlines() if l.startswith("SUMMARY: ThreadSanitizer") and ("/module/Server.cpp" in l or "-module.so" in l)]
    return bad + [l for l in out.splitlines() if "ERROR: AddressSanitizer" in l]

PORT = 25404
CFG = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "data", "configs", "ep", "iec60870-5-104-server.xml")


class Master:
    def __init__(self):
        self.s = socket.create_connection(("127.0.0.1", PORT), timeout=5)
        self.buf = b""
        self.rx = 0  # I-frames received (N(R) to acknowledge)

    def startdt(self):
        self.s.sendall(bytes([0x68, 0x04, 0x07, 0, 0, 0]))
        t = time.time() + 5
        while time.time() < t:
            if self.frames(0.5, until_u=0x0B):
                return
        sys.exit("FAIL: no STARTDT_CON")

    def frames(self, secs, until_u=None):
        """Read for secs; ack I-frames; return how many I-frames arrived (or True on STARTDT_CON)."""
        end = time.time() + secs
        n = 0
        while time.time() < end:
            self.s.settimeout(max(0.05, end - time.time()))
            try:
                d = self.s.recv(4096)
            except socket.timeout:
                break
            if not d:
                break
            self.buf += d
            while len(self.buf) >= 2 and len(self.buf) >= self.buf[1] + 2:
                f, self.buf = self.buf[: self.buf[1] + 2], self.buf[self.buf[1] + 2 :]
                if f[2] & 1 == 0:
                    n += 1
                    self.rx += 1
                    self.s.sendall(bytes([0x68, 0x04, 0x01, 0]) + struct.pack("<H", self.rx << 1))
                elif until_u is not None and f[2] == until_u:
                    return True
        return n

    def close(self):
        self.s.close()


with tempfile.TemporaryDirectory() as d:
    cfg = os.path.join(d, "iec104-server.xml")
    with open(CFG) as f, open(cfg, "w") as g:
        g.write(f.read().replace("127.0.0.1:2404", f"127.0.0.1:{PORT}"))
    log = open(os.path.join(d, "104.out"), "w+")
    dev = subprocess.Popen(["bennu-field-device", "--file", cfg], cwd=d, stdout=log, stderr=log)
    try:
        for _ in range(50):
            try:
                a = Master()
                break
            except OSError:
                time.sleep(0.2)
        else:
            sys.exit("FAIL: server never accepted")
        a.startdt()
        if a.frames(4) == 0:
            sys.exit("FAIL: A got no data after STARTDT")
        print("A active, receiving")

        b = Master()  # second client connects; A is still the active master
        if a.frames(4) == 0:
            sys.exit("FAIL: A stopped receiving after a second client connected")
        print("A still receiving with B connected")

        b.close()
        if a.frames(4) == 0:
            sys.exit("FAIL: A stopped receiving after the second client closed")
        print("A still receiving after B closed")

        # failover: A closes, B takes over
        a.close()
        c = Master()
        c.startdt()
        if c.frames(4) == 0:
            sys.exit("FAIL: new master C got no data")
        print("C active after A closed, receiving")
        if dev.poll() is not None:
            sys.exit("FAIL: server died")
        # under a -fsanitize=thread/address build, a data race or memory error shows up here
        log.seek(0)
        out = log.read()
        if sanitizer_findings(out):
            sys.exit("FAIL: sanitizer report\n" + "\n".join(sanitizer_findings(out)))
    finally:
        dev.kill()
        dev.wait()
print("PASS")
