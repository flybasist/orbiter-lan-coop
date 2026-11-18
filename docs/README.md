**English** · [Русский](ru/README.md)

# orbiter-lan-coop (OLC) documentation

OLC is a plugin for the Orbiter space flight simulator that lets several people fly in one
simulation from different computers on a local network. The prototype (stage 1) is under way:
there is an architecture, a plan, a map of the Orbiter core, a smoothing model, a network core
with tests and a plugin in which the host sends the world state and the client shows it.
Testing happens on test rigs.

## Contents

| File | What it covers |
|---|---|
| [QUICKSTART.md](QUICKSTART.md) | Setting up a developer environment and building Orbiter from source |
| [ARCHITECTURE.md](ARCHITECTURE.md) | How the plugin is built and why it is done this way |
| [PROTOCOL.md](PROTOCOL.md) | Network protocol: channels, messages, fields (draft) |
| [PLAN.md](PLAN.md) | Work stages, prototype criteria, test rigs |
| [SMOOTHING.md](SMOOTHING.md) | Motion smoothing: state rate, buffer delay and interpolation method based on the model results |
| [RESEARCH.md](RESEARCH.md) | Research: Orbiter, past attempts, Linux and macOS, licenses, releases |
| [ORBITER-INTERNALS.md](ORBITER-INTERNALS.md) | Map of the Orbiter core: what the plugin relies on, with references to the code |
| [SDK-CHEATSHEET.md](SDK-CHEATSHEET.md) | Cheat sheet: which SDK function to call for a specific task and what the catch is |
| [SECURITY.md](SECURITY.md) | What is protected, what is not, and why LAN only |
| [STANDS.md](STANDS.md) | Test rigs: supported systems, machine roles, agent access, preparation. Your own machines go into `stands.yaml` (not in git, example — `stands.example.yaml`) |
| [CREDITS.md](CREDITS.md) | Third-party components and their terms |
| [DECISIONS.md](DECISIONS.md) | Log of decisions made |
| [QUESTIONS.md](QUESTIONS.md) | Open questions to the project owner |

---

## For AI agents and developers

This page is the entry point. Read it first, then the file you need from the contents.
Do not rescan the whole repository: the map below is enough.

### What is where

| File | Purpose |
|---|---|
| `README.md`, `README.ru.md` | A short description for a human (English and Russian versions). |
| `CHANGELOG.md`, `CHANGELOG.ru.md` | Change history. |
| `docs/`, `docs/ru/` | All other documentation: the English version is in `docs/`, the Russian one (the source) is in `docs/ru/`. It must always match the code. |
| `CMakeLists.txt` | Root CMake build. The build folder is always `orbiter-lan-coop.out`. |
| `core/` | The `olc-core` network core: the protocol and everything that does not depend on Orbiter (`ARCHITECTURE.md`, section 6). |
| `tests/` | Core tests on CTest without third-party libraries; checks are in `tests/check.h`. |
| `plugin/` | The `OrbiterLanCoop.dll` plugin for Orbiter (Windows only): stage 1 host and client roles, settings from `Config\OrbiterLanCoop.cfg` (example — `plugin/OrbiterLanCoop.cfg`). Build — `QUICKSTART.md`, section 5, running — section 6. |
| `.github/workflows/build.yml` | GitHub Actions build: the Orbiter core from the base commit (cached), then the OLC core with tests and the plugin for Windows x64. |
| `tools/smoothing/` | The `olc-smoothing` smoothing model: simulates host packets with delay and loss and compares ways of reconstructing motion on the client; `run-grid.sh` runs it over a parameter grid. Does not depend on Orbiter. Conclusions — `SMOOTHING.md`. |
| `tools/netcheck/` | The `olc-netcheck` utility: a network check between two machines on the OLC network core — TCP handshake, UDP `PING`/`PONG`, loss and delay. How to use it — `STANDS.md`, section 10. |
| `tools/doccheck/` | The `olc-doccheck` check: every document has a version in the other language and the structure of the pair matches (headings, tables, code blocks). Runs in CI. |
| `tools/rigtest/` | The helper plugin `OlcRigTest.dll` for test rigs, not part of OLC: creates and deletes vessels on a schedule from `Config\OlcRigTest.cfg`. How to use it — `STANDS.md`, section 11. |
| `tools/stands/` | Scripts for a Windows test rig: `setup-windows.ps1` — preparation for building Orbiter and OLC (Git, Visual Studio, DirectX SDK, firewall); `setup-autologon.ps1` — automatic sign-in of the user (the owner types the password); `register-run-task.ps1` and `run-orbiter.ps1` — starting Orbiter on the visible desktop on a command from SSH, with screenshots and presses in windows (Launchpad, the OLC dialog); `console-send.ps1` — console commands for Orbiter without graphics (pause, time acceleration). Test rig layout — `STANDS.md`. |

