**English** · [Русский](ru/ARCHITECTURE.md)

# How orbiter-lan-coop is built

What is where and which conditions must not be broken — see [README.md](README.md) in this folder.

> Below is the architecture being checked by the prototype (stages 0–1 in [PLAN.md](PLAN.md)).
> Stage 1 code exists (section 6): the host sends the world state and the client shows it;
> client input is stage 3. After the prototype the document is rewritten to match the real code.
> References to the Orbiter core are in [ORBITER-INTERNALS.md](ORBITER-INTERNALS.md).

## 1. The big picture

One and the same plugin (a DLL in `Modules/Plugin`) works in one of two roles.

```
   Host                                              Client
  ┌──────────────────────────────────┐              ┌──────────────────────────────────┐
  │ Orbiter + OLC (host role)        │   vessel     │ Orbiter + OLC (client role)      │
  │                                  ├─────────────►│                                  │
  │ computes physics of ALL vessels  │   states,    │ imposes the host's states on     │
  │ controls time and pause          │   time       │ vessels, smooths between packets │
  │ applies client input to          │              │ intercepts the player's input    │
  │ "their" vessels                  │◄─────────────┤ and sends it to the host         │
  └──────────────────────────────────┘    input     └──────────────────────────────────┘
```

- **Host** — an ordinary Orbiter in which the instructor also flies. Its simulation is the "truth".
- **Client** — an ordinary Orbiter with the same scenario. Its own vessel physics is
  replaced by the states it receives, and the player's input goes to the host.
- **Vessel ownership.** Every vessel has a "controller": the host, a specific client or
  nobody. A client's input is applied only to the vessel it controls. The "crew" mode
  (two people in one vessel) is two participants with input rights for one vessel.

## 2. What happens at run time

### Connecting

1. The host starts a scenario and turns on the host role.
2. The client gets **the same world**. The SDK has no function to load a scenario at run time,
   so:
   - at stage 1 both start the same scenario by hand;
   - from stage 2 the student presses "Connect" in Launchpad, the host sends its current
     scenario, the client plugin selects it in Launchpad itself and starts it in the same process
     ([ORBITER-INTERNALS.md](ORBITER-INTERNALS.md), section 18; the fallback is restarting
     Orbiter with the `-s` switch) and connects by itself (more — [PLAN.md](PLAN.md), stage 2).
   Add-ons are not sent over the network: only a check and a list of what is missing.
3. The client checks the set of add-ons and receives the "vessel → controller" mapping.
   Vessels are matched by name: a handle in the core is a pointer inside the process and
   cannot be sent over the network. The host makes sure names are unique.

### Every frame on the host

1. `clbkPreStep`: apply the latest received client input to their vessels.
2. There, capture the vessel states (`GetStatusEx`), time, warp and pause and send them
   to clients (at 20–30 Hz, not every frame). A state refers to the moment
   `oapiGetSimTime`.
3. Orbiter computes the physics.

### Every frame on the client

1. Intercept the player's input (`clbkProcessKeyboardImmediate`, `clbkProcessKeyboardBuffered`,
   joystick via `GetManualControlLevel`) and send it to the host.
2. `clbkPreStep`: impose the host's latest states on the vessels (`DefSetStateEx`), and smooth the
   motion between packets. Exactly in `clbkPreStep`: in `clbkPostStep` the core loses the
   imposed attitude (see [ORBITER-INTERNALS.md](ORBITER-INTERNALS.md), section 3).
3. A vessel that is landed on the host is switched to the "landed" status once, not
   every frame.
4. Time: pause and warp come from the host (`oapiSetPause`, `oapiSetTimeAcceleration`).
   A small sim time lag is pulled in smoothly, a large one with a jump (`oapiSetSimMJD`).

### Pause and an inactive window

While paused, Orbiter does not call `clbkPreStep`, and only the active window receives keyboard
callbacks. So the plugin has its own hidden Windows window with a timer: its messages always
arrive. On the timer, while paused, the plugin receives packets and, when the host releases the pause,
calls `oapiSetPause(false)`. The pause flag is in every `WORLD_STATE` packet.
There is no separate network thread (decision 23): the sockets are non-blocking, receiving and sending happen
in Orbiter's main thread — in `clbkPreStep`, and while paused on the hidden window's timer.
Details — [ORBITER-INTERNALS.md](ORBITER-INTERNALS.md), section 14.

### Interface

- **Launchpad**, the "Extra" tab, an "OLC" item: a Win32 dialog with the role (host or client),
  the host address, the port and the player name. Settings are saved to `Config/` before the
  session starts (`clbkWriteConfig`). From stage 2 the "Connect" button is there too.
- **In the simulation** — an "OLC" ImGui window: connection state, players and their vessels,
  network statistics (delay, loss). It opens from the Ctrl+F4 list and from a menu button.
- **Notifications** (`oapiAddNotification`) — players connecting and disconnecting.
- Without a graphics client (a host in console mode) there are no windows — only the log.

