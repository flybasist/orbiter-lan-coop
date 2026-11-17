**English** · [Русский](ru/RESEARCH.md)

# Research

The information was collected on 4–5 November 2026. Code numbers are for the Orbiter base commit
`9721ab8d`. How the core works is in [ORBITER-INTERNALS.md](ORBITER-INTERNALS.md).

## 1. Orbiter today

- A free space flight simulator with realistic physics. The author is Martin Schweiger. Since 2021 the
  code has been open on GitHub under the MIT license.
- C++ for Windows: windows and input — Win32 API and DirectInput, graphics — DirectX 9 (D3D9Client).
  Build — CMake and Visual Studio.
- Code size (C/C++/Lua, including comments):

  | Part | Lines | What is inside |
  |---|---|---|
  | `Src/Orbiter` — the core | ~88 k | physics, time, scene, input, MFD |
  | `Src/Celbody` | ~145 k | ephemerides of planets and moons, the SPICE library |
  | `Src/Vessel` | ~44 k | standard vessels |
  | `OVP/D3D9Client` | ~68 k | graphics client |
  | `Orbitersdk`, plugins, `Utils`, `Extern` | ~130 k | SDK, plugins, utilities, Lua, imgui, zlib |

- Activity: 2332 commits, 43 authors over all time, about 10 active in the last year
  (86 commits). About 2000 stars and 290 forks on GitHub.
- Documentation: user and add-on developer manuals (`Doc/`), doxygen in the
  SDK headers. The internal structure of the core is not described anywhere.

## 2. Past multiplayer attempts

| Project | Years | How it works | Outcome |
|---|---|---|---|
| OMP (Orbiter Multiplayer Project) | 2006–2017 | A "universe" server + clients; each computes the physics of its own vessel, states over UDP between clients, clock synchronization | Never finished, the last version OMP 2016 is for 32-bit Orbiter |
| OMX | after OMP | A continuation of OMP, GPLv2 | Limited adoption |
| Multiplayer Lite | ~2024 | A web server + a client plugin, session synchronization | Simple synchronization, not a shared flight |
| Project Hamac, IRCMFD, Multiorb | various | Experimental | Abandoned |

The common problem: each computer has its own physics, so the worlds drift apart, and docking
vessels of different players and time acceleration work poorly. None of the solutions works
with Orbiter 2024 x64.

What makes OLC different: one physics on the host; a modern Orbiter with an open core where
hooks can be proposed; a narrow goal — a LAN, 2–4 people.

## 3. Linux and macOS

- **Linux via Wine/DXVK/Proton** already works (community, OrbiterWiki). We will check it on
  a Steam Deck and Linux x86.
- **Native Linux** is being done by the community: the `linux` branch (Gondos, according to forum posts) and PR #561 moving to SDL3
  (open since February 2025, +7.9/−10.6 k lines). `CMakePresets.json` has build presets
  using `winegcc`. A native port is not part of OLC.
- **macOS** — only through a virtual machine with Windows or CrossOver.

## 4. Licenses

| Part of Orbiter | License |
|---|---|
| Core, SDK, standard vessels and plugins | MIT |
| XRSound | MIT |
| `OVP/D3D9Client` | GPLv3 / LGPLv3 at your choice |
| Libraries in `Extern` | free licenses (MIT, zlib and others), fonts — SIL/FFC |

- OLC links only with the SDK (MIT), so it can be released under GPLv3.
- `Orbitersdk.lib` is a static library, its code ends up in the plugin DLL. A release must
  include Orbiter's MIT notice (see [CREDITS.md](CREDITS.md)).
- MIT gives no rights to the name: OLC is an add-on "for Orbiter", not an official part.
- If OLC ever needs to be built into the core (MIT), the author can relicense the code;
  this must be agreed in advance when accepting third-party PRs.

## 5. Orbiter releases

- The latest stable release is the `2024` tag (31.12.2024).
- The `latest` tag is a nightly build from `main`, re-released automatically.
- `main` has 131 commits since the 2024 release. Important for OLC: vessel collisions (`CollisionCore`,
  10.2026, the first of three parts), ImGui dialogs, `oapiGetKeyState`, keyboard input in
  ExtMFD, time via `std::chrono`, a fix of ephemeris interpolation.
- SDK changes since 2024 only add things: old plugins load in `main`, new
  functions are not available in the 2024 release.
- GitHub has an open "2026 Release" milestone due 28.12.2026. There is no official announcement.

## 6. Links

- Orbiter: https://github.com/orbitersim/orbiter
- OMP: https://www.orbiterwiki.org/wiki/OMP
- Running under Wine: https://orbiterwiki.org/wiki/Running_Orbiter_under_Wine
- SDL3 port: https://github.com/orbitersim/orbiter/pull/561
- Forum discussion of multiplayer levels: https://www.orbiter-forum.com/threads/multiplayer-concept-overview.4791/
