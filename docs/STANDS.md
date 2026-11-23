**English** · [Русский](ru/STANDS.md)

# Test rigs

How to check OLC on real machines and how the AI agent works with them. The document is general:
every participant or fork has their own set of machines. Here — which systems make sense,
which roles machines have and how to prepare them.

Everyone describes their own machines in the `stands.yaml` file in the repository root (example —
[`stands.example.yaml`](../stands.example.yaml)). The file is in `.gitignore` and never gets into git.

> In section 7 what was checked on test rigs is marked with a date; the rest are hypotheses checked
> as the stages go ([PLAN.md](PLAN.md)).

## 1. Layout: one brain, many hands

- **The brain** is the AI agent's session on the developer's machine. It has the whole context: `docs/`,
  decisions, the task queue. A separate agent is not installed on each test rig: it would have its own
  context, synchronization through git and the same account limit.
- **The hands** are SSH to the test rigs. Building, starting and stopping Orbiter and OLC, collecting logs,
  copying built files. Work on a test rig can be given to a subagent: it uses
  the same SSH.
- **The eyes** — only where a screen is unavoidable (section 6).

## 2. Supported systems

Orbiter is a Windows x64 program. Everything else runs it through emulation or Wine.

| System | What it can do | Value as a test rig |
|---|---|---|
| Windows 10/11 x64 | Everything: build, host, client, host without graphics | The reference. At least one is needed |
| Windows 11 ARM (a virtual machine on a Mac) | Everything, but Orbiter x64 runs through emulation | Convenient for development; FPS and smoothness are not meaningful |
| Linux x64 + Wine or Proton (including Steam Deck) | The OLC server without graphics under Wine 10 is checked (task 3.29); a client with graphics, Proton and Steam Deck are not checked; not the plugin build | A server without graphics; checking clients on Linux |
| Linux x64 or ARM without Wine | Building and testing the `olc-core` network core | A second machine for network tests; Orbiter does not run |
| macOS | Building and testing `olc-core`, the agent's session | Orbiter does not run natively; the project does not check Wine on Mac |
| FreeBSD, other systems, 32-bit Windows | — | Supported by neither Orbiter nor OLC. Makes no sense |

For Linux we recommend Debian 13 or Ubuntu LTS: WineHQ publishes its own packages for them.

## 3. Machine roles

Values of the `roles` field in `stands.yaml`:

| Role | What the machine does | What it needs |
|---|---|---|
| `build` | Builds Orbiter and OLC | Windows, Visual Studio (section 8.1) |
| `client` | Runs Orbiter with graphics and connects to the host | A graphics card with DirectX 9 (or Vulkan under Wine) |
| `host` | Runs Orbiter with graphics and hosts the session | The same as `client` |
| `headless-host` | Hosts the session without a graphics client ("Orbiter Server Console") | No graphics card needed |
| `core-tests` | Builds and tests `olc-core`, takes part in network tests | A C++ compiler and CMake |

## 4. How many machines are needed

- **The minimum to start (stage 0 and the start of stage 1):** one Windows machine with the roles `build`,
  `host` and `client`. The host and the client are two Orbiter instances on it
  ([ORBITER-INTERNALS.md](ORBITER-INTERNALS.md), section 15).
- **For the whole of stage 1:** a second machine on the same network — to check real loss and
  delays, not an exchange inside one machine. At least one of the two is Windows x64 without
  emulation: smoothness is judged on it.
- **Further, as desired:** Linux via Wine or Proton, Steam Deck, different Windows versions —
  checking clients, not the idea.

## 5. Access

1. On every machine — a separate user (we recommend `olc`), SSH login with a key only.
2. A separate key for test rigs on the developer's machine (for example, `~/.ssh/olc_stands`) and
   short machine names in `~/.ssh/config`. The address, user and key live only there.
3. The name from `~/.ssh/config` goes into the `ssh` field in `stands.yaml` — the agent works by it.
4. Windows: the OpenSSH Server component, commands via PowerShell.
5. Access only to the machines themselves — not to the hypervisor or its API.