Details and catches — [ORBITER-INTERNALS.md](ORBITER-INTERNALS.md), section 16.

### How input reaches a vessel on the host

The detailed key table is in [SDK-CHEATSHEET.md](SDK-CHEATSHEET.md), section 3.

| Kind of input | How it is applied on the host |
|---|---|
| Single presses (autopilot modes, RCS, undocking, add-on keys) | `VESSEL::SendBufferedKey` — works on any vessel, including the add-on's own keys |
| Held RCS, full throttle and brake keys | `IncThrusterGroupLevel_SingleStep` every frame while the key is held — the same as the core does for the focus vessel |
| Smooth throttle, hover, trim, wheel brakes | `Set/IncThrusterGroupLevel`, `SetControlSurfaceLevel`, `SetWheelbrakeLevel` with the same logic as in the core |
| Joystick | The client sends axis deflections; the host turns them into RCS group and main engine levels |
| Add-ons' own held keys | Not supported: the SDK cannot deliver them to a vessel that is not in focus |
| Clicks in the cockpit and on panels | Later, the "Crew" stage |

### Communication channels

| Channel | Protocol | What it carries | Rate |
|---|---|---|---|
| Session | TCP | connection, scenario, add-on check, vessel names and classes, ownership | on events |
| World state | UDP | positions, velocities, attitudes, rotation, sim time, warp, pause | 20–30 Hz, less often at high warp |
| Input | UDP | control positions, key presses | every client frame |

## 3. Why it is done this way

| Decision | Rejected options and why |
|---|---|
| **Authoritative host** (all physics on one computer) | Everyone computes their own vessel (like OMP) — worlds drift apart, player docking and time acceleration break. Deterministic lockstep is impossible: Orbiter's simulation step comes from the real clock. |
| **A network, not two seats at one PC** | The core knows one focus, one camera, one window (`g_focusobj` — about 190 uses). Two sets of keyboard and mouse would mean rewriting the core. |
| **The host is a computer with a player**, no separate server | For 2–4 people on a LAN a separate server is extra complexity. A host without graphics is possible later: Orbiter can run without a graphics client (the "Orbiter Server Console"). |
| **A plugin, not a fork** | A fork has to be maintained after the main project. The SDK is enough for the prototype; missing hooks go as PRs to the main project. |
| **The base is `main` at a fixed commit**, the target is the nearest official release | The 2024 release is almost two years older; it has no vessel collisions, no ImGui dialogs, no `oapiGetKeyState`. |
| **LAN only** | The internet needs NAT handling, security and latency compensation — not needed for a classroom. |

## 4. What is needed to run

- Orbiter built from `orbitersim/orbiter` at the base commit (see [README.md](README.md),
  condition 3), the same on all computers, with the same add-ons.
- Windows 10/11 x64. Linux — via Wine/Proton, as a client.
- Vessel collisions are off on the client (`VesselCollision = FALSE` in `Orbiter.cfg`):
  otherwise the local collision calculation argues with the host's states. Checked at stage 1.

## 5. Main risks

| Risk | What is known | How we check |
|---|---|---|
| `DefSetStateEx` 20–30 times per second | Inside: orbit re-initialization, gravity recalculation, iteration over bodies and bases, a check of neighbouring vessels' hulls. No accumulating side effects were found in the code | Stage 1: smoothness and load |
| A vessel on the surface | The "landed" status is set once; the core may "land" a motionless vessel by itself after 5 s | Stage 1: a vessel on the runway does not jump or fall through |
| Smoothing between packets | The core already has a similar mechanism — flight recording playback, but it is not available from the SDK. Smoothing model: our own buffer with Hermite interpolation copes ([SMOOTHING.md](SMOOTHING.md)) | Stage 1; if needed, propose a hook in the core |
| Auto-docking on the client | The client's core may dock vessels that end up close by itself; the SDK has no switch for it | Stage 3 |
| Internal state of add-on vessels | Hidden inside the add-on DLL; it cannot be passed to a new vessel except through a scenario | Stage 4; standard vessels only at first |
| A vessel of an unknown class | `oapiCreateVesselEx` with a missing class terminates Orbiter | The client checks the class file before creating |

## 6. Code

The code has two parts:

| Part | Where | What is in it |
|---|---|---|
| Network core `olc-core` | `core/` | Protocol, serialization, interpolation buffer, vessel table, sockets. It does not depend on Orbiter or its SDK, so it builds and is tested on any system, including Mac |
| Plugin `OrbiterLanCoop` | `plugin/` | A thin layer between the Orbiter SDK and `olc-core`: captures and imposes states. It builds only on Windows, against the SDK of a built Orbiter (`QUICKSTART.md`, section 5) |

This way most of the logic is checked by tests without running Orbiter, and the plugin keeps
only what cannot be checked without the simulator.

