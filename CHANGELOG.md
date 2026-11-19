**English** · [Русский](CHANGELOG.ru.md)

# CHANGELOG

## v0.2 — Test package, round 2 (2026-11-19)

Plugin `0.2`, protocol version 7. Added to round 1 (stage 3, plugin `0.1-stage3`): the flight
director mode — shared pause and time acceleration, the player summary, saves, flight recording
(stage 5); all participants see the vessel mechanisms and NAV frequencies (stage 4A). Plugins of
different versions do not connect to each other. Details — the task entries below.

## v0.0.1 — Research and documentation (2026-11-05)

### What is there

- **Research:** how Orbiter works, past multiplayer attempts, Linux and macOS,
  licenses, the state of releases — `docs/RESEARCH.md`.
- **Architecture (design):** an authoritative host and terminal clients over a LAN — `docs/ARCHITECTURE.md`.
- **Map of the Orbiter core:** the frame loop, time, vessel state, input, modules, recording
  playback, collisions — `docs/ORBITER-INTERNALS.md`.
- **Work plan** by stages with prototype criteria — `docs/PLAN.md`.
- **Developer environment:** preparing Windows and building Orbiter from source — `docs/QUICKSTART.md`.
- **Answers to stage 1 questions from the sources:** imposing state, vessels on the
  surface, scenarios and creating vessels, docking, input and joystick — `docs/ORBITER-INTERNALS.md`.
- **SDK cheat sheet:** which function to call for a task and what the catch is — `docs/SDK-CHEATSHEET.md`.
- **Network protocol draft** — `docs/PROTOCOL.md`.
- **Working rules:** the decision log `docs/DECISIONS.md`, questions to the owner
  `docs/QUESTIONS.md`, the AI agent's autonomous work rules in `docs/README.md`.
- **Smoothing model** `tools/smoothing/` (`olc-smoothing`): reference motion (orbit,
  engine and RCS pulses), packets with delay, jitter and loss, four ways of
  reconstruction on the client and error metrics. Build: `cmake -S . -B orbiter-lan-coop.out`.
- **Network core `olc-core`** (`core/`, a start): protocol constants and comparison of
  UDP sequence numbers with wrap; writing and reading protocol fields (`u8`…`f64`, `str`,
  little-endian) with bounds checks — `core/serialize.h`; the UDP header and length-prefixed TCP frames —
  `core/header.h`; type numbers and packing and parsing of all TCP and UDP messages, including
  `WORLD_STATE`, `VESSEL_ADDED`, `INPUT`, `PING`/`PONG`, `FUEL` and `SCENARIO`, — `core/messages.h`; CTest tests without
  third-party libraries (`tests/`), including truncated and broken data; the client
  interpolation buffer (Hermite over position and velocity, slerp for attitude) — `core/interpolation.h`, conversion of
  Orbiter angles to quaternions — `core/attitude.h`; the session table (players and their limit,
  `net_id`, vessel ownership) — `core/session_table.h`; a non-blocking wrapper over
  TCP and UDP sockets for Winsock and POSIX with a loopback test — `core/socket.h`.
  Code layout — `docs/ARCHITECTURE.md`, section 6.
- **Smoothing model results** — `docs/SMOOTHING.md`: a grid run (`tools/smoothing/run-grid.sh`,
  2025 runs). Recommendation: `WORLD_STATE` at 30 Hz, a 100 ms buffer delay, Hermite interpolation;
  the whole client world lags. Fixed in the model: host sending no longer coincides with the boundaries
  of engine pulses, the random number generator mixes the seed, frames before the first packet
  are not part of the metrics.
- **Check against Orbiter:** the procedure of the daily upstream check and the log — `docs/ORBITER-INTERNALS.md`,
  section 13; the base commit moved to `a7a3396e` (the mesh collision fix — sections 5,
  11, 17 of the map; on-screen labels through ImGui — section 16 and the cheat sheet; line references
  updated; the distance format in info panels — no effect on OLC).
