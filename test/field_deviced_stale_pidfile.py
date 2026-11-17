#!/usr/bin/env python3
"""Regression test: bennu-field-deviced must treat a pidfile whose process is gone as stale.

After a crash the pidfile still holds the dead pid. Upstream isAlreadyRunning() only checked for a
non-zero pid, so `--c start` refused forever ("already running") until someone ran `--c restart`.
Runs the installed daemon as root with a pidfile naming a reaped pid; it must start, record a live
pid, and stop cleanly. Linux, root only.
"""
import os, subprocess, sys, tempfile, time

if os.getuid() != 0:
    print("SKIP: bennu-field-deviced must run as root")
    sys.exit(77)

ENV = "stalepidtest"
PIDFILE = f"/var/run/{ENV}-bennu-field-deviced.pid"
CONF = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "data", "configs", "ep", "dnp3-server.xml")


def alive(pid):
    try:
        os.kill(pid, 0)
        return True
    except ProcessLookupError:
        return False


os.makedirs("/etc/sceptre/log", exist_ok=True)
dead = subprocess.Popen(["true"])
dead.wait()  # reaped: this pid no longer exists
assert not alive(dead.pid)

with tempfile.TemporaryDirectory() as d:
    cfg = os.path.join(d, "dnp3-server.xml")
    with open(CONF) as f, open(cfg, "w") as g:
        g.write(f.read().replace("127.0.0.1:20000", "127.0.0.1:20997"))
    with open(PIDFILE, "w") as f:
        f.write(str(dead.pid))
    cmd = ["bennu-field-deviced", "--env", ENV, "--file", cfg]
    try:
        r = subprocess.run(cmd + ["--c", "start"], cwd=d, capture_output=True, text=True, timeout=30)
        print(f"start: rc={r.returncode} {r.stderr.strip()}")
        if r.returncode != 0:
            sys.exit(f"FAIL: start refused with a stale pidfile (pid {dead.pid} is gone)")
        for _ in range(50):
            pid = int(open(PIDFILE).read() or 0)
            if pid != dead.pid:
                break
            time.sleep(0.1)
        if pid == dead.pid or not alive(pid):
            sys.exit(f"FAIL: pidfile holds {pid}, daemon is not running")
        print(f"daemon running as pid {pid}")
    finally:
        subprocess.run(cmd + ["--c", "stop"], cwd=d, capture_output=True)
        if os.path.exists(PIDFILE):
            os.remove(PIDFILE)
print("PASS")
