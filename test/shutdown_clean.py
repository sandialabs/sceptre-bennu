#!/usr/bin/env python3
"""Regression test: bennu-field-device with a DNP3 or IEC-104 server must exit cleanly on SIGTERM
and SIGINT: exit status 0, within a few seconds, no hang or crash.

Before the fix SIGTERM was not handled (killed by signal, status -15) and the servers' threads
(DNP3 Server::update, IEC-104 reverse poll) were `while (1)` raw threads that were never stopped or
joined. The IEC-104 case keeps a master activated so the reverse-poll thread is sending when the
signal arrives. Run against a -fsanitize=address/thread build, a ThreadSanitizer race
from the servers' own Server.cpp also fails the test (races inside the vendored lib60870 are not counted).
"""
import os, signal, socket, subprocess, sys, tempfile, time

HERE = os.path.dirname(os.path.abspath(__file__))
CONF = os.path.join(HERE, "..", "data", "configs", "ep")
BOUND = 8  # seconds from signal to exit


def master_104(port):
    s = socket.create_connection(("127.0.0.1", port), timeout=5)
    s.sendall(bytes([0x68, 0x04, 0x07, 0, 0, 0]))  # STARTDT_ACT
    s.settimeout(5)
    if s.recv(6)[2] != 0x0B:
        sys.exit("FAIL: no STARTDT_CON")
    return s



def sanitizer_findings(out):
    """TSan data races whose top frame is in a comms Server.cpp, and any ASan error. Races inside the
    vendored lib60870 (cs104_slave.c) are a known, separate issue and are not counted here."""
    bad = [l for l in out.splitlines() if l.startswith("SUMMARY: ThreadSanitizer") and ("/module/Server.cpp" in l or "-module.so" in l)]
    return bad + [l for l in out.splitlines() if "ERROR: AddressSanitizer" in l]


CASES = [
    ("dnp3", "dnp3-server.xml", "127.0.0.1:20000", 20998, lambda p: socket.create_connection(("127.0.0.1", p), timeout=5)),
    ("iec104", "iec60870-5-104-server.xml", "127.0.0.1:2404", 25405, master_104),
]

for name, conf, addr, port, connect in CASES:
    for sig in (signal.SIGTERM, signal.SIGINT):
        with tempfile.TemporaryDirectory() as d:
            cfg = os.path.join(d, conf)
            with open(os.path.join(CONF, conf)) as f, open(cfg, "w") as g:
                g.write(f.read().replace(addr, f"127.0.0.1:{port}"))
            log = open(os.path.join(d, "out.log"), "w+")
            dev = subprocess.Popen(["bennu-field-device", "--file", cfg], cwd=d, stdout=log, stderr=log)
            try:
                for _ in range(50):
                    try:
                        client = connect(port)
                        break
                    except OSError:
                        time.sleep(0.2)
                else:
                    sys.exit(f"FAIL {name}: server never accepted")
                time.sleep(2)  # let the update / reverse-poll thread run a cycle
                t0 = time.time()
                dev.send_signal(sig)
                try:
                    rc = dev.wait(BOUND)
                except subprocess.TimeoutExpired:
                    sys.exit(f"FAIL {name} {sig.name}: still running {BOUND} s after the signal (hang)")
                took = time.time() - t0
                client.close()
                log.seek(0)
                out = log.read()
                if rc != 0:
                    sys.exit(f"FAIL {name} {sig.name}: exit status {rc}\n{out[-2000:]}")
                if sanitizer_findings(out):
                    sys.exit(f"FAIL {name} {sig.name}: sanitizer report\n" + "\n".join(sanitizer_findings(out)))
                print(f"{name} {sig.name}: exit 0 in {took:.1f} s")
            finally:
                if dev.poll() is None:
                    dev.kill()
                    dev.wait()
print("PASS")