## 6. What the agent does and what the rig owner does

| Action | Who |
|---|---|
| Installing programs (Visual Studio, DirectX SDK, CMake, Wine), system settings, firewall | The owner |
| Machine snapshots and rollback | The owner |
| docker commands | The owner |
| Building Orbiter and OLC, starting and stopping Orbiter, copying OLC files, reading logs | The agent (decision 20 in [DECISIONS.md](DECISIONS.md)) |

This is how a machine with `access: limited` in `stands.yaml` works — for example, a personal machine lent
for a while. If a machine is given to the agent entirely (`access: full`), it installs and removes packages itself,
reboots it and changes settings (decision 27). The agent does not type passwords: installing
packages on such a machine needs passwordless `sudo`.

### How to look at the picture

Orbiter is a windowed application. On Windows a program started over SSH runs in an
invisible service session, not on the desktop. So:

- **The main thing is to manage without a screen.** The plugin writes the delay, loss and position error
  to the log; the agent reads the log over SSH. A screen is needed only for the final smoothness check.
- **A virtual machine on a Mac** (`screen: mac-window`): its window is a window on the Mac screen. The
  agent looks at the picture through screenshots taken by `run-orbiter.ps1` (below).
- **A server hypervisor** (`screen: console`): a console in the browser, the owner logs into the hypervisor.
- **Without a graphics client Orbiter runs right over SSH** (checked): the
  "Orbiter Server Console" opens, the simulation runs, the plugin loads. For network tests without
  a picture the scheduled task is not needed. Console commands (`pause on`, `tacc 10`) are fed to such an
  Orbiter by [`tools/stands/console-send.ps1`](../tools/stands/console-send.ps1).
- **Starting Orbiter on the visible Windows desktop from SSH:** user auto-logon and the scheduled
  task "OLC Run Orbiter", which runs only on command and executes
  [`tools/stands/run-orbiter.ps1`](../tools/stands/run-orbiter.ps1) in the user's session.
  The agent puts the job (which Orbiter copies to start with which switches, when to take screenshots) into
  `C:\olc\run-orbiter.json` (the `window` field places the copies' windows side by side, the `ui` field presses
  buttons and selects items by name, closes the simulation window; the `key` step brings the Orbiter window to the front and holds a key by scan code for the given time, for example `{"at":70,"key":"0x4E","seconds":5}`, several keys at once — joined with `+`: `"0x1D+0x38+0x16"` (Ctrl+Alt+U); the `instance` field (the copy number from zero, in the order of `instances`) sends a `key` or `close` step to that copy, without it — to the first simulation window found — this is how the "Connect" button in
  Launchpad and reconnecting are checked); the result is
  a log and screenshots in `C:\olc\run-orbiter`. The log notes after how many seconds a copy
  exited by itself; the `timeout` field counts from the start of the copies, so with `-T` it must be
  greater than `-T` plus loading (up to 20 s), otherwise the copy is stopped before its limit. The task
  is created once by [`tools/stands/register-run-task.ps1`](../tools/stands/register-run-task.ps1)
  with the machine owner's consent.
- **Linux:** starting in the user's graphical session (the `DISPLAY` variable).

## 7. Virtualization specifics

- **A virtual machine without GPU passthrough** (typical for Proxmox and other server
  hypervisors). There is no proper 3D: D3D9Client on Windows will most likely not start or
  will be very slow; Wine with DXVK needs Vulkan. Such a machine is for a host without graphics,
  builds and network tests. With a passed-through graphics card (PCI passthrough) it becomes
  a full client. Checked on Proxmox (2026-11-17): Windows 10 and 11 see only the
  Microsoft Basic Display Adapter; Orbiter without graphics works and sends 30 states per
  second — x64 without emulation.
- **Windows over SSH.** Turning on Windows features (DISM, for example .NET Framework 3.5)
  over SSH is denied even to an administrator. Such steps are done in PowerShell on the
  machine itself or with a one-off scheduled task running as SYSTEM.