The tests are in `tests/`, one executable per part of the core, and run through
CTest. There are no third-party test libraries: checks are the `CHECK` macro from `tests/check.h`.

```bash
cmake -S . -B orbiter-lan-coop.out
cmake --build orbiter-lan-coop.out
ctest --test-dir orbiter-lan-coop.out
```

Currently in `olc-core`:

| File | What is in it |
|---|---|
| `core/messages.h` | Message type numbers and packing and parsing of the bodies of all TCP and UDP messages (`PROTOCOL.md`, sections 5 and 6); their headers are in `header.h`. Parsing rejects a truncated body and extra bytes at the end |
| `core/protocol.h` | Protocol constants: version, port, UDP signature, size limits; comparison of UDP packet sequence numbers taking counter wrap into account |
| `core/header.h` | The UDP header (writing and checking: size, signature, version) and TCP frames: `makeTcpFrame` builds a message with its length, `TcpFrameReader` takes whole messages from the stream and stops parsing on an invalid length |
| `core/interpolation.h` | `InterpolationBuffer` — the interpolation buffer of one vessel on the client (`SMOOTHING.md`): accepts states from `WORLD_STATE` by host sim time, drops late ones and gives the state at any moment: position — Hermite, velocities — linear, attitude — slerp. It does not interpolate between flying and landed packets or across a reference body change; when the buffer runs out, it continues the motion with the last velocity. Which moment to show (delay 100 ms × warp) is decided by the caller |
| `core/session_table.h` | `SessionTable` — the table of one session. On the host: accepting clients with a player limit (the host is a player too; 4 by default, from 2 to 8 — decision 14), handing out `net_id` in order without reuse, vessel ownership and checking whose input to apply; a client that leaves frees its vessels. On the client: the same table from `VESSELS`, `VESSEL_ADDED`, `VESSEL_REMOVED` and `OWNERSHIP` |
| `core/socket.h` | A socket wrapper: `UdpSocket`, `TcpConnection` (non-blocking connect, a send queue that finishes partial writes, receiving into `TcpFrameReader`), `TcpListener`; IPv4 addresses in digits only (`parseIpv4`). All sockets are non-blocking and never wait, there are no threads inside: the network runs in Orbiter's main thread (decision 23). Winsock and POSIX are in one `socket.cpp`; the network is started by `startNetwork` from `clbkSimulationStart`, not from `InitModule`. The Windows part is checked on a test rig (stage 0) |
| `core/attitude.h` | Conversion of the `arot` Euler angles into a quaternion and back with the same formula as in the Orbiter core (`Vessel.cpp:914`), and slerp between quaternions |
| `core/host_link.h` | `HostLink` — the host network side: listens for TCP and UDP, runs the handshake (`HELLO` → `WELCOME`/`REJECT` → `BODIES`, `VESSELS`, `OWNERSHIP`, then `CLASSES_MISSING` and `UDP_READY`), answers `PONG`, sends `WORLD_STATE` and `FUEL` split into packets of up to 1200 bytes and `LANDED` over TCP; removes clients that left from `SessionTable`. It does not know Orbiter: the plugin provides vessels and states |
| `core/client_link.h` | `ClientLink` — the client network side: connecting and the handshake, the host's vessel and body table, receiving `WORLD_STATE` (session and order check) into per-vessel interpolation buffers, `FUEL`, `LANDED`, `PING` once per second and the round trip time. The plugin asks for a vessel state at display time (`sampleVessel`) |
| `core/client_clock.h` | `ClientClock` — the client display clock: which moment of host sim time to show (the display delay behind the host). Between packets it runs by itself at the host warp rate, corrects a small error smoothly and a large one with a jump |
| `core/serialize.h` | `Writer` and `Reader`: writing and reading `u8`…`f64` and `str` fields little-endian, and raw bytes (a scenario file). `Reader` never reads past the end of the data: if bytes are missing, the read returns `false`, and so do all later reads |

In `plugin/`:

| File | What is in it |
|---|---|
| `plugin/OrbiterLanCoop.cpp` | The Orbiter module: the role from the settings, session and frame callbacks, a hidden window with a timer for working while paused (decision 23) |
| `plugin/config.h` | Settings from `Config\OrbiterLanCoop.cfg` (example — `plugin/OrbiterLanCoop.cfg`): role, host address, port, rate, display delay, trace |
| `plugin/host_side.h` | The "host" role: captures vessel states (`GetStatusEx`) in `clbkPreStep`, sends `WORLD_STATE` at `StateRate`, `FUEL` once per second, `LANDED` on landing and repeated for landed vessels |
| `plugin/client_side.h` | The "client" role: pause and warp as on the host, sim time near display time (a jump when the error exceeds `TimeSyncThreshold`), fuel, landing once, vessel states via `DefSetStateEx` in `clbkPreStep`, thruster levels |
| `plugin/trace.h` | Writes one vessel's position to CSV on the host and the client — to measure the error |
