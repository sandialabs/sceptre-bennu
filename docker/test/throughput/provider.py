#!/usr/bin/env python3
"""Mock bennu provider (REP). Accepts any tag, timestamps every WRITE, prints a summary.
usage: provider.py <endpoint> <seconds> <tag-to-time>
Set THROUGHPUT_MAX_CYCLE_S to fail (exit 1) when the mean output cycle is slower than that."""
import os, sys, time, zmq

ep, secs, probe = sys.argv[1], float(sys.argv[2]), sys.argv[3]
s = zmq.Context().socket(zmq.REP)
s.bind(ep)
reqs, tags, probe_ts = 0, 0, []
start = None
t0 = time.time()
while True:
    now = time.time()
    if not start and now - t0 > secs + 30:
        sys.exit('THROUGHPUT no WRITE received')
    if start and now - start > secs:
        break
    if not s.poll(500):
        continue
    msg = s.recv().rstrip(b"\0").decode()
    op, _, payload = msg.partition("=")
    if op.upper() == "WRITE":
        now = time.time()
        start = start or now
        pts = [p.split(":")[0] for p in payload.split(",") if p]
        reqs += 1
        tags += len(pts)
        if probe in pts:
            probe_ts.append(now)
    s.send(b"ACK=ok\0")
elapsed = time.time() - start
gaps = [b - a for a, b in zip(probe_ts, probe_ts[1:])]
print(f"THROUGHPUT window={elapsed:.1f}s requests={reqs} tag_writes={tags} "
      f"requests_per_sec={reqs/elapsed:.2f} tag_writes_per_sec={tags/elapsed:.2f}")
cycle = sum(gaps) / len(gaps) if gaps else None
if cycle is not None:
    print(f"THROUGHPUT cycles_observed={len(gaps)} seconds_per_cycle={cycle:.3f} (mean gap between writes of {probe})")
else:
    print(f"THROUGHPUT seconds_per_cycle=n/a (<2 writes of {probe} seen)")
limit = os.environ.get("THROUGHPUT_MAX_CYCLE_S")
if limit and (cycle is None or cycle > float(limit)):
    sys.exit(f"THROUGHPUT FAIL seconds_per_cycle={cycle} > {limit}")
if limit:
    print(f"THROUGHPUT OK seconds_per_cycle <= {limit}")