- **Windows 11 ARM.** Orbiter x64 runs through Windows' built-in x64 emulation:
  good for development and checking features, not for FPS measurements. Checked in Parallels with
  3D acceleration (2026-11-17): D3D9Client works on the Parallels Display Adapter, two
  copies with graphics side by side — about 60 frames per second on the client and about 22 on the host
  playing back a flight recording.
- **Screen scaling.** In Parallels on a Retina screen Windows often has 200% scaling: the Orbiter
  window size and the window coordinates in `run-orbiter.json` are in logical pixels.
- **Linux ARM.** Orbiter cannot run (Wine needs x86 emulation), but the OLC network core does not
  depend on Orbiter — the machine is fine as the second one in network tests.
- **Network.** Best of all when the host and the client are in one network segment: a bridge in
  virtual machines, a shared bridge on the hypervisor, one switch for physical machines.
- **A shared network with NAT** (shared network in Parallels — the only option when the Mac is on
  Wi-Fi). Virtual machines see each other, the Mac and the outside network, but outside machines cannot
  connect to them. The host for network tests is placed outside the NAT, the clients inside:
  the client starts the exchange, and the host's replies pass through the NAT.

## 8. Preparing machines

For any machine:

1. User `olc`; SSH login with a key only.
2. The machine name in `~/.ssh/config` and an entry in `stands.yaml`.
3. Network: the machine is in a common segment with the other test rigs.
4. Firewall: incoming SSH (22) and OLC (TCP and UDP `47800`) only from the local network.
5. A snapshot after preparation — a "clean" point to roll back to.

### 8.1. Windows, build (`build`)

Items 1–3 and 6, and the automatic start of `sshd`, are done by the script
[`tools/stands/setup-windows.ps1`](../tools/stands/setup-windows.ps1): it is run in
PowerShell as administrator (`powershell -ExecutionPolicy Bypass -File .\setup-windows.ps1`),
running it again is safe. Turning on OpenSSH Server and the key remain manual.

1. Visual Studio 2022 Community with the "Desktop development with C++" workload. On the
   Individual Components tab tick "C++ MFC for latest build tools (x86 & x64)". On Windows
   ARM also "MSVC … C++ x64/x86 build tools": there the x64 build tools are not installed
   by default.
2. DirectX SDK (June 2010) — per [QUICKSTART.md](QUICKSTART.md). Its installer needs .NET Framework 3.5:
   a clean Windows 10 and 11 do not have it, the script turns it on itself.
3. Git for Windows. CMake is not needed separately: it comes with Visual Studio.
4. OpenSSH Server: "Settings → System → Optional features → OpenSSH Server", the `sshd`
   service — automatic start. The key for an administrator user goes into
   `C:\ProgramData\ssh\administrators_authorized_keys`, for an ordinary user into
   `C:\Users\olc\.ssh\authorized_keys`.
5. Auto-logon of the `olc` user — so that Orbiter can be started on the visible
   desktop (section 6). The script [`tools/stands/setup-autologon.ps1`](../tools/stands/setup-autologon.ps1)
   is run by the machine owner: it asks for the password, checks it and writes it to the registry
   in plain text — for test machines only. To turn it off — the `-Disable` switch.
6. Firewall — a rule for TCP and UDP `47800` on the private network.

Virtual machine: 4 cores or more, 8 GB of memory or more, 3D acceleration on, network —
bridge. On a server hypervisor — VirtIO drivers (disk, network) during installation.

### 8.2. Windows, run only (`client`, `host`, `headless-host`)

1. For graphics — a graphics card with DirectX 9 and a current driver.
2. The Visual C++ 2015–2022 x64 Redistributable and the DirectX End-User Runtime (June 2010):
   D3D9Client needs the `d3dx9` libraries. Visual Studio is not needed.
3. OpenSSH Server, the `olc` user, auto-logon, firewall — as in 8.1.

Schools often still have Windows 10 — it is worth checking on a par with Windows 11.

### 8.3. Linux, client via Wine (`client`, `host`)

Not checked: an outline from the Orbiter Wiki.


