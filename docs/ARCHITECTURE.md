**English** · [Русский](ru/ARCHITECTURE.md)

# How orbiter-lan-coop is built

What is where and which conditions must not be broken — see [README.md](README.md) in this folder.

> Below is how the plugin is made now (stages 0–3, 4A and 5, protocol version 7; code — section 6). The
> crew (4B) is not done. References to the Orbiter core are in [ORBITER-INTERNALS.md](ORBITER-INTERNALS.md).

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

- **Host** — Orbiter with the host role: a host-player with graphics who flies too, or a server without
  graphics (decision 34). Its simulation is the "truth".
- **Client** — an ordinary Orbiter with the same scenario. Its own vessel physics is
  replaced by the states it receives, and the player's input goes to the host.
- **Vessel ownership.** Every vessel has a "controller": the host, a specific client or
  nobody. A client's input is applied only to the vessel it controls.
- **The flight director** — a client with graphics under the name from the host setting `Director`:
  flies no vessel, their pause, time acceleration and save go to the host.

## 2. What happens at run time

### Connecting

1. The host starts a scenario and turns on the host role.
2. The client gets **the same world**. The SDK has no function to load a scenario at run time,
   so:
   - at stage 1 both start the same scenario by hand;
   - from stage 2 the pilot presses "Connect" in Launchpad, the host sends its current
     scenario, the client plugin selects it in Launchpad itself and starts it in the same process
     ([ORBITER-INTERNALS.md](ORBITER-INTERNALS.md), section 18; the fallback is restarting
     Orbiter with the `-s` switch) and connects by itself (more — [PLAN.md](PLAN.md), stage 2).
   Add-ons are not sent over the network: only a check and a list of what is missing.
3. The client receives the "number → name and class" and "vessel → controller" mappings. Vessels
   are matched by name: a handle in the core is a pointer inside the process and cannot be sent over
   the network. A vessel of a class the client lacks is not created (the class name goes to the host
   in `CLASSES_MISSING`).

### Every frame on the host

1. `clbkPreStep`: apply the latest received client input to their vessels.
2. There, capture the vessel states (`GetStatusEx`), time, warp and pause and send them
   to clients (up to 60 times per second, 60 by default, not every frame). A state refers to the moment
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
4. Time: pause and warp come from the host (`oapiSetPause`, `oapiSetTimeAcceleration`); for the
   flight director it is the other way round, their pause and warp go to the host (`TIME_REQUEST`).
   If the client sim time drifts from the host by more than a threshold (`TimeSyncThreshold`,
   0.1 s by default, × warp when accelerated), it is reset with a jump (`oapiSetSimMJD`); a smaller
   drift is only counted in the log.

### Pause and an inactive window

While paused, Orbiter does not call `clbkPreStep`, and only the active window receives keyboard
callbacks. So the plugin has its own hidden Windows window with a timer: its messages always
arrive. On the timer, while paused, the plugin receives packets and, when the host releases the pause,
calls `oapiSetPause(false)`. The pause flag is in every `WORLD_STATE` packet.
There is no separate network thread (decision 23): the sockets are non-blocking, receiving and sending happen
in Orbiter's main thread — in `clbkPreStep`, and while paused on the hidden window's timer.
Details — [ORBITER-INTERNALS.md](ORBITER-INTERNALS.md), section 14.

### Interface

- **Launchpad**, the "Extra" tab, the "OLC - LAN co-op" item: a Win32 dialog with the role (host
  or client), the host address, the port, the player name and a "Connect" button. The dialog saves
  the settings to `Config\OrbiterLanCoop.cfg` (`saveConfig`).
- **In the simulation** — messages to the pilot as text on the screen (`oapiCreateAnnotation`,
  20 s): which vessel is theirs, "view only" on someone else's, refusals. The connection state and
  network statistics go to `Orbiter.log`. The flight director also gets a permanent text on the left: the
  players, their vessels and the round trip time to the host (`core/player_summary.h`, from `PLAYERS`).
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
| Session | TCP | connection, scenario, vessel names and classes, ownership, single key presses, dockings, animations and frequencies, players, pause and save from the FD | on events; `PLAYERS` also every 2 s |
| World state | UDP | positions, velocities, attitudes, rotation, sim time, warp, pause | 60 Hz by default (decision 36), 5 Hz while paused |
| Input | UDP | control positions, key presses | every client frame |

