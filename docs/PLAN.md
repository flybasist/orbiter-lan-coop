**English** · [Русский](ru/PLAN.md)

# Work plan

The goal of the first part is a **prototype** that answers the question "is the approach viable".
The project is announced publicly only after it. The detailed plan of stages 2–5 is written after
stage 1, when the real problems are known.

Task IDs keep the letters of the Russian source in Latin form: D — smoothing model, V — network core,
I — research, S — test rigs.

## Stages

| Stage | Result | Status |
|---|---|---|
| 0. Test rig | Orbiter built from the base commit, an empty plugin loads, the plugin builds in CI | done (Windows 11 ARM, 2026-11-17) |
| 1. Idea check | The host sends vessel states, the client shows them; shared time and pause | in progress: checked with graphics and over the network between machines; smoothness by eye in a live window remains |
| 2. Observer | The client sees the host's world, switches camera and vessels | in progress: detailed plan approved on 2026-11-17; 2.1, 2.2 and the host part of 2.3 are done (built, not checked on a rig) |
| 3. Two vessels (the first working version) | Everyone controls their own vessel; rendezvous and docking of two DeltaGliders in orbit | — |
| 4. Crew | Two in one vessel: pilot and navigator with MFDs | — |
| 5. For the instructor | Shared pause, watching any vessel, saving and debriefing a flight | — |

The order of stages 4 and 5 depends on the space club's answers (what matters more to them).
Alongside all stages: the plugin log, network statistics (delay, loss, position
error), documentation.

## Work queue without Windows

Tasks that are done and checked on a Mac while there are no test rigs. The AI agent in autonomous
mode takes the **first unfinished** task from the top. Every task is a finished piece:
the code builds, the tests pass, the documentation is updated. Statuses: `waiting`, `in progress`,
`done`, `stop: question Qn` (waiting for an answer in [QUESTIONS.md](QUESTIONS.md)),
`in progress (main session)` — the owner's session does the task, the autonomous agent skips it.

| # | Task | Result | Status |
|---|---|---|---|
| D1 | Smoothing model: a reference trajectory (orbit, a maneuver with thrust, rotation), host packets with a rate, delay, delay jitter and loss; the client reconstructs motion in different ways (last state, linear interpolation, cubic over position and velocity, extrapolation) | A program in `tools/smoothing/`, builds with CMake | done |
| D2 | Running the model over a grid: 10/20/30 Hz, loss 0/5/20%, delay jitter 1/20/50 ms, warp ×1/×10/×100 | `docs/SMOOTHING.md`: error tables, the recommended rate, buffer delay and smoothing method | done |
| V1 | Network core skeleton: a folder and CMake for a library independent of Orbiter, simple CTest tests without third-party libraries | Build and `ctest` in `orbiter-lan-coop.out`; a "Code" section in `ARCHITECTURE.md` | done |
| V2 | Serialization of protocol fields (little-endian, `u8`…`f64`, `str`) with bounds checks | Code and tests | done |
| V3 | Messages from [PROTOCOL.md](PROTOCOL.md): packing, parsing, header and length checks. Done: headers (`core/header.h`), type numbers and all TCP and UDP messages (`core/messages.h`) | Code and tests, including broken packets | done |
| I1 | Plugin life outside the frame: which callbacks are called while paused, for an inactive and minimized window, in Launchpad; how a network thread safely passes commands to Orbiter's main thread (known: `clbkPreStep` is not called while paused, keyboard callbacks — only for the active window, `Orbiter.cpp:1060`) | A section in `ORBITER-INTERNALS.md`, a decision on the threading scheme in `ARCHITECTURE.md` (a fork — a question to the owner) | done |
| I2 | Two Orbiter instances on one machine: protection against a second copy, shared files (`Orbiter.cfg`, `Orbiter.log`, the "Current state" scenario), what is needed to run from different folders | A section in `ORBITER-INTERNALS.md`, the procedure for stage 0 in `PLAN.md` | done |
| V4 | State interpolation buffer based on D2: Hermite over position and velocity, a 100 ms delay at 30 Hz ([SMOOTHING.md](SMOOTHING.md)) | Code and tests | done |
| V5 | Vessel and ownership table: `net_id` ↔ name, owner, player limit | Code and tests | done |
| V6 | Socket wrapper (BSD sockets on Mac and Linux, Winsock on Windows), TCP frames and UDP | Code and a "host and client on loopback" test on Mac; the Windows part is checked at stage 0 | done (Windows — at stage 0) |
| V7 | The `FUEL` message ([PROTOCOL.md](PROTOCOL.md), decision 24): packing and parsing, like `WORLD_STATE` | Code and tests, including broken packets | done |
| I3 | Plugin skeleton: how the standard plugins are built (`Src/Plugin`), DLL export, a separate build against the SDK; an empty OLC plugin and a GitHub Actions workflow (decision 9), the Orbiter core is built in the workflow from the base commit (decision 25) | The plugin code and the workflow are ready; the task is "done" only after a green CI build following the owner's push, until then the status is "stop: waiting for push" | done (CI green 2026-11-07) |
| I4 | OLC interface: a Launchpad item (`oapiRegisterLaunchpadItem`) and windows inside the simulation (ImGui): connection, status, network statistics | A section in `ORBITER-INTERNALS.md` and `SDK-CHEATSHEET.md`, an interface sketch in `ARCHITECTURE.md` | done |
| I5 | Vessel state beyond motion: what else is visible and affects the game (fuel, thruster levels and exhaust, animations — gear, doors, docking port, lights; DeltaGlider system states), what of it can be read and set through the SDK on another player's vessel and what has to be sent | A section in `ORBITER-INTERNALS.md`; proposals to extend `PROTOCOL.md` (a fork — a question to the owner) | done |

