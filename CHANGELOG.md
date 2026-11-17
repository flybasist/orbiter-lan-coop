**English** · [Русский](CHANGELOG.ru.md)

# CHANGELOG

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