The Orbiter source code is not part of this repository. A separate clone of
`orbitersim/orbiter` is used for reading (the owner's is `~/Documents/github/orbiter`), see "Conditions" below.

### Conditions that must not be broken

1. **A plugin, not a fork.** OLC works through the official Orbiter SDK (`Orbitersdk/include`)
   and does not need a modified core. If a hook in the core is unavoidable, that is a separate
   decision made as a PR to `orbitersim/orbiter`, not as a local change.
2. **The Orbiter clone is read-only.** Nothing is changed in it and nothing is built "for us".
   The only allowed action is updating from GitHub (`git pull --ff-only`) during an upstream
   check. The core map and all conclusions about it live here, in `docs/ORBITER-INTERNALS.md`.
3. **The Orbiter base commit is the last checked commit of `main`:** currently `a7a3396e`
   (2026-11-12). All references to core code in the documentation are for it. The base commit
   moves forward after every upstream check (the "Check against Orbiter" section in
   `docs/ORBITER-INTERNALS.md`), so that OLC does not get stuck between releases.
4. **Only the host computes physics.** The client makes no decisions about vessel motion: it
   shows the state it receives and sends input. The client has no "physics of its own".
5. **LAN only.** The protocol is not designed for the internet and is not secured (see `SECURITY.md`).

### Where to change what

The paths below are to the Russian version (the source); the English version of the same file is
changed in the same commit (decision 29).

| What needs changing | Where to edit |
|---|---|
| Architecture, protocol, decisions | `docs/ru/ARCHITECTURE.md` |
| Stages, criteria, test rigs | `docs/ru/PLAN.md` |
| Facts about the Orbiter core, the base commit | `docs/ru/ORBITER-INTERNALS.md` and condition 3 above |
| Environment setup | `docs/ru/QUICKSTART.md` |
| The network core and its tests | `core/`, `tests/`; design — `docs/ru/ARCHITECTURE.md`, section 6 |
| State rate, buffer delay, smoothing | `docs/ru/SMOOTHING.md` (based on the `tools/smoothing/` model) |

### Rules for working on the project

- **Languages (decision 29).** Documentation is in Russian and English, both versions complete.
  The source is the Russian one (`README.ru.md`, `CHANGELOG.ru.md`, `docs/ru/`), the English one
  (`README.md`, `CHANGELOG.md`, `docs/`) is its full translation. Any document change goes into
  both versions at once, in one commit; the structure of the pairs (headings, tables, code blocks)
  is checked by `tools/doccheck` in CI. Orbiter and SDK terms are not translated.
- Comments in code, scripts and example settings are in English only. Everything the
  plugin writes to the Orbiter log, to windows and to error messages is in English too.
- Code must work and be good: simple, clear and easy for people and AI agents to extend.
  Clarity matters more than elegance; complexity only when it is needed.
- After any change, check `README.md`, `docs/` and the comments against the code. They must
  describe the current state, not the past.
- Do not leave unused functions, files or dependencies.
- On test rigs (Windows, Linux) the AI agent builds, starts and stops Orbiter and
  OLC over SSH and reads logs. Installing programs, system settings, snapshots and docker are done
  by the owner only (decision 20, layout — `STANDS.md`). The agent works only with machines from
  `stands.yaml`; no file — no test rigs (decision 26). The exception is machines with
  `access: full`: the owner has given them to the agent entirely, and there it installs packages and
  reboots itself (decision 27).
- Git: until the first fixed release (around 0.1) the agent commits finished tasks
  itself and pushes them straight to `main` — with the owner's permission, in this project only.
  After the release — work in branches. Commit messages are in English (decision 38).
- The check build folder is always the same — `orbiter-lan-coop.out` — and it is
  listed in `.gitignore`.

### Autonomous work

The owner is busy during the day, so the AI agent works on the project by itself on a batch of tasks
the owner approved. Rules:

1. **Only what is approved.** The agent does items of `PLAN.md` and tasks agreed in
   chat. Code research and the core map are added to the queue and done by the agent itself
   (decision 21); anything else new goes through a question.
2. **A fork in the road is a question, not a guess.** A question with options and a recommendation is
   written to `QUESTIONS.md`, the owner gets a notification, the agent switches to another independent
   task. The decision made is recorded in `DECISIONS.md`.
3. **Windows or Linux needed — wait for a test rig.** No workarounds.
4. **Models.** Opus — architecture, task setting, checking results. Sonnet —
   implementation and code analysis from a ready task description. Haiku — code search. Opus always
   checks a subagent's result itself.
5. **Schedule (UTC+5).**
   - The agent works in a row while there is work and limit. It does not wake up by itself after a
     stop, so on reaching 90% of the window it schedules a one-time run for the window reset plus
     5 minutes in advance (at the limit the session is cut off, and there is no time left to schedule).
     Waiting for a long process — a one-time check after the needed time. There are no permanent
     periodic runs.
   - 20:00–00:00 is the owner's time: they answer questions, check work, set up test rigs.
     During this time the agent works only at their request.
   - Weekends are discussed separately each time; without an agreement the agent does not
     work on weekends.
6. **Limits** (decisions 22 and 40). Before each piece of work the agent checks usage:
   - the 5-hour window: new work only below 90%; a step already started is brought to a whole state,
     but not above 95%: the owner can always write a message;
   - the week is spent by the reset (Saturday, 18:00): at any moment no more than
     (share of the week passed) × 100% + 5% is used. Running out a few hours before the reset is fine,
     a day before is not;
   - the limit is not spent for the sake of spending: if there is no useful work (everything waits for test rigs or
     the owner's answer), the agent stops;
   - if the threshold is crossed in the middle of work, the agent schedules the next run, brings the step to a
     whole state and writes down where it stopped.
7. **Check against Orbiter.** If the check log (`ORBITER-INTERNALS.md`, section 13) has no
   entry for today, the first thing is an upstream check following the procedure described there.
8. **No needless stops** (decision 37). The agent works through the queue in a row and does not wait
   for the owner after every batch. It waits in two cases only: it hit a limit, or an owner's decision is
   needed (a costly workaround, a risk of breaking something, a choice of path). Work for the sake of work
   is not done.
9. **Report.** Every batch ends with a report: what was done, what was checked, what was not,
   how much limit was used.
10. **Git.** The agent commits a finished task and pushes it to `main` (see "Rules for working
   on the project"); unfinished work does not go into a commit.
