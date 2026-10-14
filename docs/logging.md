# Logging

## C++ (`bennu-field-device`, libs)

`src/bennu/utility/Log.hpp` is a small header-only logger.

- Levels: `ERROR`, `WARN`, `INFO` (default), `DEBUG`. ERROR/WARN go to stderr, INFO/DEBUG to stdout.
- Set with env `BENNU_LOG_LEVEL` (`error|warn|info|debug` or `0-3`, case-insensitive), or
  `bennu-field-device --log-level <level>`, which sets the env before the config is loaded.
  Unset, empty or invalid means INFO.
- The level is read once per process. It is an env var so every shared lib sees the same value.
- The level check runs before formatting. A disabled DEBUG call costs one integer compare.
- Macros: `BENNU_LOG_ERROR/WARN/INFO/DEBUG(fmt, ...)` (printf style, at least a format string).

Moved to DEBUG (all hot-path, per request or per cycle):

- `Server` received request / sent reply, `Provider` per-request prints.
- `Client` ACK/ERR replies, retry notices and the default reply handler.
- `LogicModule` per-rule `LOGIC (...)` dumps and "Delaying tag".
- `IOModule` per-point binary/analog adds.
- `FieldDevice` `printExternalData` (every 10 cycles). It is skipped entirely unless DEBUG is on, and
  `DataStore::printData` prints its header inside the lock.

Unchanged: startup lines (`Server running on ...`, `Publisher running on ...`) print at any level, because
`test/test_ep_server.cpp` asserts the exact stdout+stderr. Errors in `Client`, `Server`, `LogicModule`,
`FieldDevice`, `Subscriber` and `InputModule` still use `printf`/`std::cout` and ignore the level.

The per-protocol event logs (DNP3, Modbus, ... via `DirectLoggable`) are separate and unchanged.

## pybennu

`pybennu/distributed/*.py` use `logging.getLogger(__name__)` (`pybennu.distributed.server`, `.provider`,
`.client`, `.subscriber`, `.publisher`). Library code never calls `basicConfig`.

- Per-request, per-ACK and per-publish messages: DEBUG. Errors: ERROR / `exception`. Client replies and
  subscriber updates: INFO.
- Entry points call `logging.basicConfig` with format
  `%(asctime)s %(levelname)s %(name)s: %(message)s`:

  | Entry point | Level |
  |---|---|
  | `pybennu-test-ep-server`, `pybennu-test-ep-server-helics`, `pybennu-power-daemon` | DEBUG with `-d/--debug`, else INFO |
  | `pybennu-test-subscriber`, `pybennu-probe`, `pybennu-groundtruth-monitor` | INFO (no flag) |
  | alicanto | configures DEBUG at import; untouched |

- An application embedding a provider must configure logging itself, or the INFO lines are not shown.
- `pybennu_test_ep_server.py` still has a few `print()` calls, and the helics test server attaches its own
  handler to the `helics` logger, so those lines appear twice.

## gobennu

`--log-verbosity` or env `GOBENNU_LOG_VERBOSITY`: `0` (default) to `2` for more detail (per-update lines
are at level 2), `-1` disables logging.