The AI agent adds new code research to the queue itself if it is needed to understand
Orbiter or for the architecture (decision 21), and does it in order. Tasks that need
Orbiter running (stage 0 and later) wait for test rigs: no workarounds.

## Work queue on test rigs

Tasks on machines from `stands.yaml` ([STANDS.md](STANDS.md)). The same statuses and rules as
above; the agent works within the machine's `access` field (decision 27).

| # | Task | Result | Status |
|---|---|---|---|
| S1 | The network core on Linux: building `olc-core` and the tests on x86 and ARM (GCC) | A build without warnings, `ctest` passes on both machines | done (GCC 15.2, x86 and ARM, 2026-11-17) |
| S2 | The `olc-netcheck` utility: a server and a client, TCP and UDP exchange between two machines, delay and loss | Code in `tools/netcheck/`, a measurement between machines, a section in `STANDS.md` | done (Wi-Fi and NAT; wired and between Windows machines — stage 1 measurements, 2026-11-17) |
| S3 | Stage 0 on Windows ARM: building Orbiter from the base commit, two installation copies | Orbiter is built, differences from `QUICKSTART.md` are added to it | done (2026-11-17): a full build with D3D9Client and `cmake --install`, SPICE kernels in place; the installation has no Earth textures (section 4 of `QUICKSTART.md`) |
| S4 | Stage 0: the OLC plugin built on the test rig, loads in both copies, a line in `Orbiter.log` | Confirmation in `PLAN.md` | done (2026-11-17): MSVC `/W4` without warnings, `ctest` 10/10, the plugin loads in both copies, with and without graphics |
| S5 | Stage 0: starting on the visible desktop (the "OLC Run Orbiter" scheduled task), FPS under emulation, two instances, the server console without graphics | Results in `STANDS.md` (section 7) and `ORBITER-INTERNALS.md` | done (2026-11-17): two copies with D3D9Client side by side on the visible desktop, screenshots; FPS under emulation — about 60 on the client and 22 on the host playing back a recording |
| S7 | Stage 1, code: `HostLink`, `ClientLink`, `ClientClock` in the core with loopback tests; host and client roles in the plugin, settings from a file, a trace for measurements | Two Orbiter copies without graphics: connection, states, pause, warp | done (2026-11-17), results — the "Stage 1" section |
| S8 | Stage 1 with graphics: smoothness by eye, a vessel on the runway, takeoff and landing, host and client on different machines | Results against the stage 1 criteria | almost done (2026-11-17): one machine and different machines checked, results — the "Stage 1" section; smoothness by eye in a live window remains |
| S6 | DirectX SDK on the Windows test rig: installation with `tools/stands/setup-windows.ps1` (S1023 workaround), then a full Orbiter build with D3D9Client and `cmake --install` | Orbiter with graphics in two copies | done (the owner installed the SDK on 2026-11-17) |