- **Test rig layout** — `docs/STANDS.md`: supported systems, machine roles, the minimal set,
  SSH access, who does what, preparation by machine type. Your own machines are described in
  `stands.yaml` (never gets into git, example — `stands.example.yaml`; decisions 26 and 27). The script preparing
  a Windows machine for building — `tools/stands/setup-windows.ps1`.
- **Core research without running Orbiter** — `docs/ORBITER-INTERNALS.md`, sections 14–17:
  callbacks while paused and with an inactive window, threads and DLL life; two Orbiter instances on
  one machine; a Launchpad item and ImGui windows; vessel state beyond motion (fuel,
  thrust, animations, lights). An interface sketch and work while paused — `docs/ARCHITECTURE.md`;
  questions Q4–Q6 — `docs/QUESTIONS.md` (questions are numbered Q so as not to confuse them with plan tasks).
- **Agent limit rules** (decision 22): window up to 90%, the week evenly, a wake-up every hour.
- **Plugin skeleton** `OrbiterLanCoop` (`plugin/`): loads into Orbiter, writes the start and
  end of a session to the log, builds together with `olc-core`. **GitHub Actions build**
  (`.github/workflows/build.yml`): the Orbiter core from the base commit with a cache, the OLC core with tests
  on Windows and the plugin as an artifact (decision 25). How to build by hand — `docs/QUICKSTART.md`, section 5.
- **Test rigs connected** (stage 0 started): the network core builds without warnings and passes
  the tests on Linux (GCC 15, x86 and ARM). The **`olc-netcheck`** utility (`tools/netcheck/`):
  a TCP handshake and UDP `PING`/`PONG` between two machines, loss, delay percentiles;
  the first measurements over Wi-Fi and through the Parallels NAT — `docs/STANDS.md`, section 10.
- **Stage 0 on Windows 11 ARM (partly):** Orbiter built from the base commit without
  the graphics client (the DirectX SDK did not install — error S1023, a workaround added to
  `tools/stands/setup-windows.ps1`), SPICE kernels in place. The OLC plugin built with MSVC without
  warnings, the tests pass, the plugin loads into Orbiter and writes the start and end of a
  session; two Orbiter copies run at the same time. Orbiter without graphics runs right over
  SSH; for the visible desktop — a scheduled task and `tools/stands/run-orbiter.ps1`
  with screenshots. What was found while building — in `docs/QUICKSTART.md`.
- **Stage 1, code:** in the core `HostLink` (the host network side: handshake, sending `WORLD_STATE` and
  `FUEL` split into packets, `LANDED`), `ClientLink` (the client network side: handshake, interpolation
  buffers, `PING`) and `ClientClock` (the display clock) with loopback tests. The plugin: host
  and client roles, settings from `Config\OrbiterLanCoop.cfg`, pause and warp as on the host, the client
  time near display time, fuel, landing, states via `DefSetStateEx`, work
  while paused on a hidden window timer, a vessel trace in CSV. Checked with two Orbiter copies without
  graphics: the position error in orbit is millimetres, pause and warp up to ×100 arrive.
  Results — `docs/PLAN.md`, stage 1; running — `docs/QUICKSTART.md`, section 6.
- **Stage 1, recorded flight:** the client repeats the DeltaGlider takeoff, flight and landing with
  millimetre accuracy (up to 0.4 m at the runway). The landing on the client is now applied when
  display time reaches it, not at once on `LANDED`. Duplicate lines in the client log removed.
- **Stage 0 done, stage 1 with graphics:** a full Orbiter with D3D9Client built on Windows 11 ARM.
  Two copies with graphics side by side on screen: the client shows the recorded
  DeltaGlider flight together with the engine exhaust, position error p99 11 mm, the client runs at about
  60 frames per second. `run-orbiter.ps1` can place the copies' windows (the `window` field).
  Findings — in `docs/QUICKSTART.md` (Earth textures, window size, recording folder name).
