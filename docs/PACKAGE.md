**English** · [Русский](ru/PACKAGE.md)

# Test package

Ready archives for people who test OLC (task 3.16, decisions 43 and 50): nothing has to be built.
The archives are made by `tools/package/make-package.ps1` from a ready Orbiter installation
(see section 3). The test procedure itself — [TEST-PROCEDURE.md](TEST-PROCEDURE.md).

## 1. What is in the package

| Archive | Size | Contents |
|---|---|---|
| `olc-1-orbiter.zip` | about 160 MB | Orbiter of the base commit (x64) with D3D9Client, an empty `Textures\Earth` folder |
| `olc-2-kernels.zip` | about 1.3 GB | SPICE kernels (the `Kernels` folder) — Orbiter does not start without them |
| `olc-3-plugin.zip` | about 0.2 MB | The `OrbiterLanCoop.dll` plugin (version `0.1`), the `Config\OrbiterLanCoop.cfg` template, the `Scenarios\OLC-test` scenarios, a starting `Orbiter.cfg` (D3D9Client and the plugin on, vessel collisions off), the license texts of OLC and of the plugin's third-party components |

Earth textures are not in the archives (decision 50): the licenses of the texture packs are not
confirmed, so they are installed from a link (section 2, step 5). Without them the Earth is grey,
but docking can still be tested.

## 2. Installation

You need Windows 10 or 11 x64 and 3 GB of free space. For the picture: a video card with DirectX 9,
the "Visual C++ 2015–2022 Redistributable x64" and the DirectX End-User Runtime (June 2010)
([QUICKSTART.md](QUICKSTART.md), section 2).

1. Create an empty folder without spaces in the path, for example `C:\OLC`.
2. Unpack `olc-1-orbiter.zip` into it, then `olc-2-kernels.zip`, then `olc-3-plugin.zip` (in this
   order, into the same folder; Explorer's "Extract all" or `tar -xf <archive>` in that folder
   will do). The archives share only folders, nothing is overwritten.
3. Start `Orbiter.exe`. The `D3D9Client` graphics client and the `OrbiterLanCoop` plugin are already
   on (the `Orbiter.cfg` from `olc-3-plugin.zip`); you can check it on the Modules tab of the Launchpad.
4. On the "Extra" tab open the "OLC - LAN co-op" item: role (`host` or `client`), host address,
   port and player name. The same settings are in `Config\OrbiterLanCoop.cfg`; without the file the
   plugin does nothing. Then follow [QUICKSTART.md](QUICKSTART.md), section 6, and
   [PILOT-GUIDE.md](PILOT-GUIDE.md).
5. Planet textures (optional; without them the Earth is grey, which does not hinder the tests). They
   come from Orbiter 2016 (build 160828) and are not in our package (decision 50).
   - **Orbiter 2016 is already installed** (for example in `C:\Orbiter2016`): nothing has to be
     copied. Add the line `PlanetTexDir = C:\Orbiter2016\Textures` to `C:\OLC\Orbiter.cfg` (the path
     to the `Textures` folder of your 2016 installation). Orbiter takes planet surfaces from there and
     everything else from its own installation; the line is kept on exit.
   - **No Orbiter 2016:** download the Orbiter 2016 archive (`Orbiter2016.zip`, about 2.4 GB) — the
     official site `orbit.medphys.ucl.ac.uk` no longer opens, mirrors are collected on the Orbiter
     forum (`orbiter-forum.com`, the topic "Orbiter 2016 – Verified Download Mirrors"). Unpack it into a
     separate folder, for example `C:\Orbiter2016` (it does not have to be started), and do as in the
     item above.
   - A fallback is copying: copy everything from `C:\Orbiter2016\Textures` into `C:\OLC\Textures`
     and answer "Skip" to the question about matching files: the package's own files must not be replaced.

   We checked in the Orbiter sources that planet surfaces are read from `PlanetTexDir`
   (`GraphicsAPI.cpp:197`); with a real 2016 installation it is not checked on a rig.
6. Firewall: allow `Orbiter.exe` on the private network; the default host port is 47800 (TCP and UDP).

Check: after a scenario starts with the plugin, `Orbiter.log` has the lines `OLC: session started`
and `OLC: running as host` (or `client`).

## 3. How to build the archives

On Windows x64 with a ready Orbiter installation with D3D9Client (`QUICKSTART.md`, section 3,
`cmake --install`) and a built plugin (section 5):

```bat
powershell -ExecutionPolicy Bypass -File tools\package\make-package.ps1 -OrbiterDir C:\olc\orbiter -PluginDll <path>\OrbiterLanCoop.dll -OutDir C:\olc\package
```

The script leaves out everything that belongs to one machine: `Orbiter.cfg`, logs, traces, the
working `OLC` and `OLC-test` scenarios, the `OlcRigTest` plugin, the SDK and build leftovers (`*.lib`, `CMakeLists.txt`); it writes the starting `Orbiter.cfg` itself. The archives are in `OutDir`.

## 4. Check of 2026-11-18

The archives were built on Windows 11 x64 (Proxmox) from an installation of base commit `a7a3396e`,
unpacked by section 2 into an empty folder on a second machine (Windows 10 x64, Proxmox) and started
without graphics: `Orbiter.exe -s "OLC-test\Two DG 20 m" -p OrbiterLanCoop -T 15`. The log has
`OLC host: listening on port 47800`, `2 vessels`, `Fast process shutdown`, exit code 0. With the
picture — on Windows 11 ARM (Parallels, 3D): the package was unpacked by section 2 into an empty
folder, `Orbiter.exe` started without switches, D3D9Client and the plugin came on from the starting
`Orbiter.cfg`, "Connect" to a host without graphics with the `Two DG 20 m` scenario — the client is
in the host's world, two DeltaGliders on the screen, "you fly 'GL-01'".

## 5. Check of the round 1 package (2026-11-19)

The archives with plugin `0.1` were built by section 3 and unpacked by section 2 into an
empty folder on Windows 11 ARM (Parallels, 3D): all three archives unpacked without errors, the starting
`Orbiter.cfg` and the plugin are in place. This installation connected as client `FD` to a server without
graphics (Windows 11 x64, `Director = FD`): the "flight director - view only" message, the player summary
on the screen; `T` and `R` — the host set x10 and x1, `Ctrl+Alt+S` — a `save-...scn` file on the host,
autosave is written; `Ctrl+C` — a recording in `Flights\` and `Scenarios\Playback\`, the recording
scenario opens and runs with the plugin off. The archive checksums are in `SHA256SUMS.txt` next to them; the script does not write it, the sums were computed separately (`shasum -a 256 olc-*.zip`).
