**English** · [Русский](ru/PROTOCOL.md)

# OLC network protocol

> Protocol version 7 as it is made in the code: constants — `core/protocol.h`, field types —
> `core/serialize.h`, headers — `core/header.h`, all messages of sections 5 and 6 — `core/messages.h`.
> If the document and the code differ, the code is right and the document must be fixed.

How the plugin is built as a whole — [ARCHITECTURE.md](ARCHITECTURE.md). Which SDK functions stand behind
the fields — [SDK-CHEATSHEET.md](SDK-CHEATSHEET.md).

## 1. Principles

1. **Two channels.**
   - **TCP** — for everything that must not be lost: connecting, vessel and body lists,
     ownership, single key presses, dockings.
   - **UDP** — for what is sent in full every time: the world state and input.
     A lost packet does not matter, the next one replaces it.
2. **Every UDP packet is self-contained.** It carries the full current value, not a change.
   So UDP needs no acknowledgements or retransmissions.
3. **Names over TCP, numbers over UDP.** The host gives every vessel and body a short number
   (`net_id`) and sends the "number → name" mapping over TCP. The number of a removed vessel is
   not reused during the session: a late packet will not land on another vessel. UDP carries only numbers.
   `OBJHANDLE` is never sent over the network.
4. **Explicit serialization.** Every field is written and read by a separate function with a fixed
   size and little-endian byte order. C++ structs are never copied to the network as a whole:
   that way the format does not depend on the compiler or alignment.
5. **Check before parsing.** A packet with a foreign signature, another version, a wrong length or
   an unknown vessel number is dropped and counted in the statistics. A broken packet must not
   crash Orbiter.

## 2. Field types

| Type | Size | What it is |
|---|---|---|
| `u8`, `u16`, `u32` | 1, 2, 4 bytes | Unsigned integers, little-endian |
| `f32`, `f64` | 4, 8 bytes | IEEE 754 numbers, little-endian |
| `str` | 1 + N bytes | Length (`u8`, up to 255) and a UTF-8 string without a terminating zero |

Coordinates and velocities are `f64` only: the orbit radius is about 7·10⁶ m, and millimetre
precision is needed. This does not load the traffic: even 60 full packets a second of 1200 bytes is about 70 KB/s.

## 3. Ports and connecting

- The host listens for **TCP and UDP on one port number**, `47800` by default (it can be changed in
  the plugin settings).
- The client connects over TCP and reports its UDP port after the handshake.

Sequence:

```
Client                                   Host
  │── TCP connect ─────────────────────────►│
  │── HELLO ───────────────────────────────►│  protocol version, Orbiter version, scenario number, player name
  │◄─────────────────────────── WELCOME ────│  client number, session number, state rate
  │                          or REJECT ─────│  reason: version, slots, scenario
  │◄──────────────────────────── BODIES ────│  number → name for planets and moons
  │◄──────────────────────────── VESSELS ───│  number → name, class for all vessels
  │◄────────────────────────── OWNERSHIP ───│  who controls which vessel (right after VESSELS)
  │◄──────────────── DOCK, ANIM_STATE, NAV_STATE │  links, animations and frequencies now (only if there are any)
  │── CLASSES_MISSING ─────────────────────►│  which classes the client lacks (may be empty)
  │── UDP_READY (over TCP) ────────────────►│  the client's UDP port (both right on VESSELS)
  │◄═══════════════ WORLD_STATE (UDP) ══════│  up to 60 Hz, 60 by default
  │═══ INPUT (UDP) ════════════════════════►│  every frame, at most 60 Hz
```

Scenario request (stage 2): a client that has no session yet sends `HELLO` with `scenario_id` = 0.
The host checks the versions and that a place is free, saves the current world and answers `SCENARIO`
(or `REJECT`). Once the whole file has gone out, the host closes the connection; such a client is not
counted as a player. The client starts the scenario and connects again with the number from `SCENARIO` —
then the usual sequence above follows.

```
Client                                   Host
  │── TCP connect ─────────────────────────►│
  │── HELLO (scenario_id = 0) ─────────────►│
  │◄────────────────────────── SCENARIO ────│  save number, name, file
  │◄──────────────────────── TCP close ─────│
```