- **Stage 1 code review:** a client behind NAT (for example, Windows in the Parallels shared network) could
  miss `WORLD_STATE` — the host sent it to the port from `UDP_READY`, while behind NAT another one is visible.
  Now `PING` carries the client number, and the host sends to where the `PING` came from (protocol version 2,
  decision 28, a "client behind NAT" test). The client clock is adjusted once per state, not per
  packet, when a state arrives in several packets.
- **Stage 2 reconnaissance:** the plugin can start a scenario sent by the host in the same Orbiter
  process — close the session, put the scenario in place, select it in Launchpad and press "Launch"
  itself. Checked on a test rig; restarting the process with `-s` remains a fallback.
  Details — `docs/ORBITER-INTERNALS.md`, section 18.
- **A bilingual project** (decision 29): all documentation has complete versions in Russian
  (the source: `README.ru.md`, `CHANGELOG.ru.md`, `docs/ru/`) and English (`README.md`,
  `CHANGELOG.md`, `docs/`). Comments in code, scripts and example settings are in
  English. The `olc-doccheck` check (`tools/doccheck/`) in `ctest` and CI compares the structure
  of every document pair: headings, tables, code blocks.
- **Stage 1 over the network between machines:** a host without graphics on Windows 11 x64 (Proxmox), clients —
  Windows 11 ARM with graphics over Wi-Fi and the Parallels NAT, and Windows 10 x64 over a wire.
  The recorded DeltaGlider flight: error p99 7 mm and 5 mm, maximum 0.17 m at the moment of
  touchdown, no losses; the fix for a client behind NAT worked on a real network. Results —
  `docs/PLAN.md`, stage 1. Test rigs: four machines on Proxmox (Windows 10, Windows 11, Ubuntu,
  Debian), the auto-logon script `tools/stands/setup-autologon.ps1`; `setup-windows.ps1` now
  turns on .NET Framework 3.5 for the DirectX SDK installer and downloads the SDK without
  `Invoke-WebRequest`. The core tests also pass on GCC 14 (Debian).
- **Stage 2 started:** the detailed plan is approved, the scenario is started through Launchpad,
  restarting with `-s` is the fallback (decision 30). Protocol version 3: `HELLO` and `SCENARIO`
  carry the scenario save number `scenario_id` (decision 31), the host logs it when a client joins.
  The host in the core answers a scenario request (`scenario_id` = 0) with the file of its world through
  `HostConfig::makeScenario` and closes the connection once the file has gone out; the client receives
  the file (`ClientLink::scenario`). A new refusal reason 5 — the host cannot give a scenario.
  The host sends vessels created and deleted during the session (`VESSEL_ADDED` with the state,
  `VESSEL_REMOVED`); the client in the core hands these changes to the plugin (`takeVesselChanges`).
  The client plugin keeps the same list of vessels as the host: creates missing ones if the class
  is installed, deletes extra ones and those the host deleted, leaves the last vessel alone; a class
  from `VESSEL_ADDED` that is not installed goes to the host in `CLASSES_MISSING`. Checked on rigs
  (host Windows 11, client Windows 10, without graphics) with the new helper plugin
  `tools/rigtest/` (`OlcRigTest.dll`) that creates and deletes vessels on a schedule.
  The "OLC - LAN co-op" item in Launchpad (the "Extra" tab): role, address, port, name and the
  "Connect" button — the client gets the host's world, writes it to `Scenarios\OLC\Host world.scn`,
  starts it itself and then joins the session with the scenario number. Checked with graphics (client
  Windows 11 ARM, host Windows 11 x64): 21 s from the button to the session. `run-orbiter.ps1` can press
  buttons in windows (the `ui` field of the job).
  Check 2.7: 10 connections in a row without restarting Orbiter, the "no places" refusal is seen in the OLC
  window. Vessels of classes the client lacks are removed from the received scenario before start
  (`core/scenario_filter.h`, otherwise Orbiter terminates abnormally), the list of such classes stays on
  screen for 20 s. Switching with F3 remains to be looked at by eye.
