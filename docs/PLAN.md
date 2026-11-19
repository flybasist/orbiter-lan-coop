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
| 2. Observer | The client sees the host's world, switches camera and vessels | in progress: detailed plan approved on 2026-11-17; 2.1–2.7 are done, 5 of 6 criteria checked on rigs; F3 remains to be looked at by eye |
| 3. Two vessels (the first working version) | Everyone controls their own vessel; rendezvous and docking of two DeltaGliders in orbit | in progress: 3.0–3.6 and 3.8–3.17 are done, the package, the procedure and the scenarios for 3.7 are ready; waits for the docking check by people |
| 4. Crew | 4A: everyone sees the vessel state (mechanisms, NAV frequencies); 4B: two in one vessel — pilot and navigator | 4A — in progress: research and protocol are done (decisions 57, 58), queue 4A.1–4A.4; 4B — postponed |
| 5. For the flight director | Shared pause, watching any vessel, saving and debriefing a flight | done except the check by people: 5.1–5.9 (protocol version 6, rig check) |

Order: 4A (stage 5 is done), 4B — later (decisions 46, 52). Everything our rigs can check is done without waiting for the tests by people (decision 53).
Alongside all stages: the plugin log, network statistics (delay, loss, position
error), documentation.

## Work queue without Windows

Tasks that are done and checked on a Mac while there are no test rigs. The AI agent in autonomous
mode takes the **first unfinished** task from the top. Every task is a finished piece:
the code builds, the tests pass, the documentation is updated. Statuses: `waiting`, `in progress`,
`done`, `stop: question Qn` (waiting for an answer in [QUESTIONS.md](QUESTIONS.md)),
`in progress (main session)` and `waiting (main session)` — the owner's main session on Opus does the task
(decision 48); a scheduled run does not take it but wakes the main session.

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
   remains a fallback. Another fallback: the scenario is selected in Launchpad, and the pilot
   starts it with one click.
4. Vessels that appear on the host during the session are created by the client plugin itself
   (`oapiCreateVesselEx`), after checking that the class is installed.
5. Add-ons are not sent over the network: the plugin shows what is missing.

If some item runs into the core and cannot be solved by the plugin, that is the main risk found.
It is assessed separately (a workaround or a PR to the core) before the work continues.

### Detailed plan (approved by the owner on 2026-11-17)

**How connecting goes** (one button for the pilot):

1. In Launchpad the pilot opens the "OLC" item (the "Extra" tab), types the host address and a name and
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
| 2.6 | The "OLC" item in Launchpad: role, address, port, name, a "Connect" button, a status line (connecting, host refusal with the reason, missing classes). Settings — in `Config\OrbiterLanCoop.cfg`, as now | Windows ARM, plugin | The pilot connects without touching files |
| 2.7 | A check on the rigs: the host on Windows x64, a client with graphics; connecting with the button in the middle of the host's flight, creating and deleting a vessel on the host, refusal for a different version and for a missing class | Proxmox + Parallels | Results in the "Stage 2" section against the criteria below |

**Stage 2 succeeds if:**

- the pilot connects with one button and after 30–40 s sees the host's world — the same vessels in the same
  places as on the host;
- a vessel created or deleted on the host during the session appears or disappears on the client;
- the pilot switches the camera and the focus vessel (F3), and the picture does not break;
- a missing vessel class does not crash Orbiter on the client: the player sees what is missing;
- a host refusal (version, another system, no free places) is shown in Launchpad in a clear text;
- reconnecting works without restarting Orbiter.

**Owner's decisions (2026-11-17):** the plan is approved; the Launchpad path is the main one, restarting
with `-s` is the fallback (decision 30).

**Results of check 2.7 (2026-11-18).** The host is Windows 11 x64 without graphics (Proxmox), the clients are
Windows 11 ARM with graphics (Parallels, Wi-Fi and NAT) and Windows 10 x64 without graphics. Buttons in
Launchpad were pressed by `run-orbiter.ps1` (the `ui` field), vessels were created and deleted by `OlcRigTest`.

| Criterion | Result |
|---|---|
| Connecting with one button in 30–40 s | Yes: 21 s from "Connect" to the session, the same vessels |
| A vessel created and deleted on the host appears and disappears on the client | Yes, with and without graphics |
| Switching the camera and focus (F3) does not break the picture | Not checked: needs to be looked at by eye |
| A missing class does not crash Orbiter, the player sees what is missing | Yes: the vessel is removed from the scenario before start, the host gets `CLASSES_MISSING`, the list of classes stays on screen for 20 s |
| A host refusal is shown in a clear text | Yes for "no places": "Not connected: rejected by host: no free player slots". Refusal by version is checked by core tests |
| Reconnecting without restarting Orbiter | Yes: 10 cycles "Connect" → session → close in a row without failures |

Found on the way: a vessel of an unknown class in the received scenario would crash Orbiter already while
loading, so a scenario filter was added (`core/scenario_filter.h`).

Not part of stage 2: controlling one's own vessel (stage 3), docking, sending add-ons,
an ImGui window in the simulation (the log and notifications are enough for now).

## Stage 3. Two vessels: everyone controls their own

### Detailed plan (owner's decisions of 2026-11-18)

