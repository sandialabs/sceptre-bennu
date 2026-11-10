#!/usr/bin/env python3
"""Regression test: DNP3 outstation must keep accepting after an accept() error.

Starts bennu-field-device with the example dnp3 server config, then lowers its fd limit to the
lowest free fd so the next accept() fails with EMFILE, connects a master (waits in the kernel
backlog), restores the limit and expects the outstation to answer a link status request.
Before the fix the outstation treated the accept error as a shutdown and never accepted again.
Linux only (/proc, prlimit); the fd limit can only be lowered for a process we own.
"""
import os, resource, socket, subprocess, sys, tempfile, time

PORT = 20999
CFG = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "data", "configs", "ep", "dnp3-server.xml")


def crc(data):
    c = 0
    for b in data:
        c ^= b
        for _ in range(8):
            c = (c >> 1) ^ 0xA6BC if c & 1 else c >> 1
    return ~c & 0xFFFF


def link_status_request(dest=10, src=1):
    hdr = bytes([0x05, 0x64, 0x05, 0xC9]) + dest.to_bytes(2, "little") + src.to_bytes(2, "little")
    return hdr + crc(hdr).to_bytes(2, "little")


def answers(timeout):
    """Connect, send a link status request, return True if a DNP3 frame comes back."""
    with socket.create_connection(("127.0.0.1", PORT), timeout=5) as s:
        s.sendall(link_status_request())
        s.settimeout(timeout)
        try:
            return s.recv(64)[:2] == b"\x05\x64"
        except (socket.timeout, ConnectionError):
            return False


def lowest_free_fd(pid):
    used = {int(f) for f in os.listdir(f"/proc/{pid}/fd")}
    return next(i for i in range(len(used) + 2) if i not in used)


with tempfile.TemporaryDirectory() as d:
    cfg = os.path.join(d, "dnp3-server.xml")
    with open(CFG) as f, open(cfg, "w") as g:
        g.write(f.read().replace("127.0.0.1:20000", f"127.0.0.1:{PORT}"))
    log = open(os.path.join(d, "dnp3.out"), "w+")
    dev = subprocess.Popen(["bennu-field-device", "--file", cfg], cwd=d, stdout=log, stderr=log)
    old = resource.getrlimit(resource.RLIMIT_NOFILE)
    try:
        for _ in range(50):
            try:
                if answers(1):
                    break
            except OSError:
                pass
            time.sleep(0.2)
        else:
            sys.exit("FAIL: outstation never answered")

        # fds are allocated lowest-first, so a limit equal to the lowest free fd makes the
        # next accept() fail with EMFILE
        time.sleep(1)
        lim = lowest_free_fd(dev.pid)
        resource.prlimit(dev.pid, resource.RLIMIT_NOFILE, (lim, old[1]))
        s = socket.create_connection(("127.0.0.1", PORT), timeout=5)
        s.sendall(link_status_request())
        time.sleep(1)  # outstation hits EMFILE here
        resource.prlimit(dev.pid, resource.RLIMIT_NOFILE, old)

        s.settimeout(10)
        try:
            ok = s.recv(64)[:2] == b"\x05\x64"
        except (socket.timeout, ConnectionError):
            ok = False
        s.close()
        if dev.poll() is not None:
            sys.exit("FAIL: outstation died")
        if not ok:
            sys.exit("FAIL: outstation did not resume accepting after an accept() error")
        if not answers(5):
            sys.exit("FAIL: outstation does not answer new connections")
        print(f"fd limit lowered to {lim} and restored: outstation resumed accepting")
    finally:
        dev.kill()
        dev.wait()
print("PASS")