**Silence.** After the handshake the sides hear each other at least once a second: the client
sends `PING`, the host sends `WORLD_STATE` and `PONG`, also while paused. A side that has heard
nothing for 15 s (`kSilenceTimeout`, `core/protocol.h`) treats the other one as gone: the host
disconnects the client (its vessel stays with it for 60 s, as after a broken connection), the client
closes the connection. This way a frozen computer or a pulled cable, which leave the TCP connection
open, do not hold a place and a vessel forever.

## 4. Headers

**TCP** is a stream, so every message starts with a length:

| Field | Type | Meaning |
|---|---|---|
| `length` | `u32` | Message length after this field, at most 1 MB |
| `type` | `u8` | Message type (section 5) |

**UDP**:

| Field | Type | Meaning |
|---|---|---|
| `magic` | 4 bytes | `O`, `L`, `C`, `0` |
| `version` | `u16` | Protocol version, currently `7` |
| `type` | `u8` | Message type (section 6) |
| `session` | `u32` | Session number from `WELCOME`; foreign packets are dropped |
| `seq` | `u32` | The sender's sequence number; old and repeated packets are dropped |

A UDP packet is at most 1200 bytes: a packet this size is not fragmented on any ordinary network.
If the vessels do not fit into one `WORLD_STATE`, the state is split into several packets
with the same `sim_time`.

**Type numbers** (the `type` field, `core/messages.h`) — in the order of the tables of sections 5 and 6:

| Channel | Numbers |
|---|---|
| TCP | 1 `HELLO`, 2 `WELCOME`, 3 `REJECT`, 4 `SCENARIO`, 5 `BODIES`, 6 `VESSELS`, 7 `VESSEL_ADDED`, 8 `VESSEL_REMOVED`, 9 `CLASSES_MISSING`, 10 `OWNERSHIP`, 11 `UDP_READY`, 12 `KEY`, 13 `DOCK`, 14 `LANDED`, 15 `BYE`, 16 `OWNER_REQUEST`, 17 `OWNER_DENIED`, 18 `TIME_REQUEST`, 19 `PLAYERS`, 20 `SAVE_REQUEST`, 21 `ANIM_STATE`, 22 `NAV_STATE` |
| UDP | 1 `WORLD_STATE`, 2 `INPUT`, 3 `PING`, 4 `PONG`, 5 `FUEL` |

A message with extra bytes after the last field is treated as broken, the same as a truncated one.

## 5. TCP messages