## 3. Why it is done this way

| Decision | Rejected options and why |
|---|---|
| **Authoritative host** (all physics on one computer) | Everyone computes their own vessel (like OMP) — worlds drift apart, player docking and time acceleration break. Deterministic lockstep is impossible: Orbiter's simulation step comes from the real clock. |
| **A network, not two seats at one PC** | The core knows one focus, one camera, one window (`g_focusobj` — about 190 uses). Two sets of keyboard and mouse would mean rewriting the core. |
| **The host is the same Orbiter with the plugin**: a host-player or a server without graphics | No separate server program is needed: Orbiter can run without a graphics client (the "Orbiter Server Console"), and the physics is computed by the same code as everyone's (decision 34). |
| **A plugin, not a fork** | A fork has to be maintained after the main project. The SDK is enough for the prototype; missing hooks go as PRs to the main project. |
| **The base is `main` at a fixed commit**, the target is the nearest official release | The 2024 release is almost two years older; it has no vessel collisions, no ImGui dialogs, no `oapiGetKeyState`. |
| **LAN only** | The internet needs NAT handling, security and latency compensation — not needed for a classroom. |

## 4. What is needed to run

- Orbiter built from `orbitersim/orbiter` at the base commit (see [README.md](README.md),
  condition 3), the same on all computers, with the same add-ons.
- Windows 10/11 x64. Linux — via Wine/Proton, as a client.
- Vessel collisions are off on all machines (`VesselCollision = FALSE` in `Orbiter.cfg`, the package
  has it so): on a client the local calculation would argue with the host's states, and near vessels
  the core itself drops time acceleration to x1 ([ORBITER-INTERNALS.md](ORBITER-INTERNALS.md), section 22).

## 5. Main risks

