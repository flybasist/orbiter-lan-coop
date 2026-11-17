**English** · [Русский](ru/QUICKSTART.md)

# Quick start

Setting up a developer environment (Windows and building Orbiter from source at the base
commit), building the plugin and running the host and the client. The commands come from
`COMPILE.md` and `CMakePresets.json` of the Orbiter repository. On the Windows 11 ARM test rig
(stage 0, [PLAN.md](PLAN.md)) the following were checked: a full build with D3D9Client, SPICE
kernels, plugin loading, running without graphics and two copies with graphics.

## 1. Requirements

- Windows 10/11 x64 (or Windows 11 ARM: Orbiter x64 runs through emulation).
- About 10 GB of disk space for sources, the build and textures.
- Internet access while dependencies are installed.

## 2. Preparing the environment

1. **Visual Studio** 2022 or 2026 (the free Community edition will do) with the
   "Desktop development with C++" workload and the "C++ MFC" component on the Individual Components tab.
2. **DirectX SDK (June 2010):** https://www.microsoft.com/en-us/download/details.aspx?id=6812.
   If a Visual C++ 2010 Redistributable is already installed, the installer fails with error S1023 and
   **does not install** the SDK (only a `system` folder is left in the install folder). Check by the file
   `Include\d3dx9.h`. Microsoft's workaround: remove Visual C++ 2010 Redistributable, install the
   SDK, put the Redistributable back. The script `tools/stands/setup-windows.ps1` does this.
3. **Git for Windows:** https://git-scm.com/install/windows.
4. **Python 3.13+** — only if you build with the `compileOrbiter.py` script (see below).

## 3. Building Orbiter

Option A — the script from the Orbiter repository (it installs dependencies itself, builds and creates
an `install` folder with a ready-to-run Orbiter). Download the repository, check out the base
commit and run `compileOrbiter.py`, choose item 3 in the menu (build the local repository).

Option B — by hand, from the "x64 Native Tools Command Prompt for VS"
(on Windows ARM — the x64 cross-build environment):

```bat
git clone https://github.com/orbitersim/orbiter.git
cd orbiter
git checkout a7a3396e
cmake --preset windows-x64-release
cmake --build --preset windows-x64-release
```

The build appears in `out\build\windows-x64-release`, a ready-to-run installation — after
`cmake --install out\build\windows-x64-release` in `out\install\windows-x64-release`.

What we found on the test rig:

- **Windows 11 ARM.** The x64 build runs from the ordinary `vcvarsall.bat x64` environment: the x64
  compiler works through emulation. CMake configuration (with third-party library downloads) takes about 4 minutes,
  the build about 6 minutes on 4 cores.
- **A full Orbiter cannot be built without the DirectX SDK.** With `-DORBITER_BUILD_D3D9CLIENT=OFF`
  LuaInterpreter fails: it needs `gcCoreAPI.h`, which appears only when D3D9Client is built.
  The `Orbiter` and `Orbitersdk` targets (this is how CI builds) still build. The build folder does not
  replace an installation: Orbiter looks for some modules (planet atmospheres) where only
  `cmake --install` puts them.

## 4. First run

1. Planet textures are not part of the repository. Give their folder with the
   `ORBITER_PLANET_TEXTURE_INSTALL_DIR` variable before building or with the `PlanetTexDir` parameter in `Orbiter.cfg`
   afterwards (for example, from an installed Orbiter 2016 or 2024).
2. **SPICE kernels are required.** Without `Kernels\orbiter.bsp` (1.6 GB) Orbiter closes the session
   right after starting (`SUN - Couldn't load kernel`). The archives are at the links in Orbiter's
   `COMPILE.md`, SPICE section: `Kernels.7z` (1.2 GB) is unpacked into `Kernels`, `Textures.7z` into
   `Textures`. For a second Orbiter copy on the same machine `orbiter.bsp` need not be copied: a
   hard link will do (`mklink /H`).
3. Start Orbiter, turn on the D3D9Client graphics client on the Modules tab, open a
   scenario with a DeltaGlider and make sure everything flies. Without the launcher: `Orbiter.exe -s
   "Checklists\Quickstart" -p D3D9Client`.
4. **Earth textures are neither in the build, nor in `Textures.7z`, nor in the Orbiter release on GitHub** — they
   are installed as a separate package. Without them D3D9Client shows a "Surface textures
   are missing for Earth" window at start and waits for OK. For checks where the surface does not matter,
   an empty `Textures\Earth` folder is enough: there is no window, and Earth has no texture.

