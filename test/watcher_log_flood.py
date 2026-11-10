#!/usr/bin/env python3
"""Regression test: bennu-watcherd must not flood its stderr log.

Runs the installed watcher (as root) against a fake device lockfile for a few
seconds and fails if watcher-stderr.log grows past a small bound. Upstream wrote
"pid = N" to stderr on every poll (3 times per 10 ms loop). Linux, root only.
"""
import os, subprocess, sys, tempfile, time

if os.getuid() != 0:
    print("SKIP: bennu-watcherd must run as root")
    sys.exit(77)

ENV = "floodtest"
RUN_SECONDS = 3
MAX_BYTES = 1024
LOGDIR = "/etc/sceptre/log"
STDERR_LOG = f"{LOGDIR}/watcher-stderr.log"
DEVICE_LOCK = f"/var/run/{ENV}-bennu-field-deviced.pid"
WATCHER_LOCK = f"/var/run/{ENV}-bennu-watcherd.pid"

os.makedirs(LOGDIR, exist_ok=True)
with tempfile.TemporaryDirectory() as d:
    firmware = os.path.join(d, "firmware")
    ini = os.path.join(d, "watcher.ini")
    with open(firmware, "w") as f:
        f.write("fw")
    with open(ini, "w") as f:
        f.write("[Hashes]\nreghash=0\nshellhashA=0\nshellhashB=0\n")
    with open(DEVICE_LOCK, "w") as f:  # a live-looking device: non-zero pid
        f.write(str(os.getpid()))
    cmd = ["bennu-watcherd", "--env", ENV, "--binary", firmware, "--watcher_config", ini]
    try:
        subprocess.run(cmd + ["start"], check=True, capture_output=True)
        time.sleep(RUN_SECONDS)
        size = os.path.getsize(STDERR_LOG)
        print(f"watcher-stderr.log bytes after {RUN_SECONDS}s: {size}")
        if size > MAX_BYTES:
            sys.exit(f"FAIL: watcher stderr log is {size} bytes (> {MAX_BYTES}), it is flooding")
    finally:
        subprocess.run(cmd + ["stop"], capture_output=True)
        for p in (DEVICE_LOCK, WATCHER_LOCK):
            if os.path.exists(p):
                os.remove(p)
print("PASS")