| Risk | What is known | What came out |
|---|---|---|
| `DefSetStateEx` every client frame (about 60 times per second) | Inside: orbit re-initialization, gravity recalculation, iteration over bodies and bases, a check of neighbouring vessels' hulls | About 60 frames a second on the rigs, the drift is millimetres; smoothness by eye — a test by people |
| A vessel on the surface | The "landed" status is set once; the core may "land" a motionless vessel by itself after 5 s | A recorded DeltaGlider takeoff and landing are repeated on the client (up to 0.4 m at the runway) |
| Smoothing between packets | Recording playback in the core is not available from the SDK; our own buffer with Hermite interpolation ([SMOOTHING.md](SMOOTHING.md)) | Done in `core/interpolation.h`, checked on the rigs |
| Auto-docking on the client | The client's core may dock vessels that end up close by itself; the SDK has no switch for it | The client repeats the host's dockings (`DOCK`); docking by hand — a test by people |
| Internal state of add-on vessels | Hidden inside the add-on DLL | Visible mechanisms go as animations (4A); the internal module state does not change (decision 57) |
| A vessel of an unknown class | `oapiCreateVesselEx` with a missing class terminates Orbiter | The client checks the class before creating, the host's scenario is cleaned in advance |

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
| `core/protocol.h` | Protocol constants: version, port, UDP signature, size limits; comparison of UDP packet sequence numbers taking counter wrap into account; the next send time of a periodic packet (`nextSendTime`: from the planned time, so that at 60 frames per second `INPUT` and `WORLD_STATE` go out every frame, not every other frame) |
| `core/header.h` | The UDP header (writing and checking: size, signature, version) and TCP frames: `makeTcpFrame` builds a message with its length, `TcpFrameReader` takes whole messages from the stream and stops parsing on an invalid length |
| `core/interpolation.h` | `InterpolationBuffer` — the interpolation buffer of one vessel on the client (`SMOOTHING.md`): accepts states from `WORLD_STATE` by host sim time, drops late ones and gives the state at any moment: position — Hermite, velocities — linear, attitude — slerp. It does not interpolate between flying and landed packets or across a reference body change; when the buffer runs out, it continues the motion with the last velocity. Which moment to show (the display delay `DisplayDelay`, 30 ms by default, × warp) is decided by the caller |
| `core/scenario_filter.h` | Preparing the received scenario: removes vessels of classes the client lacks (otherwise Orbiter terminates while loading) and moves the focus and the camera to a remaining vessel |
| `core/session_table.h` | `SessionTable` — the table of one session. On the host: accepting clients with a player limit (the host is a player too; 4 by default, from 2 to 8 — decision 14), handing out `net_id` in order without reuse, vessel ownership and checking whose input to apply; take and release a vessel (one client — one vessel), a client that leaves leaves its vessel without an owner and closes it to others for 60 s. On the client: the same table from `VESSELS`, `VESSEL_ADDED`, `VESSEL_REMOVED` and `OWNERSHIP` |
| `core/socket.h` | A socket wrapper: `UdpSocket`, `TcpConnection` (non-blocking connect, a send queue that finishes partial writes, receiving into `TcpFrameReader`), `TcpListener`; IPv4 addresses in digits only (`parseIpv4`). All sockets are non-blocking and never wait, there are no threads inside: the network runs in Orbiter's main thread (decision 23). Winsock and POSIX are in one `socket.cpp`; the network is started by `startNetwork` from `clbkSimulationStart`, not from `InitModule`. The Windows part is checked on a test rig (stage 0) |
| `core/attitude.h` | Conversion of the `arot` Euler angles into a quaternion and back with the same formula as in the Orbiter core (`Vessel.cpp:914`), and slerp between quaternions |
| `core/host_link.h` | `HostLink` — the host network side: listens for TCP and UDP, runs the handshake (`HELLO` → `WELCOME`/`REJECT` → `BODIES`, `VESSELS`, `OWNERSHIP`, then `CLASSES_MISSING` and `UDP_READY`), answers `PONG`, receives `INPUT` from the vessel owner (the latest one per vessel, `latestInput`), handles `OWNER_REQUEST` (take, release; a refusal is `OWNER_DENIED`, changes go to everyone in `OWNERSHIP`), `SAVE_REQUEST` from the director (`takeSaveRequests`), sends `WORLD_STATE` and `FUEL` split into packets of up to 1200 bytes, `LANDED`, `VESSEL_ADDED` and `VESSEL_REMOVED` over TCP, `DOCK` (`sendDock`; the current links also to those who join later), `ANIM_STATE` (`sendAnimState`) and `NAV_STATE` (`sendNavState`; the latest values also to those who join later); accepts `NAV_STATE` only from the owner of the vessel (`takeNavStates`) and sends it on to the others; accepts `KEY` for the client's own vessel (`takeKeys`); accepts `TIME_REQUEST` only from the flight director (`HostConfig::director`, `takeTimeRequests`) and sends `PLAYERS` (on join, leave and ownership change and every 2 s); an observer cannot take a vessel; answers a `HELLO` without a scenario number with `SCENARIO` (the plugin gives the file through `makeScenario`) and closes the connection; removes clients that left (including those silent for `kSilenceTimeout` = 15 s) from `SessionTable`. It does not know Orbiter: the plugin provides vessels and states |
| `core/client_link.h` | `ClientLink` — the client network side: connecting and the handshake, the host's vessel and body table, receiving `WORLD_STATE` (session and order check) into per-vessel interpolation buffers, `FUEL`, `LANDED`, new and deleted vessels (`takeVesselChanges`), requests to take and release a vessel (`requestTake`, `requestRelease`) and refusals (`takeOwnerDenials`), single key presses `KEY` (`sendKey`), pause and warp requests of the flight director `TIME_REQUEST` (`sendTimeRequest`) and save requests `SAVE_REQUEST` (`sendSaveRequest`), animations and frequencies (`takeAnimStates`, `takeNavStates`, `sendNavState`), the player list `PLAYERS` (`players`), dockings `DOCK` (`takeDocks`), receiving `SCENARIO` on request, `INPUT` at most 60 times per second (`sendInput`), `PING` once per second and the round trip time, closing if the host is silent for 15 s. The plugin asks for a vessel state at display time (`sampleVessel`), for measurements — the latest received one (`latestVessel`) |
| `core/player_summary.h` | Player summary text for the flight director's screen: players, roles, vessels, latency (from `PLAYERS`); no Orbiter calls, covered by a test |
| `core/keymap.h` | Key layout: reads Orbiter's `keymap.cfg`, turns held vessel-control keys into `INPUT.keys` bits and clears them in the key state so that Orbiter does not fly the local copy. RCS rotation and translation share keys, so both bits are sent and the host picks by the vessel's RCS mode |
| `core/client_clock.h` | `ClientClock` — the client display clock: which moment of host sim time to show (the display delay behind the host). Between packets it runs by itself at the host warp rate, corrects a small error smoothly and a large one with a jump |
| `core/serialize.h` | `Writer` and `Reader`: writing and reading `u8`…`f64` and `str` fields little-endian, and raw bytes (a scenario file). `Reader` never reads past the end of the data: if bytes are missing, the read returns `false`, and so do all later reads |

