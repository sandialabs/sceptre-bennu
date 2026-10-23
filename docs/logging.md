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

Every module logs with [loguru](https://github.com/Delgan/loguru): `from loguru import logger`.
`pybennu/log.py` holds the setup. Library code never configures logging.

- Entry points call `setup_logging(prog, debug, file_by_default)` before any daemonizing fork. It adds a
  stderr sink and an optional file sink, and routes stdlib `logging` (pymodbus, elasticsearch, the
  vendored pypmu) into loguru.
- `BENNU_LOG_LEVEL`: the same variable as the C++ side, `error|warn|info|debug` or `0-3`. Unset or empty
  means INFO. An invalid value is an error here; the C++ side falls back to INFO.
- `BENNU_LOG_FILE`: the log file path. Set it empty to disable the file. Unset, the services write
  `/var/log/bennu/<program>.log` and the CLI and test tools write no file:

  | Entry point | File by default |
  |---|---|
  | `pybennu-power-solver`, `pybennu-groundtruth-monitor` | on `start`/`restart` |
  | `pybennu-siren`, `pybennu-alicanto` | always |
  | `pybennu-probe`, `pybennu-test-ep-server`, `pybennu-test-ep-server-helics`, `pybennu-test-subscriber` | no |

- Debug switches (`-d`/`-v`, the power-solver config `debug` option, the rtds, opal_rt and helics debug
  settings) call `enable_debug()`, which re-applies the setup at DEBUG. The level is global, except Siren: it keeps its per-device
  `level_name` (and the provider's `debug: true`) on stdlib loggers, and a DEBUG one lowers the sinks to DEBUG.
- Per-request, per-ACK and per-publish messages: DEBUG. Errors: ERROR or `exception`. Client replies and
  subscriber updates: INFO.
- Per-instance loggers bind a context: `logger.bind(ctx="Modbus 10.0.0.1:502")` prints `[Modbus ...]`.
- Messages use `{}` placeholders. A `%s` in a loguru call prints literally; `tests/test_log.py` fails on
  one, and on any stdlib module logger or `basicConfig` outside `pybennu/log.py`.
- Sinks use `diagnose=False`, so tracebacks never print local variables such as credentials.
- An application that imports pybennu gets no pybennu output until it calls `setup_logging()` or
  `logger.enable("pybennu")`.
- CLI output stays on stdout: probe replies, daemon `status`, test-subscriber updates.

## gobennu

`--log-verbosity` or env `GOBENNU_LOG_VERBOSITY`: `0` (default) to `2` for more detail (per-update lines
are at level 2), `-1` disables logging.