## Stage 0. Test rig

1. Build Orbiter from the base commit on Windows (see [QUICKSTART.md](QUICKSTART.md)).
2. Check on machines from `stands.yaml` ([STANDS.md](STANDS.md)):
   - Windows 11 ARM (if available): does x64 build, what FPS D3D9 gives;
   - Windows x64: the reference machine;
   - two Orbiter instances on one machine — the main development mode: two copies of the
     `install` folder (`host` and `client`), each started from its own folder in windowed mode,
     the second one with the joystick off ([ORBITER-INTERNALS.md](ORBITER-INTERNALS.md), section 15);
   - running without a graphics client (the "Orbiter Server Console");
   - Steam Deck and Linux x86: running Orbiter through Proton/Wine.
3. An empty plugin: loads, is enabled on the Modules tab, writes a line to `Orbiter.log`.
4. GitHub Actions: building the plugin on `windows-latest`.
5. The Windows part of the socket wrapper (`core/socket.cpp`): building `olc-core` with MSVC without
   warnings and the `socket` test; a refused connection must give `Closed`, not
   an endless `Connecting`.

## Stage 1. Idea check

What we do:

1. The host in `clbkPreStep` captures the states of all vessels and sends them over UDP (20–30 Hz).
2. The client in `clbkPreStep` imposes them on vessels via `DefSetStateEx`.
3. Pause and time acceleration follow the host, the sim time is pulled in.
4. Connecting: the client starts from the same scenario (by hand or sent by the host).

**The prototype counts as a success if:**

- the plugin builds against the base commit and loads into Orbiter;
- the host flies a DeltaGlider, the client sees the motion without noticeable jerks;
- the vessel position error between client and host in orbit is a few metres;
- the client sim time differs from the host by no more than 0.1 s;
- a vessel standing on the runway does not jump or fall through on the client;
- a pause on the host is visible on the client at once;
- desirable but not required: time acceleration up to ×100 is also visible on the client at once
  (if not, it moves to stage 2).

**Results without graphics (2026-11-17, Windows 11 ARM, two Orbiter copies on one machine,
the `Checklists\Quickstart` scenario, 8 vessels):**

- the client connects, receives vessels and bodies, pulls in the sim time with one jump;
- the ISS position error on the client against the host at the same host moment: p50 2 mm, p99 6 mm
  (except the first 0.1 s after connecting, when display time is still earlier than the first packet);
- the client sim time stays within 0.03 s of display time, which by design lags the host
  by the display delay (0.1 s × warp); at ×10 — 0.4 s, at ×100 — 1.4 s of sim time, that is
  0.04 and 0.014 s of real time;
- pause, releasing the pause and warp ×10 and ×100 reach the client; no loss and no empty buffer;
- after a warp change the display clock catches up with the new target smoothly, and there may be one jump
  of the client sim time — whether it is visible to the eye is checked with graphics (S8);
- a recorded DeltaGlider flight (`Playback\Mojave to Edwards`, 5.5 min: takeoff, flight,
  landing): the host plays the recording, the client has the same scenario without it. Position
  error p99 5 mm, maximum 0.38 m at touchdown; the landing on the client is applied
  once — when display time reaches it (the first version applied it earlier, and the next frame
  lifted the vessel again).

**Results with graphics (2026-11-17, the same machine, two copies with D3D9Client side by side on screen):**

- a recorded DeltaGlider flight (the same scenario, an external camera on the client): the client shows
  the takeoff, the flight, the approach with the gear down and the landing at the same moments as the host; the exhaust and
  the engine plume are visible — thrust levels arrive;