The goal is the first working version: a pilot flies their own vessel from the client, the host
computes the physics, two players rendezvous and dock in orbit. The basis is already there: the
protocol describes `INPUT`, `KEY`, `OWNERSHIP`, `DOCK`, the session table knows the owners, the ways to
apply input to another vessel were found in the sources ([ARCHITECTURE.md](ARCHITECTURE.md), "How input
reaches a vessel on the host"; [ORBITER-INTERNALS.md](ORBITER-INTERNALS.md), sections 7 and 9).

**Task queue:**

| # | Task | Where | Done when | Status |
|---|---|---|---|---|
| 3.0 | Measuring the smoothing buffer and the state rate on a wired pair (30 Hz / 50 ms, 60 Hz / 25–30 ms), defaults for a LAN | Proxmox | Numbers in the results, the decision in `DECISIONS.md` | done (2026-11-18) |
| 3.1 | Vessel ownership by decision 1 below (A2): take, release, keeping for 60 s after a break. The host sends `OWNERSHIP`, a refusal and "view only" are shown as on-screen notices | Mac (core) + Windows | A core test: take, release, a pilot leaving and coming back; two pilots do not control one vessel | done (2026-11-18; on the rigs: taking at join and "view only"; the take and release rules — by core tests; the Ctrl+Alt+U and F3 keys were not pressed on a rig) |
| 3.2 | Client: the key layout from `keymap.cfg`; control keys and the joystick of the own vessel → `INPUT` over UDP every frame (at most 60 Hz); control keys of any vessel are cleared locally, the camera works | Windows ARM | `INPUT` arrives in the host log, the local physics does not argue with the host | done (2026-11-18; full throttle by key checked on the rig, the other keys and the joystick — by tests and by code) |
| 3.3 | Host: apply `INPUT` to the client's vessel (RCS — `IncThrusterGroupLevel_SingleStep`, throttle, hover, trim, brakes — as the core does); no `INPUT` for more than 0.5 s — release the controls | Windows x64 | A rig check: the client turns and accelerates its vessel, the host sees the same | done (2026-11-18; full throttle by key checked on the rig, the other keys and the joystick — by tests and by code) |
| 3.4 | One-time key presses → `KEY` over TCP → `SendBufferedKey` on the host: autopilot modes, RCS, vessel keys (gear, hatches) | Windows | The "Prograde" autopilot and the DeltaGlider gear from the client work on the host | done (2026-11-18; "Prograde" `[` and gear `G` checked on the stand; RCS keys from the client as in 3.2–3.3) |
| 3.5 | Docking: the host reports `DOCK` (docking and undocking), the client repeats `Dock` in mode 0 and moves the stack through one vessel | Mac + Windows | `OlcRigTest` docks and undocks two vessels on the host — the client shows the same, without jumps | done (2026-11-18; docking and undocking are repeated on the client on the stand; jerks were not measured, only looked at in a screenshot) |
| 3.6 | Measuring control delay: from a key press on the client to motion on the client's screen | Rigs | A number in the results | done (2026-11-18; median 67 ms at 60 Hz and a 30 ms display delay, see the results of 3.6) |
| 3.7 | A check by people step by step: scenarios (DeltaGlider to a still station; two DeltaGliders 20 m apart; rendezvous from different orbits) and a memo for pilots in two languages | The agent prepares, the owner or the club checks | Results against the criteria below | scenarios and guide are ready (2026-11-18, `tools/scenarios/`, [PILOT-GUIDE.md](PILOT-GUIDE.md)); waiting for people |
| 3.8 | A server without graphics on the flight director's machine, the flight director as a client (decision 34): a description in `QUICKSTART.md`, a check of two copies on one machine | Rigs | The flight director connects to the server on the same machine | done (2026-11-18; Windows 11 ARM: a server without graphics and a client with graphics on one machine, `HostAddress = 127.0.0.1`, RTT about 16 ms; described in `QUICKSTART.md`, section 6) |
| 3.9 | A headless host sometimes does not exit on `-T` (the log stops ~27 s before the limit): find the cause | Proxmox | The cause in the results; fixed if it is in OLC | done (2026-11-18): on the rigs `-T` fires on time, the cause is an outer run timeout, see the 3.9 results |
| 3.10 | Control latency outliers (148 and 370 ms in 3.6): timestamps along the path "key press → `INPUT` → host frame → state → display" | Test rigs | The source of the outliers in the results | done (2026-11-18): the outliers come from the rig network (Wi-Fi and NAT), see the 3.10 results |
| 3.11 | Extended input check on a rig: RCS (rotation and translation), throttle, trim, brakes, Ctrl+Alt+U, F3 to another pilot's vessel | Proxmox + Parallels | A result for every key in the stage section | done (2026-11-18): everything except the retro engine is confirmed by the host log, see the 3.11 results |
| 3.12 | A detailed plan of stages 4 and 5 for the owner's approval | Mac | A section in `PLAN.md`, a question in `QUESTIONS.md` | done (2026-11-18): section "Stages 4 and 5: detailed plan", question Q8 |
| 3.13 | Code audit (core, plugin, tools): bugs, bounds, dead code; every finding checked and fixed | Mac + Windows | Builds without warnings, tests pass, results in the stage section | done (2026-11-18): the bugs found are fixed, 4 findings are not bugs, see the results of 3.13 |
| 3.14 | Audit of the documentation (both language versions) and code comments: everything matches the code and is clear to people and agents | Mac | No mismatches, `olc-doccheck` passes | done (2026-11-18): documents and comments checked against the code, see the results of 3.14 |
| 3.15 | Research of textures and add-ons: where people get them, size, licenses, what may go into the archive | Mac | A section in the documentation; the download list goes to the owner for approval (decision 43) | done (2026-11-18): `TEXTURES.md`; the download list waits for the answer to Q10 |
| 3.16 | Test package (decision 43): archives — Orbiter of the base commit with D3D9Client, the plugin, settings, scenarios, textures (if the license allows); install instructions in two languages: what to unpack in which order and where | Windows x64 (Proxmox) | The archives unpacked by the instructions and started on a clean machine | done (2026-11-18): a start without graphics on a clean machine; with the picture not checked, see results of 3.16 |
| 3.17 | Test procedure in two languages (decision 44): roles, who presses what, what is checked, the expected result, a report form for the agent; a PDF of the procedure; the overview PDF — if there is something to update | Mac | A document in `docs/`, PDF | done (2026-11-18): `TEST-PROCEDURE.md` and PDFs, the package checked with graphics, see the results of 3.17 |
| 3.18 | Research 4.1 of stage 4 (decision 47): what the standard MFDs and the DeltaGlider change in the vessel, how it is read and set through the SDK; panel and cockpit clicks on the host for a vessel not in focus | Mac | A section in `ORBITER-INTERNALS.md`; a fork — a question to the owner | done (2026-11-18): section 19 of `ORBITER-INTERNALS.md`; fork Q11 postponed with 4B (decision 52) |
| 3.20 | Decision 52: stage 4 splits into 4A "everyone sees the vessel state" (not postponed) and 4B "crew" (postponed), Q11 postponed; the "mechanisms of someone else's vessel" limitation — into the procedure; rebuild the procedure PDFs | Mac | `DECISIONS.md`, `PLAN.md`, `TEST-PROCEDURE.md`, PDF | done (2026-11-19): decisions 52–53, the limitation in the procedure, the procedure PDFs and the overview report rebuilt |
| 3.19 | Research 5.6 of stage 5 (decision 47): Orbiter's recorder format (`FlightRecorder.cpp`) and whether a plugin can write it for all vessels | Mac | A section in `ORBITER-INTERNALS.md`; a fork — a question to the owner | done (2026-11-19): section 20 of `ORBITER-INTERNALS.md`; fork Q12 |
| 3.21 | 4A, research: general animation transfer (`GetAnimPtr`, `GetAnimation`, `SetAnimation`) versus DeltaGlider Lua commands — what another participant will see, for which classes it works; NAV and transponder frequencies | Mac | A section in `ORBITER-INTERNALS.md`; a fork — a question to the owner | done (2026-11-19): section 21 of `ORBITER-INTERNALS.md`; fork Q13 |
| 3.22 | 4A: protocol and implementation plan from the results of 3.21, tasks for the runs | Mac | A decision in `DECISIONS.md`, tasks in `PLAN.md` | done (2026-11-19): decision 58 (protocol version 7), the 4A task queue in the stage 4 section |
| 3.23 | Stage 5: the work queue — tasks 5.1–5.7, what the rigs can check now and what waits for people | Mac | Tasks in `PLAN.md` | done (2026-11-19): the stage 5 tasks got a "Status" column — the queue continues there |
| 3.24 | The big audit after handing over the round 1 package: the code, comments and all MD files are checked against the code; fix what is wrong, delete what code or a check cannot confirm; less but true (the owner's instruction of 2026-11-19) | Mac | Every MD file and comment checked against the code; results in `PLAN.md` and CHANGELOG | waiting (main session) |

**Results of task 3.0 (2026-11-18).** The wired Proxmox pair (host Windows 11, client Windows 10, without
graphics), the recorded DeltaGlider flight, 180 s per set, RTT 0.3–0.5 ms:

| Rate, Hz | Display delay, ms | Frames without data | Error p50 / p99 / max |
|---|---|---|---|
| 30 | 100 (the old default) | 0 | 1.1 mm / 11.8 mm / 0.044 m |
| 30 | 50 | 0 | 1.1 mm / 11.9 mm / 0.044 m |
| 60 | 30 | 0 | 0.7 mm / 3.0 mm / 0.012 m |
| 60 | 20 | 146 | 0.7 mm / 3.0 mm / 0.012 m |

Over a wire the buffer size does not affect accuracy as long as there is enough data; accuracy comes from
the rate. The new defaults are 60 Hz and 30 ms (decision 36): the own vessel will respond in about 30 ms
instead of 100.

**Results of 3.2–3.3 (2026-11-18).** The host is Windows 11 x64 without graphics, the client is
Windows 11 ARM with graphics (Parallels). Scenario: the pilot's DeltaGlider "Ref" in orbit. The `key`
step of `run-orbiter` held numpad "+" (full main engine) for 5 s. The client counted 300 frames with a
control key and sent `INPUT`; the host accepted about 2000 `INPUT` packets, 0 rejected, and applied
them. The host trace shows that the speed of "Ref" changed by about 26 m/s within those 5 s — that is
the full main engine of a DeltaGlider. Found on the way: a vessel in a scenario without a `PRPLEVEL`
line starts with empty tanks — this is why the first runs showed no acceleration. Test scenarios need
`PRPLEVEL`. The RCS keys, throttle, trim, wheel brakes and the joystick were not pressed on the rig,
only the full main engine: the rest is checked by tests and by code.

**Results of 3.4 (2026-11-18).** The same host (Windows 11 x64, no graphics) and client
(Windows 11 ARM with graphics); the DeltaGlider "Ref" belongs to the pilot. The `key` step pressed
`[` ("Prograde" autopilot): the host received `KEY`, passed it to the vessel with `SendBufferedKey`,
and the log shows mode 3 (`NAVMODE_PROGRADE`) on; the `G` key (gear) is reported as "taken by the
vessel", i.e. the DeltaGlider took the key itself. The host drops and counts `KEY` for a vessel the
sender does not own (core test). The press is carried out on the client too (decision 33, variant
A); the host reads the key layout from its own `keymap.cfg`, so host and client layouts must match.
A key that arrives during pause waits until the pause ends.

**Results of 3.5 (2026-11-18).** `OlcRigTest` has `dock` and `undock`. The host (Windows 11 x64) finds
links itself: every frame it looks at the ports (`GetDockStatus`) and sends `DOCK` on a change,
naming the link from the vessel with the smaller `net_id`; a client that joins later gets the
current links from `HostLink` right after `OWNERSHIP`. The client (Windows 11 ARM) carries out `DOCK`
after the display delay (30 ms): `Dock` in mode 0, `Undock`. While vessels are docked, the state is
imposed only on the vessel with the smaller `net_id` (position and velocity via `DefSetStateEx`,
orientation and rotation via `SetGlobalOrientation` and `SetAngularVel`); the others are left alone.
The run: two DeltaGliders 200 m apart, `dock` at 95 s, `undock` at 115 s: the client docks with
code 0 and undocks, and its screenshot shows the pair nose to nose. Jerks were not measured (task
3.6 and the check by people).

**Results of 3.6 (2026-11-18).** The client notes the time of pressing the full main engine key and
the time when thrust appears on the displayed vessel of the pilot (`OLC client: control latency` in
the log; the thrust is seen in the exhaust, and the velocity grows after it). The host is Windows 11
x64 on Proxmox, the client is Windows 11 ARM in Parallels (Wi-Fi and NAT, RTT 14–18 ms), so it is not
a wired pair. Five presses at 30 Hz and a 100 ms display delay: 131, 136, 150, 135 ms. With the
default values (60 Hz, 30 ms): 63, 67, 67, 148, 370 ms — about 65 ms in most cases, two outliers;
their source is the rig network (Wi-Fi and NAT), see the 3.10 results. The latency is made of the `INPUT`
path (up to 16 ms until the next send, about RTT/2), the host frame, the state interval (17 ms at
60 Hz) and the display delay. It was not measured on a wired LAN.

**Results of 3.9 (2026-11-18).** `-T` was checked six times: Windows 11 x64 without graphics — a
scenario without a recording and with a recorded flight, without the plugin, with a client connected
from Windows 10 x64; Windows 11 ARM — a host without graphics and a client with a picture on one machine
(as in 3.8). Each time Orbiter exited by itself, `-T` plus 4–9 s after the process start. From the
session start to the exit the wall clock showed 1.0–1.2 s more than `-T` (with `-T` of 20, 80 and 90 s),
the same on x64 and ARM: Orbiter's system time falls behind in the first frames after loading and does
not fall further behind later. Capping the frame step at 0.1 s (`Orbiter.cpp:1771`) matters only for
frames longer than 0.1 s, and a host without graphics runs thousands of frames per second. The likely
cause of the log "stopping" is the `run-orbiter.ps1` timeout: it counts from the start of the copies,
and with `-T` greater than the timeout minus the loading time it stops the process before its limit,
and the log ends without `Fast process shutdown`. The run log of the old runs was not kept, so this is
a conclusion, not a measurement. Fixed in OLC: `run-orbiter.ps1` writes to `run.log` after how many
seconds a copy exited by itself, and the job description has a rule for choosing the timeout. Found
along the way: on a loading error (for example, a scenario not found) Orbiter shows an error window and
waits for a click (`TerminateOnError`); when started from SSH the process hangs until stopped, and the
log ends with the line `>>> TERMINATING <<<`.

**Results of 3.10 (2026-11-18).** The control latency measurement is now split into steps and runs on
the wall clock (`plugin/wall_clock.h`): `oapiGetSysTime` counts a frame longer than 0.1 s as 0.1 s. The
client writes in `control latency` when the first `INPUT` with the key went out after the press, when
the first state of its own vessel with thrust arrived, and the longest frame; the host writes `input to
state` — from applying the key to sending the first state with thrust — and the longest frame in the
statistics line. The same rig as in 3.6: host Windows 11 x64 without graphics (Proxmox), client
Windows 11 ARM with a picture (Parallels, Wi-Fi and NAT), 60 Hz and a 30 ms display delay, 20 full
throttle presses in each of two runs.

| Step | Usually | In outliers |
|---|---|---|
| Press → `INPUT` went out (a client frame) | 11–19 ms (after the fix below) | 14–17 ms |
| Host: key applied → state with thrust went out (state interval) | 1–16 ms | 1–16 ms |
| `INPUT` went out → state with thrust arrived (network both ways and the host) | 13–51 ms | 32–365 ms |
| State arrived → thrust on screen (display delay and a frame) | 9–51 ms | 30–135 ms |
| Total | 60–99 ms, median of both runs 81 ms | 113–499 ms |

Host frames are not longer than 27 ms, client frames — 26 ms. The outliers (6 of 20 in the second run,
none in the first) coincide with jumps of the `PING` round trip time to 49–383 ms: their source is the
rig network (Wi-Fi on the Mac and Parallels NAT), not the host or client frames. On the wired Proxmox
pair the measurement was not made: keys cannot be pressed on machines without graphics. Found and
fixed along the way: the "at most 60 times per second" limit counted the next time from the current
frame, and at 60 frames per second the client sent `INPUT` every other frame — about 39 times per
second, the press went out after 31–34 ms in 5 of 20 cases. Now the next time counts from the planned
one (`nextSendTime` in `core/protocol.h`), `INPUT` goes out 60 times per second, the press — within one
client frame. The host times `WORLD_STATE` by the same rule: a host with graphics at 60 frames per
second will not lose half of the states.

**Results of 3.11 (2026-11-18).** Host Windows 11 x64 without graphics, client Windows 11 ARM with a
picture, the pilot's DeltaGlider "Ref" in orbit. The keys were pressed by `run-orbiter.ps1` (it now
handles combinations like Ctrl+Alt+U), the focus on the client was moved by `OlcRigTest` (the new
`focus` action: the F3 window is drawn by ImGui, a script cannot click in it). The host logs what the
pilot's keys did: at each change of the held keys — the RCS mode, the levels of RCS, main, retro and
hover engines, the trim, the wheel brakes and the rotation; the client logs its notices.

| Key | Result on the host |
|---|---|
| `/` (RCS mode) | The mode switched: rotation → translation → rotation |
| Numpad 2 in translation mode | RCS 1.00, no rotation — moving up |
| Numpad 4 in translation mode | Nothing, as in Orbiter: it has no translation |
| Numpad 2 and 6 in rotation mode | Pitch 12.5 °/s, bank 18.6 °/s |
| Ctrl + numpad 2 (low power RCS) | RCS 0.10, pitch 1.2 °/s |
| Numpad 5 (kill rotation) | Autopilot 1 (`KILLROT`) on, rotation 0 |
| Ctrl + numpad "+" (throttle) for 2 s | Main engine 0.40 (0.2 per second) |
| Numpad `*` | Main engine 0 |
| Numpad 0 and "." (hover) for 2 s each | 0.42, then 0.02 |
| Delete and Insert (trim) for 2 s each | 0.50, then 0.00 |
| "," and "." (wheel brakes) | Left 1.00, right 1.00 |
| Numpad "−" (retro) | `INPUT` with the key arrived, but the level is not visible: a DeltaGlider's retro engines have no fuel while the covers are closed (`EnableRetroThrusters`); not checked with the covers open |
| Ctrl+Alt+U | Host: "releases vessel 'Ref'", the client shows a notice about it |
| Focus on a free vessel after that | The client took `GL-01S` |
| Focus on another vessel while owning one | Notice "view only - your vessel is 'Ref'"; numpad 2 at that time did not reach `Ref` |
| Ctrl+F3 (back to one's own) | The focus came back: the next numpad 2 rotated `Ref` (6.3 °/s) |

Ctrl+F3 and the RCS keys also go to the host as `KEY` (decision 33, option A) and are passed to the
vessel; the DeltaGlider does not take them, no harm. Focus on a vessel flown by another pilot was not
checked: it needs a second client with a picture.

**Results of 3.13 (2026-11-18).** Subagents read all the code; the main session checked every
finding against the code and the Orbiter sources. Fixed:

- **A silent connection (core).** A frozen computer or a pulled cable left the TCP connection open:
  the pilot's slot and vessel stayed taken forever, and coming back under the same name was impossible.
  Now a side that hears nothing for 15 s (`kSilenceTimeout`) treats the other one as gone
  (`PROTOCOL.md`, section 3); test `testSilence`.
- **`HELLO` of another version** — now `REJECT` 1, not a dropped connection; **`TcpFrameReader::next`**
  no longer erases the buffer head on every frame (both in the previous run).
- **`WORLD_STATE` and `FUEL` for an unknown `net_id`** are dropped by the client, as `PROTOCOL.md`,
  section 1, promises, and counted in the log (`unknown vessels`).
- **`INPUT` out of range, NaN and infinity** — the packet is broken (`readInput`), a test in
  `test_messages`.
- **`ClientLink::start`** is called once: a second call returns `false` (the state used not to be
  reset; the plugin creates a new `ClientLink` for each connection anyway).
- **Docking on the client (plugin):** the links of a removed vessel are forgotten, so the partner does
  not freeze; a pair docked already in the loaded scenario is recognised as a bundle (`already docked`);
  undocking with a vessel already removed does not hold the `DOCK` queue.
- **`detectDocking`** gave four links for two links between one pair: now each port is taken once
  (the SDK does not tell which port mates which, so the port pair may come out crossed).
- **`;` in the player name** — Orbiter reads it in settings as the start of a comment; the character
  is removed on input.
- **`run-orbiter.ps1`:** the `instance` field sends `key` and `close` steps to the right Orbiter copy.
- **`kPluginVersion`** — "0.1-stage3"; the dead field `ServerClient::player` in `olc-netcheck` is removed.

Not bugs: a stream of `HELLO` with number 0 overwrites the same file (`Scenarios\OLC\Sent to
clients.scn`), the disk does not grow; the `input.throttle` branch is a protocol field for a throttle
lever that the client does not send yet (explained in the code); `launchedScenarioId_` after a launch
that did not happen stays until the next scenario start — a manual start of the same scenario has the
right number; a stale `DOCK` at the queue head waits 5 s so as not to break the "dock → undock" order.
Document mismatches (`ARCHITECTURE.md`, `STANDS.md`, `PROTOCOL.md`) move to audit 3.14.

Check: builds without warnings on Mac and MSVC x64, `ctest` 13 of 13 on both. On the rigs (host
Windows 11 x64 without graphics, client Windows 11 ARM with graphics, scenario `Demo\Docked at ISS`):
the client writes "docking vessels 0 and 4: already docked" — the pair docked in the scenario is recognised as a bundle; after the bundle leader ISS is deleted on the host (`OlcRigTest`, `100 delete ISS`), GL-02 on the client keeps moving until the end of the run (by `olc-trace.csv`) instead of freezing; `unknown vessels 0`.

**Results of 3.14 (2026-11-18).** Three subagents checked against the code the documents (except the
logs: `PLAN`, `DECISIONS`, `QUESTIONS`, `CHANGELOG`, `RESEARCH`, `ORBITER-INTERNALS`), the Russian
versions against the English ones and all code comments; the main session checked every mismatch
itself. The Russian and English versions agree in meaning everywhere. Fixed:

- `ARCHITECTURE.md`: the interface is described as it is (the "OLC - LAN co-op" item, `saveConfig`,
  messages to the pilot via `oapiCreateAnnotation`, the log); there is no ImGui window,
  `oapiAddNotification` or `clbkWriteConfig` in the code — the players window is marked as a plan
  (task 5.3); sim time — only a jump past the `TimeSyncThreshold` threshold; the state rate does not
  depend on warp (5 Hz while paused); the display delay is 30 ms by default; `KEY` and `DOCK` added to
  the `HostLink` and `ClientLink` rows.
- `PROTOCOL.md`: the host does not send `REJECT` 3 (the client notices another system); only the client
  sends `PING`; `OWNERSHIP` comes right after `VESSELS`, and the client answers with `CLASSES_MISSING`
  and `UDP_READY` without waiting for it; the sim time threshold is `TimeSyncThreshold`, not 1 s.
- `SMOOTHING.md`, `STANDS.md`, `SDK-CHEATSHEET.md`, `SECURITY.md`, `docs/README.md`: a 30 ms delay for
  a wire and 100 ms for Wi-Fi instead of a single 100 ms, stages in the past tense, vessel ownership
  already works, the plugin is at stage 3.
- `olc-netcheck` counts the share of RTT above 60 ms (wire) and 200 ms (Wi-Fi) instead of 100 and 200 ms.
- Code comments: a wrong decision number, "stage 1/2" where the behaviour changed, `DOCK` over TCP,
  `PONG` in the shared `seq` counter, the 30 ms delay, the `Vessel.cpp:909` line, the descriptions of the
  plugin file and the tests. The unused `ClientLink::stateRateHz` is removed; `RejectReason::OtherSystem`
  is marked as reserved.

Left as is: task labels in comments (references to the plan), the "minimum to start" in `STANDS.md`
(advice for a new rig), `ClientClock::jumps` and `HostLink::port` — the tests check by them.
Check: builds without warnings on Mac and MSVC x64, `ctest` 13 of 13 on both, `olc-doccheck` passes.

**Results of 3.15 (2026-11-18).** The research is in `TEXTURES.md`. The main points: Orbiter 2024
(373 MB) and the SPICE kernels come from GitHub and `cdn.openorbiter.space`; they hold no Earth
textures, the base textures come from an Orbiter 2016 installation; the high-resolution packs (Earth
32.83 GB) are not needed for orbital docking. No license is stated for the texture packs and the forum
pages are closed to the agent (403), so they must not go into the archive until the owner confirms.
The download list is question Q10 (I recommend option A: only our files and Orbiter with D3D9Client
in the archive, the rest by links).

**Results of 3.16 (2026-11-18).** The `tools/package/make-package.ps1` script builds three archives from
a ready installation: Orbiter with D3D9Client (about 160 MB), the SPICE kernels (about 1.3 GB), the plugin
with a settings template and the scenarios. Earth textures come from a link, decision 50. The
instructions are in `PACKAGE.md`. Check: the archives were built on Windows 11 x64 and unpacked by the
instructions into an empty folder on Windows 10 x64; a start without graphics worked
(`OLC host: listening`, exit 0). The Proxmox rigs cannot check a start with the picture on a clean
machine (no 3D) - that is left to people in task 3.7.

**Results of 3.17 (2026-11-18).** The test procedure — [TEST-PROCEDURE.md](TEST-PROCEDURE.md) (the first version; never handed over, replaced on 2026-11-19 by the round 1 procedure for plugin `0.2`):
roles (flight director, pilots 1 and 2), preparation, tests T1–T9 with keys and the expected outcome,
what to save after the lesson, the report form and common problems. The keys are checked against the
Orbiter key map (`Keymap.cpp`) and the DeltaGlider (`K` — nose port, `G` — gear), the on-screen texts
against the plugin code. A known limitation is recorded: the DeltaGlider retro doors open from the
panel, whose clicks the client does not pass yet (stage 4). The procedure and installation PDFs in both
languages are built outside the repository together with the archives.

The package (3.16) is improved: the plugin archive gets a starting `Orbiter.cfg` (D3D9Client and the
plugin on, vessel collisions off), build leftovers (`CMakeLists.txt`, `*.lib`) are removed from the
Orbiter archive. `run-orbiter.ps1` starts a copy without switches (`args` may be left out). The check
with graphics on a clean installation (Windows 11 ARM) — `PACKAGE.md`, section 4. The overview PDF was
not updated: what is new for it — the audits and the package — goes into the report after the tests.

**Stage 3 succeeds if:**

- a pilot flies their own vessel from the client: RCS, main engine, autopilots, gear;
- two vessels, each under its own player, rendezvous and dock in orbit; both see the stack the same way;
- switching the focus does not let anyone take over someone else's vessel;
- when the connection breaks, the pilot's vessel does not keep flying with a key held down.

**Owner's decisions (2026-11-18, decisions 32–35):**

1. **Ownership — A2:** a vessel stays with the pilot until the pilot releases it or leaves; switching the
   focus (F3) does not take the vessel away. Take — by moving the
   focus to a free vessel when the pilot has none yet; release — with a key or by leaving the session.
   Someone else's vessel in focus is view only, with an on-screen notice. Connection lost — the vessel
   stays with the pilot for 60 s, the controls released.
2. **The target environment is a wired LAN** through one switch (decision 32). The delay of the own vessel
   is set by the smoothing buffer; its default for a LAN is chosen by a measurement (task 3.0). Prediction
   and extrapolation are not part of stage 3.
3. **Animations — option A:** a one-time key press is executed both on the host and on the client. Later —
   reconnaissance of sending animations in a general way (`GetAnimation`/`SetAnimation`).
4. **The check by people — step by step:** three docking scenarios and a memo for pilots (task 3.7).
5. The OLC interface is in English for now, language choice comes later.

**What was found about focus and control in Orbiter** (sources, 2026-11-18):

- In single play the focus is the control: all standard control goes only to the vessel in focus
  (`Orbiter::UserInput`), F3 moves the focus to any vessel that allows it (`DlgFocus`). Looking at another
  vessel without changing the focus is possible with the camera (the "Camera" dialog). Orbiter has no
  "view without control" — OLC adds it.
- Keyboard: if a plugin "eats" the keys entirely (`clbkProcessKeyboardImmediate` returns `true`), the core
  skips for this frame camera rotation as well, not only vessel control. So on the client only the vessel
  control keys must be cleared: the key array is passed to the plugin itself, not as a copy, and single keys
  can be zeroed in it.
- The SDK does not give the key layout, but the core reads it from `keymap.cfg` in the Orbiter folder — the
  plugin reads this file itself. This is needed in any option: `INPUT` sends actions, not key codes.
- The core applies the joystick to the vessel in focus past the plugins; it cannot be suppressed. The harm
  is small: the vessel's state is imposed every frame.
- The option "take and release only with a key" (F3 everywhere is viewing) does not argue with Orbiter's
  code: the mechanics are the same (clear the controls if the vessel is not one's own), the difference is
  one key and an on-screen hint. It can be made a setting on top of the default option.
  The owner chose the default option (2026-11-18), the key-only one is a possible setting later.

Not part of stage 3: a crew in one vessel (stage 4), clicks in the cockpit and on panels, add-ons' own
held keys (the SDK does not allow it), sending animations in a general way.

## Stages 4 and 5: detailed plan (approved by the owner on 2026-11-18)

Written in task 3.12 (2026-11-18) from the Orbiter sources and the results of stages 2–3. The order
of the stages, the way to recognise the flight director and the navigator's rights are question Q8,
decision 46. Stage 5 is proposed first: it is cheaper, needs no research of
cockpit clicks and is needed already in the club's lessons (shared pause, saves).

### Stage 5. Flight director

What there is now: pause and warp come only from the host, a client returns its own to the host's
within 0.5–1 s; on a server without graphics nobody can press them except through the console
(`console-send.ps1`). Watching another vessel is possible (F3 — "view only"), but the first focus on
a free vessel takes it.

**How it will work:**

1. The flight director (FD) is a client with a picture on the server machine (decision 34) or on
   any other one. The host recognises the FD by the player name from `HELLO` and a server setting
   (question Q8, item 2).
2. Pause and warp: for the FD they are not reverted but go to the host as a request; the host
   applies them, and all clients follow as now. For pilots — as now.
3. The FD owns no vessel: focus on any vessel is view only, control keys are cleared.
4. The FD's screen shows a summary: players, who flies which vessel, latency and losses of each.
5. Saving: on the FD's key and automatically every few minutes the host saves the world
   (`oapiSaveScenario`) to `Scenarios\OLC\Saves\`; a new lesson can start from a save.
6. Debriefing: Orbiter's flight recorder is switched on only by a key or a dialog
   (`Orbiter::ToggleRecorder`, `Orbiter.cpp:1550`), the SDK has no function for it. How to record a
   flight on a server without graphics is research 5.6: write files in the recorder format
   ourselves (then the debriefing is the standard playback) or an OLC log of our own.

**Task queue:**

| No. | Task | Where | Done when | Status |
|---|---|---|---|---|
| 5.1 | Protocol v5: a pause and warp request from a client (`TIME_REQUEST`), an FD flag in `WELCOME`, a player summary (`PLAYERS`); a decision in `DECISIONS.md`, `PROTOCOL.md` | Mac, core | Parsing and exchange tests on loopback; a request not from the FD is refused | done (2026-11-19): protocol v5 in the core, loopback tests, `PROTOCOL.md` and `ARCHITECTURE.md` updated |
| 5.2 | FD client: pause and warp go to the host; the FD has no vessel, focus is view only | Windows ARM | On the rig the FD sets pause and ×10 — the pilot has them within 0.5 s | done (2026-11-19): pause (Ctrl+P) and warp x10, x100 from the FD reach the host and stay; warp near vessels works only with `VesselCollision = FALSE` (section 22 of `ORBITER-INTERNALS.md`); rig .163 + olc-win-arm, the FD client sim time error grows with the warp, no bounce back to x1 |
| 5.3 | Player summary on the FD's screen (an annotation or an ImGui window) | Windows ARM | The FD sees the players, vessels, latency | done (2026-11-19): an `oapiCreateAnnotation` text on the left of the FD's screen, refreshed from `PLAYERS`; on the .163 + olc-win-arm rig the host and the FD show with latency (18 ms); `PLAYERS` has no packet loss, only latency |
| 5.4 | Saves: on the FD's key and every `AutosaveMinutes` minutes | Mac (core) + Windows | The file appears, a new session starts from it | done (2026-11-19): protocol v6, `SAVE_REQUEST`, Ctrl+Alt+S for the FD, `AutosaveMinutes` on the host (decision 55); on the rig .163 + olc-win-arm `save-*.scn` and `autosave.scn` appeared, Orbiter starts from a save |
| 5.5 | Server without graphics: pause and warp from the server console in the description (`console-send.ps1`) as a fallback | Mac | Description in `QUICKSTART.md` | done (2026-11-19): description in `QUICKSTART.md`, section 6; commands checked against the script, not run on a rig |
| 5.6 | Research: Orbiter's recorder format (`FlightRecorder.cpp`) and whether a plugin can write it for all vessels | Mac | A section in `ORBITER-INTERNALS.md`; a fork — a question to the owner | done (2026-11-19): task 3.19, `ORBITER-INTERNALS.md`, section 20 |
| 5.7 | Recording a lesson and debriefing by the results of 5.6 | Mac + Windows | The flight can be watched again after the lesson | done (2026-11-19): Ctrl+C recording by the FD described in QUICKSTART (section 6) and PILOT-GUIDE (section 5), playback checked on the rig on 2026-11-19 (section 20 of ORBITER-INTERNALS) |
| 5.8 | Check on the rigs: a server without graphics, the FD on the same machine, a pilot on another | Rigs | Results by the criteria below | done (2026-11-19): a server without graphics (.163), the FD with graphics (olc-win-arm), a pilot without graphics (.162, takes `GL-01S`); Ctrl+P at the FD: pause and its release reach the pilot within 0.3 s, the player summary on the FD screen shows the pilot and the FD with latency; the FD has `input sent 0` (flies no vessel); autosave once a minute (`AutosaveMinutes = 1`) written. Not checked: the FD on the same machine as the host (checked 2026-11-18, task 3.8), the pilot's pause being refused (core tests), flight replay (5.7, Q12) |
| 5.9 | Leftovers of 5.2 from the main session check: (1) x10 from the FD reaches the pilot on the rig; (2) observer mode (`HELLO` with the observer flag, F3 is view only); (3) `restoreHeldVessels`: an observer or FD with the name of a pilot who left must not get that vessel — check the core code, fix it and add a test if wrong; (4) an FD section in PILOT-GUIDE: pause, warp (only with `VesselCollision = FALSE`), save, summary | Mac + rigs | x10 at the pilot within 0.5 s; the observer takes no vessel; a test for the held vessel | done (2026-11-19): (1) on the rig (host .163 without graphics, FD on olc-win-arm, pilot .162) `T` at the FD — x10, `R` — x1 reach the host and the pilot, at the pilot about 0.3 s later (from the process start times, accuracy — tenths of a second); (2) in the plugin the observer is only the FD (there is no separate observer setting): F3 is view only, `input sent 0` (5.8); the observer flag is checked by core tests; (3) the bug is confirmed and fixed: an observer or FD under the name of a pilot who left got that vessel — not any more, a test in `testOwnership`; (4) section 5 of `PILOT-GUIDE.md` for the FD: preparation, keys, saves, summary |

**Stage 5 succeeds if:**

- the FD sets and clears the shared pause and changes warp at their own machine — everyone gets it within 0.5 s;
- pilots do not change pause and warp themselves;
- the FD watches any vessel and does not take over control;
- the FD sees who flies which vessel and each pilot's connection;
- saving on command and autosave work, a lesson starts from a save;
- after the lesson the flight can be watched again.

### Stage 4. Crew: pilot and navigator in one vessel

**Split (decision 52):** 4A "Everyone sees the vessel state" is in progress (tasks 3.21–3.22); of the
tasks below it includes 4.4 (NAV frequencies) and the general transfer of vessel mechanisms; 4B "Crew"
(4.2, 4.3, 4.5, 4.6) is postponed.

**4A task queue** (decisions 57 and 58; tasks 4.1 and 4.4 below are covered by it):

| No. | Task | Where | Done when | Status |
|---|---|---|---|---|
| 4A.1 | Protocol version 7 in the core: `ANIM_STATE` and `NAV_STATE` (decision 58) — reading and writing, queues in `HostLink` and `ClientLink`, `NAV_STATE` accepted only from the vessel owner, the host keeps the latest values and sends a snapshot to a new client after `VESSELS`; `PROTOCOL.md` | Mac, core | Core tests: message parsing, someone else's vessel rejected, a new client gets the snapshot; a build without warnings | done (2026-11-19): protocol version 7 in the core, core tests pass (messages, a foreign vessel is refused, snapshot for a new client, a removed vessel is forgotten), build without warnings. |
| 4A.2 | Plugin, animations: the host reads the animations of all vessels (`GetAnimPtr`), threshold 0.01, at most 5 times a second per vessel; the client applies `SetAnimation`. Measure on the rig how many DeltaGlider animations change by themselves every frame and how many bytes a second go out; if many, a filter (decision 58) | Windows | On the rig the pilot on a client presses `G` and `K` — on another client the gear and the cone move; the traffic is in the results | done (2026-11-19): the host reads animations (`GetAnimPtr`, threshold 0.01, 5 scans per second per vessel, a stop at 0 or 1 is always sent), the client sets `SetAnimation`, values for a vessel not created yet wait. Rig: headless host .163, a pilot with graphics on olc-win-arm, a headless observer on .162, scenario `Two DG 20 m`, `G`/`K` pressed by `run-orbiter.ps1`. A DeltaGlider has 28 animations; they do not change by themselves (0 messages in 40 s of rest), the first transfer is 56 values (354 bytes) for two vessels, each `G` or `K` press is 25-50 values (0.5-1 KB) while the mechanism moves; 331 values in 130 s, about 4.2 KB. The observer applied all 331 (0 rejected), its count grows with each press. No filter needed. The wheels on the observer's screen were not looked at by eye (check in 4A.4). MSVC x64 build without errors, tests pass |
| 4A.3 | Plugin, frequencies: the owner reads the NAV and transponder steps every 0.5 s and sends `NAV_STATE` on change; the host applies them (`SetNavChannel`, `SetTransponderChannel`) and sends them on; the client applies them | Windows | On the rig a frequency the pilot set in an MFD is seen in the observer's MFD | done (2026-11-19): the owner checks the NAV and transponder steps of its vessel every 0.5 s and sends `NAV_STATE` on change (the host does it for the vessel it flies itself); the host sets the pilot's frequencies on its copy and sends them on; a client sets the others' (waiting for a vessel not created yet). Rig: headless host .163, a pilot on .162 (`OlcRigTest`: `nav`, `xpdr`, `shownav`), a second client on olc-win-arm: the pilot set NAV 123 and 245 and the transponder 77 - the host shows the same steps, a client that joined later gets them right after joining. Live delivery to a client that is already connected is covered by a core test; the frequencies in the observer's MFD window were not looked at by eye (4A.4). MSVC x64 build without warnings, tests pass |
| 4A.4 | Rig check of 4A (host without graphics, two clients with graphics) and documents: remove the "mechanisms of someone else's vessel" limitation from `TEST-PROCEDURE.md` for the next round, `PILOT-GUIDE.md`, `ARCHITECTURE.md`, CHANGELOG | Rigs, Mac | The 4A criteria below are met on the rig | done (2026-11-19): on the rig (headless host .163, a pilot and an observer with graphics on olc-win-arm, DeltaGlider `GL-01`) check by the main session (pilot `pilot1` and a second client `obs1` with graphics on olc-win-arm, host .163): before `G` neither the pilot nor the second client shows wheels, after `G` the gear of `GL-01` is down on the screenshots of both; 194 values applied, 0 rejected. The cone cannot be told apart from the camera angle (from behind) — checked by the counters only; the earlier conclusion "the cone is open on the screenshot" is wrong: the same shape is seen before `K`. A new participant gets the mechanisms and frequencies right after joining (4A.2, 4A.3). The 0.5 s limit is met by design: a scan every 0.2 s plus one LAN message, the delay was not measured separately. The "mechanisms of someone else's vessel" limitation is removed from `TEST-PROCEDURE.md`; ARCHITECTURE and CHANGELOG updated. Not checked by eye: the cone and the frequencies in the observer's MFD window (this goes into the procedure for people); wheels on the ground were not checked (the vessels are in orbit) |

DeltaGlider Lua commands (option A of question Q13) — only if the rig or people show that the general
transfer is not enough (decision 57); they are not queued.

**4A succeeded if:** the gear, the cone, the airlock and other mechanisms of the pilot's vessel are in
the same position for all participants as for the pilot, within 0.5 s; the pilot's NAV frequencies are
seen in the observer's MFD; a new participant sees the mechanisms and frequencies right after connecting.

**What is known from the sources:**

- MFDs are computed in each Orbiter from its own world. The navigator has the pilot's vessel in
  focus, and their MFDs work with the imposed state: the MFDs themselves need not be sent (section 7
  of `ORBITER-INTERNALS.md`).
- What MFDs change in the vessel itself, for example NAV receiver frequencies (`VESSEL::SetNavRecv`,
  `VesselAPI.h:5343`), stays on the navigator's client and does not reach the host and the pilot.
- Clicks in the cockpit and on panels are callbacks of the vessel module (`clbkPanelMouseEvent`,
  `clbkVCMouseEvent`, `VesselAPI.h:5960, 6042`); they work for the focus vessel with a loaded panel.
  Whether the host can apply them to a vessel not in focus is unknown — this is research 4.1.

**How it will work:**

1. The crew is a second client in the pilot's vessel: focus on the pilot's vessel and a "join the
   crew" key. There is one owner (the pilot), the number of crew seats is a server setting, one by
   default.
2. The navigator: own MFDs and camera; held control keys are cleared for them — the pilot flies.
   The navigator's single key presses (autopilots, RCS mode) — by the answer to question Q8, item 3.
3. NAV frequencies and whatever research 4.1 finds the navigator changes locally, the client sends
   the change to the host, the host applies it and sends it out.
4. The pilot and the navigator swap roles with a key, without leaving the session.

**Task queue:**

| No. | Task | Where | Done when |
|---|---|---|---|
| 4.1 | Research: what the standard MFDs and the DeltaGlider change in the vessel and how it is read and set through the SDK; panel and cockpit clicks on the host for a vessel not in focus | Mac | A section in `ORBITER-INTERNALS.md`; a fork — a question to the owner |
| 4.2 | Protocol: the crew in the session table (owner and crew members), `OWNER_REQUEST` "join" and "leave", the crew in `OWNERSHIP` | Mac, core | Core tests: joining, leaving, the pilot leaving, the seat limit |
| 4.3 | Navigator client: joining the crew, control cleared, single key presses by the owner's decision | Windows | On the rig the navigator is in the pilot's vessel, no takeover |
| 4.4 | Syncing NAV frequencies and whatever 4.1 finds | Mac (core) + Windows | A frequency set by the navigator is seen by the pilot |
| 4.5 | Swapping roles pilot ↔ navigator | Mac (core) + Windows | Roles swap without leaving the session |
| 4.6 | Check on the rigs and by people with a test package (decision 43) | Rigs, people | Results by the criteria below |

**Stage 4 succeeds if:**

- two in one vessel: the pilot flies, the navigator works with MFDs on their own screen;
- NAV frequencies set by the navigator are seen by the pilot and the host;
- the navigator cannot take over control by accident;
- the pilot and the navigator swap roles without leaving the session.

## Test rigs

Which systems are suitable, machine roles, the minimal set and preparation — [STANDS.md](STANDS.md).
Everyone describes their own machines in `stands.yaml` (not in git).

| Stage | Which machines are needed |
|---|---|
| 0 and the start of 1 | One Windows machine: build, host and client as two instances |
| The whole of 1 | Plus a second machine on the same network; at least one is Windows x64 without emulation |
| 2 | The same |
| 3 | Up to 4 machines; Linux clients (Wine, Proton); live tests on other people's Windows machines |