| Type | Direction | Fields |
|---|---|---|
| `HELLO` | client → host | `protocol: u16`, `orbiter_version: u32` (`oapiGetOrbiterVersion`), `scenario_id: u32`, `plugin_version: str`, `player: str`, `flags: u8` (bit 0 — observer: the client never takes a vessel). `scenario_id`: 0 — there is no session yet, the client asks the host for a scenario (stage 2); 4294967295 (`0xFFFFFFFF`) — the player started a scenario by hand, as at stage 1; otherwise — the number from `SCENARIO` that the client loaded |
| `WELCOME` | host → client | `client_id: u8`, `session: u32`, `state_rate_hz: u8`, `system: str` (planetary system name), `flags: u8` (bit 0 — the client is the flight director, bit 1 — an observer; the director is always an observer). The host knows the director by the setting `Director = <player name>`: the client with that name in `HELLO`; empty — there is no director |
| `REJECT` | host → client | `reason: u8` (1 — protocol version, 2 — Orbiter version, 3 — reserved, the host does not send it: a different planetary system is noticed by the client after `WELCOME`, and it leaves with `BYE`; 4 — no slots: the player limit is reached, the host is a player too, 4 by default, at most 8; 5 — the host cannot give a scenario: it was not saved or the file is over the limit), `text: str` |
| `SCENARIO` | host → client | `scenario_id: u32` (save number, the client returns it in `HELLO`), `name: str`, `size: u32`, then `size` bytes of the scenario file (stage 2). The file is at most 1 MB − 265 bytes: that way the whole message with the longest name fits into a TCP frame. A `size` not equal to the number of remaining bytes means a broken message |
| `BODIES` | host → client | `count: u16`, then `net_id: u16`, `name: str` |
| `VESSELS` | host → client | `count: u16`, then `net_id: u16`, `name: str`, `class: str` |
| `VESSEL_ADDED` | host → client | `name: str`, `class: str`, then the vessel state as in `WORLD_STATE` (it starts with `net_id`) |
| `VESSEL_REMOVED` | host → client | `net_id: u16` |
| `CLASSES_MISSING` | client → host | `count: u16`, then `class: str` |
| `OWNERSHIP` | host → client | `count: u16`, then `net_id: u16`, `owner: u8` (0 — host, 1…254 — client, 255 — nobody) |
| `UDP_READY` | client → host | `udp_port: u16` — the port on the client machine; if the client is behind NAT, the host takes the real address from `PING` (section 6) |
| `KEY` | client → host | `net_id: u16`, `key: u8` (an `OAPI_KEY_*` code), `mods: u8` (bits: 0 — Shift, 1 — Ctrl, 2 — Alt) |
| `DOCK` | host → client | `a: u16`, `port_a: u8`, `b: u16`, `port_b: u8`, `docked: u8` (1 — docked, 0 — undocked). The link is named from the vessel with the smaller `net_id`. The host remembers the current links and sends them to a client that joins later, right after `OWNERSHIP` |
| `LANDED` | host → client | `net_id: u16`, `lng: f64`, `lat: f64`, `hdg: f64` (rad) — a vessel came to rest on a surface |
| `OWNER_REQUEST` | client → host | `net_id: u16`, `action: u8` (1 — take a free vessel, 0 — release one's own). The host checks the rules and on success sends the whole `OWNERSHIP` to everyone |
| `OWNER_DENIED` | host → client | `net_id: u16`, `reason: u8` (1 — no such vessel, 2 — the vessel is busy: it belongs to the host, to another client or is kept for a pilot who dropped out, 3 — the client already has another vessel, 4 — not the client's vessel, cannot release, 5 — an observer does not take a vessel). Only to the one who asked |
| `TIME_REQUEST` | client → host | `paused: u8` (0 — running, 1 — paused), `warp: f64` (time acceleration, finite and above 0). The host applies the request only from the flight director; from anyone else it writes to the log and skips it. Everyone sees the result in `WORLD_STATE`. Another value of `paused` or `warp` makes the message broken |
| `PLAYERS` | host → client | `count: u8`, then `client_id: u8` (0 — the host; a host without graphics does not play and is not listed), `player: str`, `flags: u8` (bit 0 — director, bit 1 — observer), `net_id: u16` (the player's vessel, 65535 — none), `rtt_ms: u16` (the player's round trip time to the host, ms; 65535 — unknown, 0 for the host). The host sends it on every join, leave and ownership change and every 2 s |
| `SAVE_REQUEST` | client → host | Empty body. A request to save the world now; the host saves it to `Scenarios\OLC\Saves\` only for the flight director's request, and logs and skips anyone else's. Any body makes the message broken |
| `ANIM_STATE` | host → client | `net_id: u16`, `count: u16`, then `count` pairs `index: u16`, `value: f32` (0…1) — the vessel's animations that changed by more than 0.01 (the plugin reads them with `GetAnimPtr` at most 5 times a second per vessel). A value outside 0…1 or NaN makes the message broken. The host remembers the latest value of every animation and sends them in one message to a client that joins later, after `DOCK`; when a vessel is removed they are forgotten. The numbers match only for the same vessel class from one package (decision 58) |
| `NAV_STATE` | both ways | `net_id: u16`, `transponder: u16`, `count: u8` (at most 16), `channels: u16[count]` — steps of the transponder and of the NAV receivers (`SetTransponderChannel`, `SetNavChannel`). The owner is the source: a client sends it only for its own vessel, the host ignores it for any other (counted in the stats), applies it and sends it on to the other clients; for the host's own vessels the host sends it itself. The latest `NAV_STATE` of each vessel goes to a client that joins later, after `ANIM_STATE` |
| `BYE` | any | `reason: str` |

Ownership (decision 35): one client — one vessel, a vessel without an owner is free. If a client
left or lost the connection, its vessel stays without an owner but for 60 s (`kOwnerHoldSeconds`)
is closed to the others and goes back to the pilot who connects under the same name (`player` from
`HELLO`). The host sends `OWNERSHIP` on every change: take, release, a client leaving, a pilot
coming back, a new vessel.

Single presses (`KEY`) go over TCP because "engage autopilot" must not be lost. The host accepts `KEY` only for the sender's own vessel and passes the key code to the vessel as is: the host's `keymap.cfg` decides which logical key ("Prograde") it is.

## 6. UDP messages

### `WORLD_STATE` (host → client)

Common part:

| Field | Type | Meaning |
|---|---|---|
| `sim_time` | `f64` | The host's `oapiGetSimTime` — all states refer to this moment |
| `mjd` | `f64` | `oapiGetSimMJD` |
| `warp` | `f64` | Time acceleration |
| `paused` | `u8` | 1 — paused |
| `count` | `u8` | How many vessels are in the packet |

For each vessel:

| Field | Type | Meaning |
|---|---|---|
| `net_id` | `u16` | Vessel number |
| `status` | `u8` | 0 — flying, 1 — landed on a surface |
| `ref_body` | `u16` | Reference body number |
| `rpos` | 3 × `f64` | Position relative to the reference body, m (only with `status = 0`) |
| `rvel` | 3 × `f64` | Velocity relative to the reference body, m/s (only with `status = 0`) |
| `arot` | 3 × `f64` | Attitude, Euler angles as in `VESSELSTATUS2` (only with `status = 0`) |
| `vrot` | 3 × `f64` | Angular velocity, rad/s (only with `status = 0`) |
| `nthr` | `u8` | Number of thrusters |
| `thr` | `nthr` × `u8` | Thruster levels, 0…255 → 0…1: the client needs them to draw the exhaust |

For `status = 1` the position is not sent: the client got the landing place in `LANDED`.
Any other `status` value means a broken packet: it is unknown which fields follow.
Fuel is not needed in every packet — it comes once per second in a separate `FUEL` message.

### `INPUT` (client → host)

The full current state of the controls. The client sends it every frame while it
controls a vessel; the host applies the latest one received.

| Field | Type | Meaning |
|---|---|---|
| `net_id` | `u16` | The vessel the client controls |
| `keys` | `u32` | Keys held right now, bits — section 7 |
| `rot` | 3 × `f32` | Joystick: pitch, yaw, bank, −1…1 |
| `lin` | 3 × `f32` | Joystick: translation up-down, left-right, forward-back, −1…1 |
| `throttle` | `f32` | Throttle lever 0…1; −1 — no lever or it has not moved. The version 0.1 client does not read the lever and always sends −1 |

The client sends at most 60 packets per second. The host accepts `INPUT` only from the UDP
address of the client that owns the vessel (`net_id`); input for someone else's vessel is dropped.
Only a packet with a newer `seq` than the last accepted one counts (the first is always
accepted), and the latest one wins. If no `INPUT` arrives for 0.5 s, the host releases the
vessel's controls: with a lost connection no key stays stuck. A packet with `rot` or `lin` outside
−1…1, `throttle` other than −1 and outside 0…1, or with NaN or infinity is broken and is dropped.

Joystick signs: in `rot` positive means pitch up, yaw left, bank left; in `lin` positive means up,
left, forward.

In Orbiter, RCS rotation and translation share the same keys. The client does not know the RCS mode of
the vessel on the host, so for such a key it sends both bits — rotation and translation. The host uses
the bits of its vessel's RCS mode.

### `PING` (client → host) and `PONG` (host → client)

`PING`: `t: f64` — the sender's system time (`oapiGetSysTime`), `client_id: u8` — the client
number from `WELCOME`, `rtt_ms: u16` — the latest round trip time the client measured (65535 — not measured yet; the host uses it for `PLAYERS`). The host answers with `PONG` with the same body, from which the client gets
the round trip time. The client sends `PING` once per second.

From `PING` the host learns the client's real UDP address: behind NAT (for example, a shared virtual
machine network) the port the packets come from may differ from the port in `UDP_READY`, and
`WORLD_STATE` sent to the port from `UDP_READY` would not arrive. The address is accepted only if the IP matches
the IP of the TCP connection of the client with this number. The host uses the same address for
`INPUT`.

### `FUEL` (host → client)

Once per second (decision 24). Without fuel the exhaust disappears on the client: the core zeroes the thrust
of a thruster with an empty tank ([ORBITER-INTERNALS.md](ORBITER-INTERNALS.md), section 17).
The client applies fuel before the thruster levels from `WORLD_STATE`. Vessels that do not
fit into one packet go into several `FUEL` packets, as in `WORLD_STATE`.

| Field | Type | Meaning |
|---|---|---|
| `count` | `u8` | How many vessels are in the packet |

For each vessel:

| Field | Type | Meaning |
|---|---|---|
| `net_id` | `u16` | Vessel number |
| `empty_mass` | `f64` | Empty mass, kg (`GetEmptyMass`): add-ons change it themselves |
| `ntank` | `u8` | Number of tanks |
| `level` | `ntank` × `f32` | Tank fill in `GetPropellantHandleByIndex` order, 0…1 |

## 7. Held key bits (`INPUT.keys`)

| Bit | Action | Bit | Action |
|---|---|---|---|
| 0–5 | RCS rotation: pitch +/−, yaw +/−, bank +/− | 16 | Increase throttle smoothly |
| 6–11 | RCS translation: up/down, left/right, forward/back | 17 | Decrease throttle smoothly |
| 12 | Low RCS power (Ctrl) | 18, 19 | Hover up / down |
| 13 | Full main thrust | 20, 21 | Trim up / down |
| 14 | Full retro thrust | 22, 23 | Left / right wheel brake |
| 15 | Kill main and retro thrust | 24–31 | Reserved |

The client sends **actions**, not key codes. So the key layout on the client may
differ from the host.

## 8. Time on the client

- Pause and warp come from every `WORLD_STATE`. If the player on the client pauses
  or changes warp, the plugin restores the host's value. The exception is the flight director: their
  pause and warp go to the host in `TIME_REQUEST`, and the plugin leaves them alone until the host answers.
- While paused the host keeps sending `WORLD_STATE` (less often, on the hidden window's timer): otherwise
  the client would not learn that the pause was released. The paused client also receives packets on a timer —
  `clbkPreStep` is not called while paused ([ORBITER-INTERNALS.md](ORBITER-INTERNALS.md), section 14).
- The client shows the world with a small delay, interpolating between the received states
  (cubic in position and velocity). The whole client world lags, not only other players' vessels
  ([SMOOTHING.md](SMOOTHING.md)). The default for a wired LAN is `WORLD_STATE` at 60 Hz and a delay of
  30 ms (decision 36, measured on the rigs); over Wi-Fi 30 Hz and 100 ms are safer.
- If the client sim time drifts from the host by more than the `TimeSyncThreshold` threshold (0.1 s
  by default, × warp when accelerated), there is a jump with `oapiSetSimMJD`, at most once in 2 s; a
  smaller drift is only counted in the log.

## 9. Versions and compatibility

- `version` in every UDP packet and `protocol` in `HELLO`. Different versions do not work together:
  the host answers `REJECT` with reason 1.
- Version 7 (2026-11-19): 4A — `ANIM_STATE` and `NAV_STATE` (decision 58, task 4A.1).
- Version 6 (2026-11-19): `SAVE_REQUEST` (decision 55, task 5.4).
- Version 5 (2026-11-19): stage 5 — `flags` in `HELLO` and `WELCOME`, `TIME_REQUEST`, `PLAYERS`, `rtt_ms` in `PING`, reason 5 in `OWNER_DENIED` (decision 54, task 5.1).
- Version 4 (2026-11-18): vessel ownership — `OWNERSHIP`, `OWNER_REQUEST`, `OWNER_DENIED` (task 3.1).
- Version 3 (2026-11-17): `scenario_id` added to `HELLO` and `SCENARIO` (decision 31).
- Version 2 (2026-11-17): `client_id` added to `PING`. Version 1 is without it.
- The Orbiter version must be the same on the host and the client (`REJECT`, reason 2): different builds
  may compute physics differently and store state differently.