- position error p50 1 mm, p99 11 mm, maximum 0.43 m at touchdown;
  the client draws about 60 frames per second, no frame was left without data;
- the host under x64 emulation on ARM gives about 22 frames per second and therefore sends about 22
  states per second instead of 30 (a state is captured once per frame) — this does not get in the way of the client's
  smoothness;
- the client sim time is on average 0.04 s from display time (0.003 s without graphics): the host has
  long frames, and the time in a packet lags the send moment by part of a frame. Within the
  criterion, but worth keeping in mind with a slow host;
- the vessel on the runway stands still before takeoff and after landing, the landing on the client is applied once each time.

Smoothness was judged from traces and screenshots, not by eye in a live window: a screenshot is
a single frame.

**Results over the network between machines (2026-11-17, the recorded DeltaGlider flight):**

| Host | Client | Network | RTT, median / maximum | Error p50 / p99 / maximum | Client sim time |
|---|---|---|---|---|---|
| Windows 11 x64 (Proxmox), no graphics | Windows 11 ARM (Parallels), with graphics | Mac Wi-Fi and Parallels NAT | 17 / 101 ms | 1.1 mm / 7.3 mm / 0.17 m | 0.04 s from the display moment |
| Windows 11 x64 (Proxmox), no graphics | Windows 10 x64 (Proxmox), no graphics | Wired, one network, no NAT | 0.5 / 1.8 ms | 1.0 mm / 5.4 mm / 0.17 m | 0.001 s from the display moment |

- The maximum is at the moment of touchdown, as on one machine. Not counting the first 0.1 s after
  connecting: the client connected 22 s after the host started and caught up the sim time with one jump.
- The host on x64 without emulation sends 30 states per second, as set (under emulation it was 22).
  No lost and no malformed packets in any run.
- Through the Parallels NAT the client named one port in `UDP_READY`, and `PING` came from another.
  The host switched to the address from `PING` (decision 28) — without that the client would not get
  a single state.
- Over Wi-Fi the client lacked fresh data in 0.3% of frames (delays of up to 100 ms happen with
  a display delay of 0.1 s); this did not affect the error.

At stage 1 two people play. The project's goal is 4 players, the upper limit is 8 (decision 14 in
[DECISIONS.md](DECISIONS.md)); 4 are checked at stage 3.

## Stage 2. Observer: connecting with one button

How the client gets the host's world (decisions 16, 17 and 30 in [DECISIONS.md](DECISIONS.md)):

1. A "Connect" button and the host address — in Launchpad (`oapiRegisterLaunchpadItem`).
2. The host saves the current world (`oapiSaveScenario`) and sends the file to the client.
3. The client plugin starts this scenario. Reconnaissance on 2026-11-17
   ([ORBITER-INTERNALS.md](ORBITER-INTERNALS.md), section 18): in the same process this
   works — the plugin closes the session, puts the scenario in place, selects it in Launchpad and
   presses "Launch" itself. This is the main path (decision 30); restarting the process with `-s` from decision 16
   remains a fallback. Another fallback: the scenario is selected in Launchpad, and the student
   starts it with one click.
4. Vessels that appear on the host during the session are created by the client plugin itself
   (`oapiCreateVesselEx`), after checking that the class is installed.
5. Add-ons are not sent over the network: the plugin shows what is missing.

If some item runs into the core and cannot be solved by the plugin, that is the main risk found.
It is assessed separately (a workaround or a PR to the core) before the work continues.

### Detailed plan (approved by the owner on 2026-11-17)

**How connecting goes** (one button for the student):

1. In Launchpad the student opens the "OLC" item (the "Extra" tab), types the host address and a name and
   presses "Connect". There is no session yet.
2. The plugin opens TCP and sends `HELLO` without a scenario number. The host saves the current world to
   a file and replies with `SCENARIO` carrying the number of this save, then closes the connection.
