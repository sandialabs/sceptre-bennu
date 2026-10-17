# Performance notes (800-tag case)

## Output path (field device -> provider)

Before:

- `OutputModule::scanOutputs` sent one REQ per changed tag.
- `Client::send` slept 500 ms after every send, even after the reply arrived.
- Cycle floor = 500 ms x changed tags. 800 changed tags is about 400 s per cycle. The 50-tag harness
  config is about 25 s.
- The updated-tag map was copied without its lock and erased unconditionally, so a value written by comms
  during the send could be lost.

After:

- One snapshot per cycle, one batched `WRITE=` for all of this module's changed tags, no sleep. Cost is one
  REQ/REP round trip plus provider-side handling of the batch.
- Erase is compare-and-erase and only after ACK. If the provider doesn't reply, the batch is retried next cycle. An `ERR=` reply falls back to sending each point
  on its own; rejected points are dropped.
- gobennu already batched through `zmqBatchUpdater`.

## Input path (provider -> field device)

800 tags as `name:value,` is about 18-28 KB per publish. At a 1500 byte MTU that is 13-20 UDP
datagrams. Receiving side parsing is trivial; the cost is datagram count and loss exposure (no retransmit,
see `protocol.md`). Delta publishing (send only changed tags) is the lever, and is not implemented.

Loss is observable with `BENNU_LOG_LEVEL=debug`: the publisher logs datagrams per publish, the subscriber
logs `datagrams_received` and `InputModule` logs `points_parsed/unknown/malformed` about every 10 s. See
`protocol.md` (UDP loss) for how to read them and for mitigations (fewer/shorter tags, lower publish rate).

## Logging overhead

Before, every request, ACK, per-rule logic dump and per-10-cycle data dump printed unconditionally, and
`printExternalData` held the DataStore mutex while printing. At INFO these are now skipped by one integer
compare. Use `BENNU_LOG_LEVEL=debug` only when diagnosing; it restores the old volume.

## Why the language is not the lever

Parsing or formatting 800 tags is sub-millisecond in C++, Go or Rust. The measured costs were a fixed
sleep and a request per tag, which are protocol and loop-structure problems. gobennu, which had the
batching, never had them. A rewrite would not have removed either.

Follow-ups since done: `LogicModule` prepares its tag lists once at load instead of every cycle,
`isBinary` / `isAnalog` use hash sets, and the Python `Client` has `write_points` (one batched request,
used by `siren`; per-tag methods remain).

## Measured throughput

Produced by `docker/test/run.sh throughput` (N output tags, each incremented every cycle by a logic rule
`pN = pN + 1`, written to a mock provider). Before is a worktree of the commit before the batching change.
`seconds_per_cycle` is the mean gap between writes of one tag.

800 tags (the case this work targeted), 1000 ms cycle:

| Metric | Before | After |
|---|---|---|
| tag_writes_per_sec | 2.01 | 792.6 |
| seconds_per_cycle | ~398 (800 / 2.01; no full cycle in a 90 s window) | 1.010 |

800 tags, 100 ms cycle (after only): 7,290 tag writes/s, 0.110 s per cycle, so about 10 ms of output
work per cycle for 800 tags.

50 tags, 100 ms cycle:

| Metric | Before | After |
|---|---|---|
| requests_per_sec | 2.01 | 9.68 |
| tag_writes_per_sec | 2.01 | 483.83 |
| seconds_per_cycle | 25.05 | 0.104 |

Commands:

```text
THROUGHPUT_TAGS=800 THROUGHPUT_CYCLE_MS=1000 docker/test/run.sh throughput
git worktree add <dir> <commit-before-batching>
docker/test/run.sh -s <dir> -v bennu-before build throughput
```

Defaults are 50 tags, 100 ms cycle, 60 s window, and `BENNU_LOG_LEVEL=INFO` (DEBUG logs every rule each
cycle and skews the result). Set `THROUGHPUT_MAX_CYCLE_S` to make the run fail when the mean cycle is slower;
CI uses this as a regression guard (see `testing.md`).