- **Stage 3 started:** decisions 32–36 — the target environment is a wired LAN, the terms "pilot" and
  "flight director", server modes, the stage 3 choices. The defaults for a LAN are changed by a
  measurement: 60 states per second and a 30 ms display delay (was 30 and 100 ms) — half the error over a
  wire, the own vessel will respond faster.
- **Task 3.1, core:** vessel ownership. Protocol version 4: `OWNER_REQUEST` (take or release) and
  `OWNER_DENIED` (a refusal with a reason). One client — one vessel, someone else's vessel and the
  host's vessel cannot be taken; a pilot who dropped out keeps the vessel for 60 s and gets it back by
  returning under the same name; the host sends ownership changes to everyone in `OWNERSHIP`.
  `ClientLink::requestTake`, `requestRelease`, `takeOwnerDenials`. Tests: the table, messages, two
  clients over loopback. The plugin: a host without graphics owns no vessels, the host-player and the pilots
  take a free vessel by focusing it (F3), Ctrl+Alt+U releases it; someone else's vessel in focus is view only,
  refusals and ownership changes are shown on screen. Checked on the rigs: a pilot took a vessel at join,
  the second pilot sees "view only". The agent's limit and run rules — decision 40.
- **Control latency (stage 3.6):** the client logs the time from pressing the full throttle key to thrust on the displayed vessel (`control latency`). On the stand (Wi-Fi and NAT): about 65 ms at 60 Hz and a 30 ms display delay, about 135 ms at 30 Hz and 100 ms.
- **A server without graphics and a client on one machine (stage 3.8):** checked on the stand, described in `docs/QUICKSTART.md`, section 6.
- **Check by people (stage 3.7, preparation):** three scenarios in `tools/scenarios/` (a station, two DeltaGliders 20 m and 3 km apart) and a pilot guide `docs/PILOT-GUIDE.md`; the host loading the scenarios was checked on the stand, the check itself is for people.
- **Docking (stage 3.5):** the host notices docking and undocking itself (vessel ports, `GetDockStatus`) and sends `DOCK`; a client that joins later gets the current links. The client repeats `Dock` in mode 0 and `Undock` after the display delay, and while vessels are docked flies the stack through the vessel with the smaller `net_id`. `OlcRigTest` got the `dock` and `undock` actions. On the stand the client repeated both docking and undocking of two DeltaGliders.
- **Single key presses (stage 3.4):** the client sends `KEY` over TCP when a key is pressed while its vessel is in focus (the key works on the client too); the host accepts `KEY` only for the sender's own vessel (`HostLink::takeKeys`) and passes it to the vessel with `SendBufferedKey`, with the modifiers. On the stand: "Prograde" (`[`) switched the autopilot mode on at the host, the gear key `G` was taken by the DeltaGlider. The key layout on the host is its own `keymap.cfg`.
- **Input (stages 3.2–3.3):** the client sends `INPUT` with the keys and the joystick of its own vessel (layout from `keymap.cfg`, at most 60 per second) and clears the control keys locally; the host applies `INPUT` to the pilot's vessel as Orbiter's core does (RCS, main and retro engines, throttle, hover, trim, wheel brakes); with no `INPUT` for 0.5 s the controls are released. On the rig (host Windows 11 x64 without graphics, client Windows 11 ARM with graphics) numpad "+" for 5 s: the client counted 300 frames with the key, the host accepted about 2000 `INPUT`, 0 rejected, the speed of "Ref" grew by about 26 m/s. The other keys and the joystick are checked by tests and by code. A vessel in a scenario without `PRPLEVEL` starts with empty tanks.
- **Exit on `-T` (stage 3.9):** on the rigs a host without graphics exits on `-T` on time (x64 and ARM, with and without a client); Orbiter's system time is about 1 s behind the wall clock and does not fall further behind. The log "stopping" is explained by a `run-orbiter.ps1` timeout shorter than `-T` plus loading; now the script writes to `run.log` when a copy exited by itself, and the rule for choosing the timeout is in `docs/STANDS.md`.
- **Control latency outliers (stage 3.10):** the latency measurement is split into steps (`INPUT` went out, a state with thrust arrived, display; on the host — from applying the key to sending the state) and runs on the wall clock; the host statistics line has the longest frame. Outliers up to 0.5 s on the rig with Wi-Fi and NAT come from the network, the host and client frames are short. Fixed: at 60 frames per second the client sent `INPUT` every other frame (about 39 times per second) — now the next time counts from the planned one (`nextSendTime`), and the host times `WORLD_STATE` the same way.
- **Extended input check (stage 3.11):** the host logs what the pilot's keys did (RCS mode and levels, engines, trim, wheel brakes, rotation), the client logs its notices. `run-orbiter.ps1` presses key combinations and sends extended keys (Delete, Insert, numpad `/`) correctly, `OlcRigTest` can move the focus (`focus`). Confirmed on the rig: RCS (rotation, translation, low power), throttle, hover, trim, wheel brakes, kill rotation, Ctrl+Alt+U, taking a free vessel by focus, "view only" on another vessel and Ctrl+F3; the DeltaGlider retro engine with closed covers cannot be checked.
- **Plan of stages 4 and 5 (task 3.12):** a detailed plan of the crew and the flight director's tools — `docs/PLAN.md`, section "Stages 4 and 5"; the order of the stages, the way to recognise the FD and the navigator's rights — question Q8 to the owner.
- **Code audit (task 3.13):** the host and the client disconnect a side they have heard nothing from for 15 s (a frozen computer, a pulled cable); a `HELLO` of another version gets `REJECT` 1; `INPUT` out of range, NaN and infinity are dropped; the client drops states of unknown vessels; a stack docked already in the scenario is recognised on the client, and the partner of a removed vessel does not freeze; `;` in the player name is removed; `run-orbiter.ps1` sends steps to the right copy (`instance`). Plugin version — 0.1-stage3.
- **Research of textures and add-ons (task 3.15):** `TEXTURES.md` - where the SPICE kernels and planet textures come from, sizes, licenses (not confirmed for the texture packs), a proposal for the test archive contents; question Q10 to the owner.
- **Test package (task 3.16):** `tools/package/make-package.ps1` builds three archives (Orbiter with D3D9Client, the SPICE kernels, the plugin with settings and scenarios), install instructions in `PACKAGE.md`; checked by unpacking on a clean Windows 10 x64 machine and a start without graphics.
- **Documentation and comments audit (task 3.14):** documents and comments are checked against the code in both languages: `ARCHITECTURE.md` describes the real interface and time sync, `PROTOCOL.md` the real `REJECT`, `PING` and handshake order; a 30 ms delay for a wire; `olc-netcheck` counts RTT above 60 and 200 ms.
- **Test procedure (task 3.17):** `docs/TEST-PROCEDURE.md` — roles, tests T1–T9 with keys and the expected outcome, the report form; PDFs in both languages. The test package got a starting `Orbiter.cfg` (modules on, collisions off) and was checked with graphics on a clean installation.
- **Task 5.1, core:** protocol version 5 (decision 54). `HELLO` and `WELCOME` got `flags` (observer, flight director); the host knows the director by `HostConfig::director`; an observer does not take a vessel (`OWNER_DENIED`, reason 5). New messages `TIME_REQUEST` (pause and warp, applied only from the director, `takeTimeRequests`) and `PLAYERS` (players, vessels, round trip time; on join, leave, ownership change and every 2 s). `PING` carries the client's latest round trip time. `ClientLink`: `sendTimeRequest`, `players`, `isDirector`, `isObserver`. Tests: messages, a director and an ordinary pilot, an observer, the player list over loopback. The plugin (tasks 5.2 and 5.3) does not use the new parts yet.
- **Task 5.3, plugin:** player summary on the flight director's screen — a text on the left (player, role, vessel, round trip time to the host), built by `core/player_summary.h` from `PLAYERS`, test `player_summary`. Checked on the rig with graphics.
- **Task 5.4, core and plugin:** world saves (decision 55, protocol version 6). New message `SAVE_REQUEST`; the host carries it out only for the flight director, the FD key is Ctrl+Alt+S, the file is `Scenarios\OLC\Saves\save-YYYYMMDD-HHMMSS.scn`. The host autosave is the setting `AutosaveMinutes` (default 10, 0 — off) into `Scenarios\OLC\Saves\autosave.scn`. Checked on the rig: host on .163, FD with graphics on olc-win-arm — the key save and the autosave appeared, Orbiter starts from a save (2 vessels).
- **Task 5.5, documentation:** `QUICKSTART.md` (section 6) describes pause and time acceleration on a server without graphics through `console-send.ps1` as a fallback when there is no flight director client.
- **Task 5.7, documentation:** recording a lesson for the debriefing (decision 56): the flight director presses Ctrl+C on a client with graphics, review is the standard playback; described in `QUICKSTART.md` (section 6) and `PILOT-GUIDE.md` (section 5). No code was needed; playback was checked on the rig on 2026-11-19.
- **Task 5.8, rig check:** a server without graphics (.163), the FD (olc-win-arm) and a pilot (.162): the shared pause from the FD reaches the pilot within 0.3 s, autosave works, the FD flies no vessel. Flight replay waits for the answer to Q12.
- **Task 5.9, leftovers of 5.2:** x10 from the FD reaches the pilot in about 0.3 s (rig); an observer or FD under the name of a pilot who left no longer gets that vessel (fixed in the core, a test); an FD section in `PILOT-GUIDE.md`: preparation, keys, saves, summary.
- **Task 4A.1, core:** protocol version 7 (decision 58). New TCP messages `ANIM_STATE` (host → clients: changed animations of a vessel) and `NAV_STATE` (transponder and NAV steps; a client sends it only for its own vessel, the host checks the owner, hands it to the plugin and sends it on). `HostLink` keeps the latest animations and frequencies of every vessel and sends them to a client that joins later; `ClientLink` gets `takeAnimStates`, `takeNavStates`, `sendNavState`. Tests of the messages and of delivery, owner check, relay, snapshot and forgetting a removed vessel; `PROTOCOL.md` and `ARCHITECTURE.md` updated (the stale version number in the packet header table fixed).
- **Task 4A.2, plugin:** animations of all vessels are shared (decision 58). The host scans each vessel up to 5 times a second (`GetAnimPtr`) and sends `ANIM_STATE` for animations that changed by more than 0.01 (a stop at 0 or 1 is always sent); the client sets them with `SetAnimation` on every vessel, waiting for vessels not created yet. Rig check: a DeltaGlider has 28 animations, none changes by itself, `G`/`K` cost 0.5-1 KB per press; no filter needed. Both logs got counters (`ANIM_STATE`, animations applied).
- **Task 4A.3, plugin:** NAV and transponder frequencies are shared (decision 58). The owner of a vessel checks its steps every 0.5 s and sends `NAV_STATE` on change (the host does it for the vessel it flies); the host sets a pilot's frequencies on its copy and the others set them on theirs; a client that joins later gets the latest ones. `OlcRigTest` got the actions `nav`, `xpdr` and `shownav`. Rig check: frequencies set on the pilot's vessel appear on the host and on a late client.
- **Task 4A.4, check and documents:** rig check of 4A (headless host, a pilot and an observer with graphics): the gear the pilot lowered is seen on the screen of the second participant (the cone cannot be told apart from the camera angle, checked by the counters); mechanisms and frequencies reach a late client right after joining. The limitation about mechanisms of someone else's vessel is removed from `TEST-PROCEDURE.md`.
- **Task 5.2, plugin:** flight director client. The host accepts `TIME_REQUEST` from the client named in the `Director` setting; the FD client takes no vessel (view only), its pause and warp go to the host. Warp near vessels works only with `VesselCollision = FALSE` (the core resets it to x1 on vessel collisions, `ORBITER-INTERNALS.md`, section 22). Checked on the rig: pause, x10 and x100.