## 5. Building the OLC plugin

The plugin needs the SDK headers and two libraries from the Orbiter build (section 3). Assemble
an SDK folder, for example `C:\olc\orbiter-sdk`:

- `include` — a copy of `Orbitersdk\include` from the Orbiter repository;
- `lib\Orbiter.lib` and `lib\Orbitersdk.lib` — from the Orbiter build folder
  (`out\build\windows-x64-release`, search by file name).

Then in the same x64 command prompt, from the OLC folder:

```bat
cmake -S . -B orbiter-lan-coop.out -G Ninja -DCMAKE_BUILD_TYPE=Release -DOLC_ORBITER_SDK_DIR=C:\olc\orbiter-sdk
cmake --build orbiter-lan-coop.out
ctest --test-dir orbiter-lan-coop.out
```

The finished plugin is `orbiter-lan-coop.out\plugin\OrbiterLanCoop.dll`. Copy it into Orbiter's
`Modules\Plugin` and turn it on in the Modules tab. How to run the host and the client —
section 6.

The GitHub Actions build (`.github/workflows/build.yml`) does the same: it builds the
Orbiter core from the base commit and publishes the DLL as the `OrbiterLanCoop-x64` artifact.

## 6. Running the host and the client

1. Copy `OrbiterLanCoop.dll` into `Modules\Plugin` of both Orbiter copies (host and client)
   and turn the plugin on in the Modules tab or with the `-p OrbiterLanCoop` switch.
2. In Launchpad, on the "Extra" tab, open the "OLC - LAN co-op" item: role, host address, port,
   player name. The settings are saved to `Config\OrbiterLanCoop.cfg` (an example with explanations is
   `plugin/OrbiterLanCoop.cfg`; without graphics, over SSH, the file is written by hand).
3. The host (`host`) starts any scenario as usual.
4. The client (`client`) presses "Connect" in the same window: the plugin gets the host's world, writes
   it to `Scenarios\OLC\Host world.scn` and starts it itself. In 20–40 s the student is in the host's world.
   The fallback is to start this scenario by hand, or the same scenario as the host's:
   the client connects at the start of any session, creates missing vessels and deletes extra ones.
5. Both `Orbiter.log` files contain `OLC ...` lines: the connection, and every 10 s statistics
   (delay, received packets, sim time error).

On the client it is better to turn off vessel collisions (`VesselCollision = FALSE` in
`Orbiter.cfg`): otherwise the local calculation argues with the host's states. A vessel cannot be
controlled on the client yet: the client only shows the host's world.

To measure the position error, set `TraceVessel` (the name of a flying vessel) on both: each
copy's `olc-trace.csv` gets the positions, and they can be compared by MJD.

Two copies with graphics on one screen:

- the window size is `WindowWidth` and `WindowHeight` in each copy's `Orbiter.cfg`, in logical
  pixels (at 200% screen scaling a 640 pixel window takes 1280 dots). Orbiter saves
  these fields on exit only if `DeviceIndex` is also set in the file, otherwise the size resets;
- Orbiter looks for a recorded flight in `Flights\<scenario name>`: if a scenario is copied under
  another name (for example, to change the camera), the recording folder is also copied under the new
  name;
- on the host the recording drives the camera during playback; the client has its own camera from
  the scenario.

## 7. Updating

The Orbiter base commit changes only deliberately (see [README.md](README.md), condition 3):

```bat
git fetch
git checkout <new commit or release tag>
```

After that Orbiter is rebuilt, and the core map ([ORBITER-INTERNALS.md](ORBITER-INTERNALS.md))
is checked against the new code.

## 8. Common problems

| Message or symptom | What to do |
|---|---|
| Errors while building the documentation (LaTeX) | The Windows presets build without documentation (`ORBITER_MAKE_DOC=OFF`); if you turned it on, build in one thread |
| CMake does not find system headers with the Ninja generator | Run from the "Native Tools Command Prompt", not from an ordinary console |
| Planets without textures | The texture folder is not set (see section 4) |

## 9. Removal

Delete the Orbiter clone folder and the `orbiter-lan-coop.out` folder. Visual Studio and the DirectX SDK
are removed through "Add or remove programs".