In `plugin/`:

| File | What is in it |
|---|---|
| `plugin/OrbiterLanCoop.cpp` | The Orbiter module: the role from the settings, session and frame callbacks, a hidden window with a timer for working while paused (decision 23) |
| `plugin/config.h` | Settings from `Config\OrbiterLanCoop.cfg` (example — `plugin/OrbiterLanCoop.cfg`): role, host address, port, name, player limit, rate, the flight director (`Director`), autosave (`AutosaveMinutes`), display delay, trace; writing the file from Launchpad (`saveConfig`) |
| `plugin/host_side.h` | The "host" role: captures vessel states (`GetStatusEx`) in `clbkPreStep`, sends `WORLD_STATE` at `StateRate`, `FUEL` once per second, `NAV_STATE` of the vessel the host flies (and applies the pilots' ones to its copies), `ANIM_STATE` (animations of every vessel read with `GetAnimPtr`: changes above 0.01, at most 5 scans per second per vessel), `LANDED` on landing and repeated for landed vessels; vessels created and deleted during the session — `VESSEL_ADDED` and `VESSEL_REMOVED`; the host-player owns the vessel in its focus, a server without graphics owns none; every frame outside pause it applies the pilots' `INPUT` (`vessel_control.h`), `INPUT` older than 0.5 s is not applied; when the set of a pilot's keys changes it logs what they did (RCS mode and levels, engines, trim, brakes, rotation); carries out the flight director's pause and time acceleration, saves the world on their request and every `AutosaveMinutes` |
| `plugin/client_side.h` | The "client" role: pause and warp as on the host, sim time near display time (a jump when the error exceeds `TimeSyncThreshold`), fuel, animations from `ANIM_STATE` (`SetAnimation`), frequencies (`NAV_STATE`: the owner's NAV and transponder steps are checked every 0.5 s and sent on change; the others' are set on the copies), landing once, vessel states via `DefSetStateEx` in `clbkPreStep`, thruster levels; the list of vessels as on the host: creates missing ones if the class is installed, deletes extra ones and those the host deleted (except the last vessel); the own vessel — by focusing a free one, Ctrl+Alt+U releases it, notices on screen and in the log; sends `INPUT` for the own vessel every frame (keys while it is in focus, plus the joystick) and clears control keys locally; for the flight director — pause, acceleration and save requests (Ctrl+Alt+S) and the player summary on the screen |
| `plugin/vessel_control.h` | Applies a pilot's `INPUT` to the vessel on the host as Orbiter's core does for the focus vessel: RCS single-step by RCS mode, full main and retro, kill rotation, throttle and hover 0.2 per second, trim 0.25 per second, wheel brakes |
| `plugin/launchpad_item.h` | The "OLC - LAN co-op" item in Launchpad (the "Extra" tab): role, address, port, name; the "Connect" button gets the host's scenario (`HELLO` without a number) and starts it |
| `plugin/scenario_launch.h` | Starting the received scenario: vessels without an installed class are removed (`scenario_filter.h`), writing it to `Scenarios\OLC\Host world.scn`, selecting it in the Launchpad list and pressing "Launch" (`ORBITER-INTERNALS.md`, section 18) |
| `plugin/trace.h` | Writes one vessel's position to CSV on the host and the client — to measure the error |
| `plugin/wall_clock.h` | The wall clock for latency measurements: `oapiGetSysTime` does not see frames longer than 0.1 s (`ORBITER-INTERNALS.md`, section 4) |
