# Provider <-> field-device protocol

Two channels, both ZeroMQ:

| Direction | Pattern | Default endpoint | Purpose |
|---|---|---|---|
| field device -> provider | REQ/REP (TCP) | `tcp://127.0.0.1:5555` | QUERY, READ, WRITE |
| provider -> field device | RADIO/DISH (UDP) | `udp://239.0.0.1:40000` | periodic tag publish |

The provider runs the REP server (`Server`/`Provider`) and the RADIO publisher. The field device runs the
REQ client (`Client`) and the DISH subscriber. RADIO/DISH needs libzmq built with drafts.

## Request/reply

Requests are `OP=payload`, sent as a string with a trailing null byte. Replies are `ACK=<data>` or
`ERR=<message>`. Lowercase ops and statuses are accepted. A message that is empty or has no `=` gets
`ERR=Message empty` / `ERR=Invalid message request`.

| Request | Payload | Reply |
|---|---|---|
| `QUERY=` | none | `ACK=tag1,tag2,...` |
| `READ=<tag>` | one tag name | `ACK=<value>` |
| `WRITE=<p>:<v>[,<p>:<v>...]` | one or more `point:value` pairs | `ACK=<msg>` |

Values are `true`/`false` for binary points and decimal for analog points.
The pybennu provider splits on the first `=` only, and a WRITE pair with no `:` returns
`ERR=Malformed WRITE point '<pair>'` without writing anything. An unknown op returns
`ERR=Unknown command type '<op>'`.

### Batched WRITE

One WRITE can carry many pairs, and the provider applies them as one call to `write(tags)`.

- gobennu already did this (`zmqBatchUpdater.Commit` joins all queued updates with `,`).
- The C++ field device now does too. `OutputModule::scanOutputs` snapshots the changed tags once per cycle
  and sends one `WRITE=` (`Client::buildWritePayload` / `Client::writePoints`). Before, it sent one request
  per tag and `Client::send` slept 500 ms after each.
- `Client::send` returns true only when an ACK arrives. On ACK the sent tags are removed from the updated
  maps, but only if the stored value is unchanged (compare-and-erase), so a newer value written meanwhile
  survives to the next cycle. If the provider never replies (outage), nothing is removed and the batch is
  retried next cycle.
  One ERROR line is logged per outage.
- Retry: 5 s poll timeout, 3 tries, then the socket is recreated (Lazy Pirate).
- If the provider answers `ERR=` for the batch, the field device re-sends each point on its own. Points it
  accepts are applied. Points it rejects are logged and dropped, as before batching. If it stops replying
  partway through, that counts as an outage.

### Behavior change: per-OutputModule scoping

Each `OutputModule` now writes only tags whose point belongs to that module (points recorded in
`IOModule::handleTreeData`). Previously the first output module sent every module's tags to its own
provider, including points of read-only input modules. No config in `data/configs` has more than one
`<output>`, so shipped configs are unaffected. Tags with no point (internal tags) are never sent and stay
in the updated map.

## Publish (RADIO/DISH)

The provider publishes `tag:value,tag:value,...` (trailing comma). Each datagram is a ZMQ message with a
null-terminated body, sent to a RADIO group.

- Group: `Endpoint::hash()`, which is `std::hash<std::string>` of the endpoint string in hex with the last
  character dropped (groups are limited to 15 chars). If the URL has an interface specifier
  (`udp://iface;239.0.0.1:40000`), the interface part is removed before hashing. gobennu gets the same value
  through the SWIG `Endpoint` wrapper. The hash is implementation-defined, so the provider and subscriber
  should come from the same libstdc++.
- Chunking: a message longer than the MTU is split on commas into chunks, each made of whole `tag:value,`
  parts. A part is never cut. Empty parts are skipped. A single part larger than the MTU goes out alone.
  Chunk size is held strictly under the limit (`chunk + next part >= limit` starts a new chunk).
- Datagram size: libzmq sends a RADIO message over UDP as `[group length (1 B)][group][body]`, inside 28 B
  of IPv4 and UDP headers. Both publishers size the body so the whole packet fits the link MTU, because a
  larger datagram is IP-fragmented and losing either fragment loses it.
  - Link MTU: env `BENNU_PUBLISH_MTU`, default `1500`. Set it lower when the path is smaller, for example
    `1450` across a VXLAN overlay. It must be an integer from 576 to 8220 (libzmq caps a UDP message at
    8192 B), or the publisher fails to start.
  - Body limit: `MTU - 28 - 1 - len(group)`, 1456 B for the usual 15-character group. The C++ body includes
    its null byte; the Python body has none.
  - C++ `Publisher` (`distributed::publishBodyLimit`, `publishLinkMtu`) and Python `Publisher`
    (`pybennu.distributed.utils.publish_body_limit`, `publish_link_mtu`) share the rule.
  - Before this, C++ split at 1500 (up to 1516 B datagrams) and Python at 1465 (up to 1480 B), so every full
    chunk fragmented on a 1500 B link.
- Fixed bug: the old splitter dropped the final tag when the message had no trailing comma and exceeded
  the MTU (799 of 800 tags delivered in simulation). It affected the helics, rtds, opal_rt and siren
  providers. Both implementations now deliver every tag.
- UDP loss: RADIO/DISH over UDP has no acknowledgement and no retransmit. A lost datagram loses the tags in
  it until the next periodic publish (the next publish is a full snapshot, so loss self-heals at the publish
  rate). Datagrams per publish is about message bytes / MTU, so 800 tags (18-28 KB) is 13-20 datagrams and
  loss odds scale with tag count. A field device sees the loss as stale values, not as an error.
  - Observe (set `BENNU_LOG_LEVEL=debug`, see `logging.md`):
    - Publisher: `D: Publisher: <bytes> bytes -> <n> datagrams` on every publish.
    - Subscriber: `D: Subscriber: datagrams_received=<total>` about every 10 s. Compare the growth over a
      window with (datagrams per publish) x (publishes in the window); a shortfall is loss.
    - InputModule: `D: InputModule: points_parsed=... points_unknown=... points_malformed=...` about every
      10 s. `points_malformed` growing means cut or garbled parts; parsed growing slower than expected
      means whole datagrams are missing.
    - The counters are logged on receipt, so no log line at all means nothing is arriving.
  - Mitigate: fewer or shorter tag names (fewer bytes, fewer datagrams), a lower publish rate (less
    traffic, longer recovery), a wired, unsaturated network or a dedicated interface for the group. Delta
    publishing (only changed tags) would cut datagrams most but is future work and would change publisher
    semantics; the protocol is unchanged.
- Subscriber payload format: plain `tag:value,tag:value,` only. The old rtds, opal_rt and siren providers
  published `Write={tag:value,...}`; `InputModule` does not strip the wrapper, so the first tag name became
  `Write={tag` (unknown, dropped) and the last value `true}` (a bool parsed as non-bool and rejected).
  They now publish the plain form. The `WRITE=` prefix belongs only to client requests to a provider.
- Python `Client.write_points({tag: value})` sends one `WRITE=a:1,b:true` request for many tags (bools as
  `true`/`false`, same as C++ `Client::buildWritePayload`); `write_analog_point`/`write_digital_point` still
  send one request each.