3. The client plugin writes the file to `Scenarios\OLC\<host name>.scn`, selects it in the
   Launchpad list and presses "Launch" itself (section 18 of `ORBITER-INTERNALS.md`).
4. At the start of the session the plugin connects again, now with the save number in `HELLO`. The host
   accepts it as an ordinary client: `WELCOME`, `BODIES`, `VESSELS` — as at stage 1.
5. While the client was loading (about 20 s), vessels may have appeared and disappeared on the host. The client
   compares its vessels with the `VESSELS` list by name: it creates the missing ones and deletes the extra ones.
6. After that, during the session the host reports new and deleted vessels (`VESSEL_ADDED`,
   `VESSEL_REMOVED`), and the client does the same on its side.

**Task queue:**

| No. | Task | Where | Done when |
|---|---|---|---|
| 2.1 | Protocol v3: a save number in `HELLO` (`scenario_id`, 0 — no scenario yet), the same number in `SCENARIO`. A decision in `DECISIONS.md`, `PROTOCOL.md` | Mac, core | Parsing and handshake tests for both variants |
| 2.2 | Host: on `HELLO` without a number — ask the plugin for the scenario file, send `SCENARIO`, close the connection. A file over 1 MB — `REJECT` with a clear text | Mac, core | A loopback test: the client gets the file byte for byte |
| 2.3 | Host: new and deleted vessels during the session — `clbkNewVessel`, `clbkDeleteVessel` → the table, `VESSEL_ADDED`, `VESSEL_REMOVED`, the vessel in `WORLD_STATE` | Mac (core) + Windows (plugin) | A core test; on the rig a vessel created on the host appears on the client |
| 2.4 | Client: create a vessel (`oapiCreateVesselEx`) only if `Config\Vessels\<class>.cfg` exists, otherwise — into `CLASSES_MISSING`; delete a vessel on `VESSEL_REMOVED`; reconcile against `VESSELS` at session start | Windows ARM, plugin | No class file — Orbiter does not crash, the host logs what the client is missing |
| 2.5 | Client: start the received scenario through Launchpad — write the file to `Scenarios\OLC\`, select it by path, "Launch"; if a session is running — close it first | Windows ARM, plugin | Reconnecting without restarting Orbiter, 10 times in a row without failures |
| 2.6 | The "OLC" item in Launchpad: role, address, port, name, a "Connect" button, a status line (connecting, host refusal with the reason, missing classes). Settings — in `Config\OrbiterLanCoop.cfg`, as now | Windows ARM, plugin | The student connects without touching files |
| 2.7 | A check on the rigs: the host on Windows x64, a client with graphics; connecting with the button in the middle of the host's flight, creating and deleting a vessel on the host, refusal for a different version and for a missing class | Proxmox + Parallels | Results in the "Stage 2" section against the criteria below |

**Stage 2 succeeds if:**

- the student connects with one button and after 30–40 s sees the host's world — the same vessels in the same
  places as on the host;
- a vessel created or deleted on the host during the session appears or disappears on the client;
- the student switches the camera and the focus vessel (F3), and the picture does not break;
- a missing vessel class does not crash Orbiter on the client: the player sees what is missing;
- a host refusal (version, another system, no free places) is shown in Launchpad in a clear text;
- reconnecting works without restarting Orbiter.

**Owner's decisions (2026-11-17):** the plan is approved; the Launchpad path is the main one, restarting
with `-s` is the fallback (decision 30).

Not part of stage 2: controlling one's own vessel (stage 3), docking, sending add-ons,
an ImGui window in the simulation (the log and notifications are enough for now).

## Test rigs

Which systems are suitable, machine roles, the minimal set and preparation — [STANDS.md](STANDS.md).
Everyone describes their own machines in `stands.yaml` (not in git).

| Stage | Which machines are needed |
|---|---|
| 0 and the start of 1 | One Windows machine: build, host and client as two instances |
| The whole of 1 | Plus a second machine on the same network; at least one is Windows x64 without emulation |
| 2 | The same |
| 3 | Up to 4 machines; Linux clients (Wine, Proton); live tests on other people's Windows machines |