1. A graphics card with a Vulkan driver (Mesa for AMD and Intel, the proprietary one for NVIDIA).
2. Wine from the WineHQ repository (stable), `winetricks`; into the Wine prefix — `vcrun2022` and
   `d3dx9`; DXVK to translate DirectX 9 into Vulkan. The exact set — per
   https://orbiterwiki.org/wiki/Running_Orbiter_under_Wine, refined on the first test rig.
3. A graphical session with auto-logon of the `olc` user — to start Orbiter from SSH.
4. `openssh-server`, the key, the firewall — as for any machine.

### 8.4. Steam Deck (`client`)

Not checked.


1. Desktop mode, a password for the `deck` user, `sshd` is enabled with
   `sudo systemctl enable --now sshd`.
2. Orbiter is added to Steam as a non-Steam game and runs through Proton.

### 8.5. Linux or macOS for the network core (`core-tests`)

1. A C++ compiler, CMake and Git (on Debian and Ubuntu —
   `sudo apt install build-essential cmake git openssh-server`).
2. Network — bridge, if it is a virtual machine.

What we check: building and testing `olc-core`, exchanging packets with another machine over a real
network. Orbiter is not run here.

## 9. Orbiter on test rigs

- Orbiter is built **once** — on the machine with the `build` role — per [QUICKSTART.md](QUICKSTART.md).
- The finished `install` folder is copied to the other machines together with the SPICE kernels (or the
  test package is installed on them, [PACKAGE.md](PACKAGE.md)); planet textures are optional. Its path on
  each machine is the `orbiter` field in `stands.yaml`.
- All machines have the same Orbiter build and the same set of add-ons: this is an OLC
  requirement (decision 17 in [DECISIONS.md](DECISIONS.md)).
- The plugin need not be built on a test rig: the finished `OrbiterLanCoop.dll` is an artifact of the
  GitHub Actions build (decision 25).

## 10. Network check: `olc-netcheck`

The utility from `tools/netcheck/` builds together with the core (`cmake --build orbiter-lan-coop.out`)
and runs on the same network code as the plugin. It does not need Orbiter. If it works
between two machines, neither addresses nor the firewall will get in the plugin's way. It checks NAT
only partly: its server replies to where the packet came from, while the OLC host sends `WORLD_STATE`
by itself — to the address learned from the client's `PING` (`PROTOCOL.md`, section 6).

On the host machine:

```bash
orbiter-lan-coop.out/tools/netcheck/olc-netcheck server
```

On the client machine (host address, port, duration in seconds, PING rate per second):

```bash
orbiter-lan-coop.out/tools/netcheck/olc-netcheck client 192.168.1.10 47800 300 30
```

The client goes through the `HELLO` → `WELCOME` handshake over TCP, then sends UDP `PING` at the
`WORLD_STATE` rate and prints the loss, the round trip time (RTT: minimum, mean, percentiles
50/95/99, maximum), the jitter and the share of replies later than 60 and 200 ms. Precision is about 1 ms: the polling
loop sleeps for 1 ms.

**How to read it.** The one-way delay is roughly half the RTT. The smoothing buffer on the
client is the display delay `DisplayDelay`: 30 ms for a wired LAN, 100 ms for Wi-Fi (decision 36,
[SMOOTHING.md](SMOOTHING.md)); a packet is late if it travels one way longer than that. So the share of
RTT above 60 ms (wire) or 200 ms (Wi-Fi) is roughly the share of frames in which the client will
extrapolate instead of interpolating.

### Measurements

| Date | Path | Loss | RTT p50 / p95 / p99 / max, ms | RTT > 200 ms |
|---|---|---|---|---|
| 2026-11-17 | Parallels VM (shared network, NAT) → Mac over Wi-Fi → wired Proxmox VM; 30 Hz, 300 s | 0 of 9000 | 5.0 / 80.8 / 100.2 / 186.3 | 0% |
| 2026-11-17 | Mac over Wi-Fi → wired Proxmox VM; 30 Hz, 30 s | 0 of 900 | mean 14.8, max 141.7 | — |
| 2026-11-17 | Proxmox VM → Proxmox VM, wired, one network; OLC traffic (Orbiter, 30 Hz), 420 s | 0 of 12836 | median 0.5, max 1.8 | 0% |

