**English** · [Русский](CHANGELOG.ru.md)

# CHANGELOG

## Unreleased (branch `dev-0.2`)

Changes after `v0.1`. The protocol has not changed: the round 1 package works with a host from this branch.

- **Orbiter base commit** — `48c30c77` (check of 2026-11-23): the new Orbiter command line options are
  described in `docs/ORBITER-INTERNALS.md`, section 15.
- **Core build on Linux in GitHub Actions:** the `linux` job builds `olc-core` and runs the tests.
- **The OLC server without graphics under Wine** on Linux x86 is checked with docking (task 3.29).
- **The plugin version in the host log** (task 3.32): the line about a joining player now reads
  `player 'Ann' joined as client 1 (own scenario, plugin 0.1)` — the log shows who came with a different package.
- **No code duplicates** (task 3.31): building a UDP packet, comparing `DOCK` links, what the host and
  the client share in the plugin (`plugin/common.h`), the settings defaults come from the core
  constants. The behaviour is the same, checked on the rig.
- **Scenario** `tools/scenarios/Two Shuttle-A Moon.scn` — two Shuttle-A on the Moon; not part of the
  round 1 package.
- **Documents:** `docs/ORBITER-VERSIONS.md` (Orbiter 2016, 2024 and the OLC build for the testers),
  agent permissions for autonomous work (`docs/README.md`), a check of the documents against the code.

## v0.1 — The first version for tests by people (2026-11-19)

The `v0.1` tag on GitHub. This version goes to the space club: the round 1 test package and the
procedure. The detailed history of the work is in `docs/PLAN.md` and git; this is what goes into the version.

### What is there

- **The `OrbiterLanCoop` plugin** on the official Orbiter SDK (base commit `a7a3396e`, Windows x64):
  host and client roles, the "OLC - LAN co-op" item in the Launchpad, settings in `Config\OrbiterLanCoop.cfg`.
- **A shared world over LAN.** Only the host computes physics; 60 times a second it sends vessel
  states, the client shows them with a 30 ms delay and smoothing. Shared time and pause. The "Connect"
  button: the client gets the host's world and starts it itself; vessels of unknown classes are removed first.
- **Every pilot has their own vessel.** Take a free vessel with the focus (F3), release with Ctrl+Alt+U;
  someone else's is view only. The pilot's keys and joystick go to the host, single key presses
  (autopilots, gear, cone) too. A lost connection: the controls are released, the vessel waits 60 s.
- **Docking and undocking** are repeated for everyone, including a client that joins later.
- **Mechanisms and frequencies.** All participants see the gear, the cone and other animations, the
  NAV and transponder frequencies of a vessel.
- **The flight director** (the name is the host setting `Director`): shared pause and time acceleration,
  the player summary on the screen, saving the world with Ctrl+Alt+S and autosave, flight recording with
  the stock Ctrl+C key. The server can run without graphics, also on one machine with the FD client.
- **Protocol version 7** — `docs/PROTOCOL.md`; the host does not accept a client with another protocol version.
- **The test package** — `tools/package/make-package.ps1` (three archives), installation — `docs/PACKAGE.md`,
  the procedure — `docs/TEST-PROCEDURE.md`, the guide — `docs/PILOT-GUIDE.md`.
- **Tools:** `olc-netcheck` (network check), `olc-smoothing` (smoothing model), `olc-doccheck` (RU and
  EN document pairs), `OlcRigTest` and the rig scripts in `tools/stands/`.
- **Core tests** without third-party libraries (`ctest`), a build in GitHub Actions.

### Known limitations

- Windows x64 and a local network only; the protocol is not secured and not for the internet.
- The key layout must match between participants; clicks on the cockpit panel are not passed (the
  DeltaGlider retro doors cannot be opened on a client).
- Time acceleration near vessels works only with `VesselCollision = FALSE`.
- The internal module state on a vessel copy does not change (for example, the gear touchdown points on a client).
- The plugin interface is in English.