Conclusions:

- The Parallels shared network NAT adds about 2 ms; Wi-Fi is what makes the delay swing.
- Even over Wi-Fi the 100 ms buffer was enough: by the estimate (half the RTT) no packet
  travelled one way longer than ~95 ms.
  But the margin is small (p99 RTT is 100 ms). Over a wire the RTT is under 2 ms; the stage 1
  error over Wi-Fi and over a wire is almost the same — `PLAN.md`, stage 1.
- The jitter over Wi-Fi is not even, as in the smoothing model, but has rare long
  spikes — the model did not check this.

## 11. Vessels on a schedule: `OlcRigTest`

To check without a person how OLC sends vessels created and deleted during the session
(stage 2), the helper plugin `OlcRigTest.dll` from `tools/rigtest/` runs on the host next to OLC.
It is not part of OLC: it builds together with the plugin (Windows, the SDK folder) and is
loaded only explicitly — with the `-p OlcRigTest` switch.

At session start the plugin reads `Config\OlcRigTest.cfg`, one action per line:

```text
; <sim time, s> create <name> <class> <existing vessel>
; <sim time, s> delete <name>
; <sim time, s> dock <vessel> <its port> <other vessel> <its port>
; <sim time, s> undock <vessel> <its port>
; <sim time, s> focus <vessel>
; <sim time, s> nav <vessel> <NAV receiver> <channel>
; <sim time, s> xpdr <vessel> <channel>
; <sim time, s> shownav <vessel>
60 create GL-NEW DeltaGlider Ref
60 delete Ref
90 delete GL-NEW
```

A new vessel takes the state of the existing one shifted by 200 m, so that they do not collide.
`focus` moves the focus to the vessel, as F3 does; this is needed on a client: the F3 window is drawn
by ImGui, and the rig scripts cannot click in it.
`nav` and `xpdr` tune a NAV receiver (from 0) and the transponder, `shownav` writes the vessel's
channels to the log: this checks that frequencies reach the other machines.
The class must be installed, otherwise Orbiter terminates abnormally.

Test scenarios need a `PRPLEVEL` line, otherwise vessels start with empty tanks.

A Lua script in the scenario (`Script` in `BEGIN_ENVIRONMENT`) does not do for this: in a session
without graphics started over SSH, Orbiter hangs when the script gets to creating and deleting
vessels (`oapi.create_vessel`, `oapi.del_vessel`; checked 2026-11-17, without OLC too).

## 12. The author's rig

For history: what the first version was made on (November 2026). Everyone has their own machines and
describes them in `stands.yaml`.

| Machine | System | What for |
|---|---|---|
| Mac | macOS | The AI agent session, the network core and tests, documents and PDFs |
| A Parallels virtual machine on the Mac | Windows 11 ARM, 3D | Orbiter with graphics: clients, the flight director, two copies side by side; screenshots |
| A Parallels virtual machine on the Mac | Ubuntu ARM | Network core tests on ARM |
| A Proxmox virtual machine (a home server) | Windows 11 x64 | Building Orbiter and the plugin, the server without graphics, building the test package |
| A Proxmox virtual machine | Windows 10 x64 | A pilot without graphics, a clean installation of the package |
| Proxmox virtual machines | Ubuntu x86, Debian x86 | Network core tests with GCC, network checks |

No graphics cards were passed through on Proxmox, so everything with graphics was checked on Windows 11
ARM, where Orbiter x64 runs through emulation. The agent worked with the machines only over SSH and saw
the picture through `run-orbiter.ps1` screenshots. Network: the Mac on Wi-Fi, the Proxmox machines wired
in one network, the Parallels machines behind NAT. The first version had no hardware Windows machine
with a real graphics card: the next one will be an ordinary computer with integrated graphics.
