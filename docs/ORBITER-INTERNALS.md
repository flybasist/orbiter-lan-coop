**English** · [Русский](ru/ORBITER-INTERNALS.md)

# Map of the Orbiter core

This describes only what OLC relies on. It is not a full description of Orbiter.

- **Base commit:** `a7a3396e6c3104a05fd5ac55736de75e4c0e921f` (main, 2026-11-12).
  All references like `Src/Orbiter/Orbiter.cpp:1089` are paths and lines in this commit.
- **Where to read:** a clone of `orbitersim/orbiter` (the owner's is `~/Documents/github/orbiter`).
  The clone is read-only: nothing is changed in it, it is only updated from GitHub.
- **When the base commit changes**, the lines are checked again and this file is updated
  (section 13).
- **How it was checked.** Everything below comes from reading the code, without running it. The note "check at
  stage 1" marks a conclusion from the code that needs to be confirmed in a running Orbiter.
- **What to call from the plugin for a specific task** — in [SDK-CHEATSHEET.md](SDK-CHEATSHEET.md).

## 1. Build and launch

| What | Where | Why OLC cares |
|---|---|---|
| `Orbiter.exe` is built with `NOGRAPHICS`: graphics is connected as a separate module (D3D9Client) | `Src/Orbiter/CMakeLists.txt:163` | It can run without graphics |
| Without a graphics client the "Orbiter Server Console" opens | `Src/Orbiter/console_ng.cpp:39`, `Src/Orbiter/Orbiter.h:364` | A candidate for a host without a monitor (a virtual machine without a graphics card) |
| `Orbitersdk.lib` is a static library | `Src/Orbitersdk/CMakeLists.txt:9` | Its code goes into the plugin DLL (license, see `CREDITS.md`) |
| Build presets for Windows and for Linux via `winegcc` | `CMakePresets.json` | A possible Linux build in the future |
| Unit tests on CTest | `Tests/` | An example of how to test independent code |

## 2. Global objects

The core is built on global variables. Important for the plugin:

| Variable | What it is |
|---|---|
| `g_psys` | The planetary system: all bodies and vessels |
| `g_focusobj` | The vessel in focus. About 190 uses in 19 files: the core is designed for one player |
| `td` (`TimeData`) | Simulation time and warp, `Src/Orbiter/TimeData.h:8` |
| `g_pOrbiter` | The main application object |
| `g_bStateUpdate` | `true` only inside the world update phase (section 3) |

## 3. The simulation frame

`Orbiter::SingleFrame` — `Src/Orbiter/Orbiter.cpp:1089`:

1. `BeginTimeStep` (`Orbiter.cpp:1740`): handle a pause request, compute the step.
   `td.BeginStep` (`:1774`) computes the new time `SimT1 = SimT0 + SimDT`.
2. `UpdateWorld` (`Orbiter.cpp:1946`):
   1. `ModulePreStep` (`:1949`, implementation `:1916`) — `clbkPreStep` of vessels, then of plugins.
      `g_bStateUpdate = false`;
   2. `g_bStateUpdate = true`; during recording playback — `FRecorder_Play`;
   3. `PlanetarySystem::Update` (`Src/Orbiter/Psys.cpp:815`) — bodies, forces, vessels;
   4. `ModulePostStep` (`:1960`, implementation `:1931`) — `clbkPostStep` of vessels, then of plugins.
      **Still inside the update phase**;
   5. `g_bStateUpdate = false`, `KillVessels` — deleting vessels marked for deletion.
3. `EndTimeStep` (`Orbiter.cpp:1782`): `FinaliseUpdate` (`Psys.cpp:827`) — swapping the state
   buffers, touchdown and landing checks, vessel collisions, auto-docking, flight recording.
   `td.EndStep` (`:1790`) — `SimT0 = SimT1`.
4. `UserInput` (`Orbiter.cpp:1980`) — keyboard, mouse, joystick (section 7).
5. Rendering.

Input is processed **after** the physics, so it affects the next frame.

### Two state buffers — the main rule for OLC

Every body has two state buffers `s0` and `s1` (`Src/Orbiter/Body.cpp:36-38`):

- **Outside the update phase** (including in `clbkPreStep`) `s0` and `s1` are one and the same buffer.
- **Inside the phase** (`BeginStateUpdate`, `Body.cpp:163`) `s0` is the old state,
  `s1` is the new one being computed.
- **At the end of the frame** `EndStateUpdate` (`Body.cpp:169`) makes `s1` the current one,
  and the old `s0` is thrown away.

Consequences confirmed by reading the code:

- `InitOrbiting` writes attitude and rotation into `s0` (`Vessel.cpp:3670-3675`), and position and
  velocity into `s1` (`Body::RPlace`, `Body.cpp:92-98`). **If `DefSetStateEx` is called in
  `clbkPostStep`, the position is kept, but attitude and rotation are lost.**
  `SetGlobalOrientation`, `SetRotationMatrix`, `SetAngularVel` write only into `s0`, so
  in `clbkPostStep` they have no effect at all.
- `GetStatusEx` and `GPos()` read `s0` (`Body.h:57`, `Vesselstatus.cpp:667`), and
  `oapiGetSimTime` returns `td.SimT0` (`OrbiterAPI.cpp:984`). In both callbacks they
  agree: this is the state at moment `SimT0`, that is, at the start of the current step.
- `InitLanded` is explicitly forbidden inside the update phase (`dCHECK(!g_bStateUpdate)`,
  `Vessel.cpp:3510`).

**Conclusion: both capturing the state on the host and imposing it on the client must happen in `clbkPreStep`.**
Check at stage 1.

## 4. Time

- The step comes from the real clock (`std::chrono::steady_clock`) — `Orbiter.cpp:1740`.
  The first three frames after loading are a fixed 10 ms, the step is capped at 0.1 s.
  So two computers never compute the same way: deterministic lockstep is impossible.
- `oapiGetSysTime` is the sum of these steps (`td.SysT0`, `OrbiterAPI.cpp:994`), so a frame longer
  than 0.1 s counts as 0.1 s there; latency measurements need the real clock.
- Warp — `Orbiter::SetWarpFactor` (`Orbiter.cpp:1363`) → `TimeData::SetWarp`
  (`TimeData.h:31`). A smooth transition (`TWarpDelay`, `TimeData.cpp:55-63`) happens only with
  a non-zero delay; `oapiSetTimeAcceleration` passes a delay of 0 (`OrbiterAPI.cpp:1034-1037`),
  that is, it changes the warp at once (checked on a test rig on 2026-11-17).
- Pause — `Orbiter::Pause` (`Orbiter.cpp:1189`): the request is applied at the start of the next frame.
  While paused, `clbkPreStep` and `clbkPostStep` are not called (`Orbiter.cpp:1949, 1960`).
- A time jump — `Orbiter::Timejump` (`Orbiter.cpp:1820`), for vessels —
  `PlanetarySystem::Timejump` (`Psys.cpp:835`).

SDK functions (`Orbitersdk/include/OrbiterAPI.h`):

| Function | Line | Note |
|---|---|---|
| `oapiGetSimTime`, `oapiGetSimStep`, `oapiGetSysTime`, `oapiGetSimMJD` | 4239–4273 | Reading time; `oapiGetSimTime` = `SimT0` |
| `oapiSetSimMJD (mjd, pmode)` | 4314 | A time jump — only for large differences |
| `oapiGetTimeAcceleration`, `oapiSetTimeAcceleration` | 4331, 4341 | Warp |
| `oapiGetPause`, `oapiSetPause` | 4354, 4361 | Pause |

## 5. Vessels: imposing a state

### Reading

`VESSEL::GetStatusEx` (`VesselAPI.h:611`) → `Vessel::GetState2` (`Vesselstatus.cpp:667`).
The `VESSELSTATUS2` structure: the reference body, position and velocity relative to it (in the
ecliptic frame), attitude (Euler angles), rotation, status (0 — flying, 1 — landed),
for a landed vessel — longitude, latitude, heading, base and pad. With the flags `VS_FUELLIST`,
`VS_THRUSTLIST`, `VS_DOCKINFOLIST` the core allocates the lists itself, the caller frees them.

### Writing: `DefSetStateEx`

`VESSEL::DefSetStateEx` (`Vessel.cpp:9800`) → `Vessel::SetStateEx` (`:8193`) →
`Vessel::SetState2` (`Vesselstatus.cpp:510`).

Always, on every call:

- the reference body changes directly (`:511-512`), `landtgt`, `nport`, `lstatus`
  (the link to a base and pad, `:518-520`) are reset, the orbital elements are recomputed (`:539`);
- at the end, if collisions are on and the position changed, `MarkCollisionOverlapDirty`
  (`:601-602`): the hull overlap check with other vessels is postponed. It is done by
  `FlushPendingCollisionOverlapStates` (`Vessel.cpp:5895-5921`) at the start of the collision
  check in `Vessel::PostUpdate` (`Vessel.cpp:7154`), once per step for all
  marked vessels. Overlapping pairs are temporarily excluded from collisions. Before
  commit `b0c518ab` the check ran at once on every call — now frequent `DefSetStateEx`
  calls on the client do not cost a full iteration over vessels.

For status 0 (flying) — `InitOrbiting` (`Vessel.cpp:3647-3680`):

- if the point is below the terrain, the vessel is **lifted to the surface** (a terrain elevation query,
  `:3656-3664`);
- attitude and rotation go into `s0` (section 3);
- `RPlace` → `RPlace_individual` (`Vessel.cpp:884`): the "flying" status, clearing surface
  contact, recomputing gravity, `UpdateAttachments`, `UpdateProxies` (iterating over all bodies,
  bases and vessels), `UpdateSurfParams`. For a docked vessel `RPlace` goes to the
  superstructure (section 9).

It does not touch: autopilot modes (`navmode`), accumulated forces, thruster levels and fuel
(only with the `VS_THRUSTLIST`, `VS_FUELLIST` flags). It sends no events to plugins and writes nothing to the log.

The cost of a call from reading the code: recomputing elements and gravity, iterating over bodies and bases, a terrain
elevation query and a check of neighbouring vessels' hulls. For several vessels at 20–30 Hz
it is most likely acceptable; vessels standing close (docking, formation) are more expensive. Check at stage 1.

### What happens to a vessel between two calls

The vessel is not "frozen": the core integrates it one step from the imposed state
(gravity, thrust, aerodynamics, ground contact) — `Vessel::Update` (`Vessel.cpp:6857`),
`RigidBody::Update` (`Rigidbody.cpp:182`). The integrator keeps no step history, so
the imposed state does not conflict with it. The next call overwrites the result.

What may argue with the imposed state:

- **Vessel collisions** (`PostUpdate`, `bVesselCollision`). Turned off on the client
  (`VesselCollision = FALSE`).
- **Auto-docking**: if the ports of two vessels end up close, the client's core docks them itself.
  The SDK has no switch for it.
- **Ground contact**: the reaction forces of the legs of a vessel right at the surface.
- **Auto-landing** (section 6).

### Light setters

| SDK function | Line in `VesselAPI.h` | What it does |
|---|---|---|
| `SetGlobalOrientation` | 802 | Attitude in `s0`, takes the superstructure into account. Without `InitOrbiting` |
| `SetRotationMatrix` | 4238 | The same, with a matrix |
| `SetAngularVel` | 764 | Only `s0->omega` |

Position and velocity cannot be set separately: the SDK has no public `SetGlobalPos`/`SetRelVel`,
only `DefSetStateEx`. `ShiftCentreOfMass` and `ShiftCG` are not state setters but a shift
of the centre of mass.

## 6. A vessel on the surface

- The status in the core is `fstatus`: `FLIGHTSTATUS_FREEFLIGHT` or `FLIGHTSTATUS_LANDED`.
- `SetState2` with status 1 (`Vesselstatus.cpp:573-593`) → `InitLanded`
  (`Vessel.cpp:3508-3595`). It puts the vessel at a point by longitude, latitude and heading and computes
  the equilibrium position on the legs. If a base is given, it occupies a pad (`OccupyPad`) and
  tells the base about the touchdown.
- A landed vessel is not integrated: every frame the core puts it at the same point on
  the rotating planet (`Vessel.cpp:6883-6910`). It "lifts off" only from forces — thrust,
  a collision (`bForceActive`).
- **Auto-landing** (`CheckLanded`, `Vesselbase.cpp:244-300`): a vessel that touches the
  surface, without thrust and almost motionless for 5 s, is switched by the core itself to the "landed" status.
  This will also trigger on the client for a vessel on which a motionless state is imposed.
- The core has no crashes or destruction: only dynamic ground contact (by default
  `bDynamicGroundContact = true`, `Vesselbase.cpp:199`).

For OLC:

- do not impose status 1 every frame: set it once, when the host reports the landing;
- drive a vessel rolling along the runway as a flying one (status 0);
- the `SetState2` code has a latent bug `if (proxybase = landtgt)` (assignment instead of
  comparison, `Vesselstatus.cpp:595`). It does not affect us, but could be a separate PR.

## 7. Input

### Processing order

`Orbiter::UserInput` (`Orbiter.cpp:1980`):

1. Clear `simkstate` and the `ctrlKeyboard`, `ctrlJoystick` arrays (`:1989-1990`).
2. Read the DirectInput keyboard into `simkstate` (`:2014`).
3. `clbkProcessKeyboardImmediate` of all plugins (`BroadcastImmediateKeyboardEvent`, `:2015`, implementation `:2480`).
   `simkstate` itself is passed, not a copy. If at least one returned `true`, standard processing
   of held keys is skipped **for the whole frame** (`:2015-2019`).
4. Single presses: `clbkProcessKeyboardBuffered` of plugins (`:2026`, implementation `:2491`), on press only;
   a consumed key is removed from the queue.
5. Joystick: `PollJoystick`, then `UserJoyInput_OnRunning` (`:2039`, `:2413`).
6. The sum of keyboard and joystick → `g_focusobj->ApplyUserAttitudeControls` (`:2048`).

All standard control is addressed **only to the vessel in focus**.

### Held keys

`KbdInputImmediate_OnRunning` (`Orbiter.cpp:2204`) first gives the keys to the vessel
(`ConsumeDirectKey` → `VESSEL2::clbkConsumeDirectKey`), and only then processes them itself.
The table mapping keys to SDK calls is in [SDK-CHEATSHEET.md](SDK-CHEATSHEET.md), section 3.

Important details:

- RCS keys do not "ramp up": in a frame while the key is held the level is 1.0 (or 0.1 with Ctrl).
  This is an `override` that is reset after every step (`Vessel.cpp:4083`).
  The exact SDK counterpart is `IncThrusterGroupLevel_SingleStep`.
- `SetAttitudeRotLevel` and `SetAttitudeLinLevel` write a **permanent** level: it must be
  reset when the key is released.
- Control surfaces: the core writes into a "temporary" target, while `SetControlSurfaceLevel` from the SDK
  always writes into the permanent one, overwriting the trim (`Vessel.cpp:10875`). We will have to drive the trim ourselves.
- **Held keys cannot be delivered to `clbkConsumeDirectKey` of another vessel.**
  Add-ons' own held keys (not the standard ones) will not work on the host.

### Single presses

`VESSEL::SendBufferedKey` (`VesselAPI.h:3265`, `Vessel.cpp:11029`) →
`Vessel::ConsumeBufferedKey` (`Vessel.cpp:3796`), on **any** vessel:

- first `clbkConsumeBufferedKey` of the vessel itself (add-on keys);
- then the vessel's standard processing: autopilot modes, RCS on and mode, undocking,
  aerodynamic control mode (Alt+Numpad/), a clearance request (Ctrl+C);
- HUD keys are here too, but they act on the focus vessel's HUD — do not forward them.

`oapiSimulateBufferedKey` and `oapiSimulateImmediateKey` work only with the focus vessel.

### Joystick

- The core opens the joystick in `DISCL_EXCLUSIVE | DISCL_FOREGROUND` mode
  (`Src/Orbiter/Di7frame.cpp:190-191`), only if it is set in the settings.
- There is no callback for plugins. But the polling result is available: on the client
  `VESSEL::GetManualControlLevel (group, MANCTRL_ANYMODE, MANCTRL_JOYSTICK)` returns
  the stick deflection per RCS group (`VesselAPI.h:3093`, `Vessel.cpp:8587`).
  The throttle lever sets a permanent main engine level of the focus vessel
  (`Orbiter.cpp:2435-2445`), it is visible through `GetThrusterGroupLevel`.
- A plugin's own DirectInput polling will most likely not get the device while the core's joystick
  is on (exclusive mode). Not checked.

### `oapiGetKeyState`

`OrbiterAPI.h:6695`, implementation `OrbiterAPI.cpp:2492`: returns the same `simkstate`.
In `clbkPreStep` this is the state of the **previous frame**: `UserInput` comes after `UpdateWorld`.
Empty when an ImGui window has taken the keyboard.

### Mouse and MFD

- `clbkProcessMouse` is called before the core; `true` blocks the event for the core.
- The SDK's MFD functions (`oapiSendMFDKey`, `oapiProcessMFDButton`, `OrbiterAPI.h:5127, 5158`)
  work with the MFDs of the focus vessel. For the "crew" mode the second participant's MFD is their
  own Orbiter instance, where this vessel is in focus.

## 8. Scenarios, sessions, creating vessels

### Scenarios

- `oapiSaveScenario (fname, desc)` (`OrbiterAPI.h:6375`, `Orbiter.cpp:1453`) writes **only to
  a file** in the `Scenarios` folder. Contents: description, system and date, focus, camera, panel,
  all vessels (`Vessel::Write`, `Vessel.cpp:8309`: name, class, `clbkSaveState` lines).
- **A scenario cannot be loaded from a plugin at run time through the SDK.** There is no function;
  the internal `Orbiter::Launch` (`Orbiter.cpp:679`) is called only from Launchpad and from the
  command line (`-s`). `oapiExitOrbiter` simply ends the process. But the plugin can
  close the session and press "Launch" in Launchpad itself — checked on a test rig, section 18.

### Plugin life

- Plugins are loaded once when Orbiter starts and live between sessions. Every session is
  a pair of `clbkSimulationStart` / `clbkSimulationEnd` on the same module object. All session
  state (vessel handles, tables) is reset in these callbacks.
- `clbkSimulationEnd` is called **after** the world is destroyed (`Orbiter.cpp:933`, after
  `DestroyWorld`), contrary to the description in `ModuleAPI.h:127`. Vessels must not be touched in it.
- `clbkNewVessel` is not called for vessels from the scenario, only for those created at
  run time, and before their setup is finished (`Orbiter.cpp:1253-1270`).

### Creating and deleting

- `oapiCreateVesselEx (name, classname, VESSELSTATUS2*)` (`OrbiterAPI.h:2868`) creates
  at once, the handle is valid immediately.
- **If there is no class file, Orbiter terminates abnormally** (`TerminateOnError` → `exit(1)`,
  `Vessel.cpp:129-130`). The client must check that `Config\Vessels\<class>.cfg` exists before
  creating.
- An add-on's internal state (the `clbkSaveState` lines) cannot be passed to a new vessel:
  `clbkLoadStateEx` is called only when a scenario is read.
- `oapiDeleteVessel` (`OrbiterAPI.h:2886`) only marks the vessel; deletion happens at the end of the frame
  (`KillVessels`, `Orbiter.cpp:1277`). Deleting the last vessel that can have the
  focus closes the session.

### How to recognize "the same vessel"

- `OBJHANDLE` is just a pointer inside the process (`OrbiterAPI.h:145`). It is not sent over the network.
- Vessels are matched by **name** (`oapiGetVesselByName`, case-insensitive search,
  `Psys.cpp:121-126`). The core does not check name uniqueness, and a vessel cannot be renamed from the SDK.
  The host makes sure names are unique itself.
- The reference body, base and docking target in `VESSELSTATUS2` are handles too; their names go over the network.

## 9. Docking and superstructures

- Docked vessels are merged into a superstructure (`Src/Orbiter/SuperVessel.h`): it is
  integrated as one body, and the positions of the parts are derived from it.
- `DefSetStateEx` on a part of a superstructure: position and velocity move **the whole assembly**
  (`SuperVessel::RPlace`, `SuperVessel.cpp:309-331`), while the attitude is written only into the part and
  overwritten on the next step. The assembly's attitude is set by `SetGlobalOrientation` and
  `SetAngularVel` — they take the superstructure into account. Checked on the stand (stage 3.5): the client flies the stack through one vessel and leaves the others alone.
- `VS_DOCKINFOLIST` in `DefSetStateEx` at run time only rearranges pointers and does not
  create a superstructure — do not use it.
- `VESSEL::Dock (target, n, tgtn, mode)` and `Undock (n)` (`VesselAPI.h:4520, 4535`) work
  on any vessel. Mode 0 — no teleport, for vessels that are already aligned.
- The partner's ports — through `GetStatusEx` with `VS_DOCKINFOLIST` (`dockinfo[].idx`, `ridx`, `rvessel`).

For OLC: docking on the client is repeated only after the host's message, in mode 0, when the
vessels are already aligned; after that the assembly is driven through one vessel.

## 10. Flight recording playback

Orbiter already has "a vessel controlled from outside": the recording playback mode.

- `Vessel::FRecorder_Play` (`Src/Orbiter/FlightRecorder.cpp:479`): position and velocity
  are interpolated with a cubic polynomial between recorded points, attitude with
  quaternion interpolation.
- While playback runs (the `bFRplayback` flag), the core ignores attempts to change thruster
  levels (`Src/Orbiter/Vessel.h:441` and further), direct position setting
  (`Vessel.cpp:889`) and surface contact calculation (`Vessel.cpp:6952`).
- This mode is not available from the SDK.

For OLC this is a model of how the client should drive "foreign" vessels. If `DefSetStateEx`
turns out too crude, the natural hook for a PR to the core is "an external vessel state
source" modelled on playback.

## 11. Vessel collisions

- The new collision core is `Src/Orbiter/CollisionCore.h` (GJK and EPA for convex hulls),
  added on 25.10.2026 as the first of three parts. It is used in `Vessel.cpp:6039` and further
  (hull intersection) and `:6300-6316` (penetration depth when resolving a collision).
- Commit `b0c518ab` (07.11.2026) fixed mesh collision bugs: the hull
  is recomputed on an animation change if the animation moves part of the collider
  (`SetAnimation`, `Vessel.cpp:8055-8078`); in a superstructure stabilization is turned off right after a collision
  (`SuperVessel.cpp:790-798`); the overlap check when
  imposing a state became deferred (section 5).
- Turned on by the `VesselCollision` parameter in `Orbiter.cfg`, on by default
  (`Src/Orbiter/Config.cpp:50`, `:610`).
- There are unit tests: `Tests/Collision.CollisionCore.cpp`.

For OLC: on the host collisions are part of the shared physics. On the client they must be turned off so that
they do not argue with the host's states.

## 12. Stage 1 questions

Answers obtained by reading the code (to be confirmed in operation):

1. **How often `DefSetStateEx` can be called.** No side effects accumulating from frequent
   calls were found; the cost is the recalculations from section 5. Call it in `clbkPreStep` (section 3).
2. **A vessel on the surface.** Status 1 — once on landing, after that the core holds the vessel
   itself; a rolling vessel — like a flying one (section 6).
3. **Loading a scenario from the plugin** — impossible through the SDK (section 8); a working path through
   Launchpad was checked on a test rig (section 18).
4. **Superstructures** — position through any vessel of the assembly, attitude through
   `SetGlobalOrientation` (section 9).
5. **Joystick** — on the client it is read through `GetManualControlLevel` and the main engine
   level (section 7).

Still open:

1. The real smoothness and load of `DefSetStateEx` at 20–30 Hz.
2. The attitude behaviour of a part of a superstructure.
3. The client's own control: the client's core applies the player's input to the local copy of the vessel
   (it is in focus), and that copy has time to move before the host's state arrives. This may work as
   free "prediction" or may give jerks. Decided after stage 1.

## 13. Check against Orbiter (upstream)

Orbiter evolves, and OLC must follow it to the next official release rather than
get stuck on a random commit between releases. So the main project is checked regularly.

### When

- The AI agent — on the first scheduled run of every working day;
- the owner — when they have updated the clone themselves;
- always — before the start of every stage and when an Orbiter release comes out.

### How

1. Update the clone: `git -C ~/Documents/github/orbiter pull --ff-only`. Only if the clone is clean
   (`git status` is empty) and on the `main` branch; otherwise do not touch it and ask the owner.
2. See what is new: `git log --oneline <base>..HEAD` and `git diff --stat <base>..HEAD`.
3. Pick what matters for OLC: `Orbitersdk/include/`, `Src/Orbiter/` (frame, time, input,
   vessel state, superstructures, collisions, scenarios, Launchpad, command line),
   `CMakeLists.txt` and `CMakePresets.json`, and also new releases, tags and milestones on GitHub.
4. Sort it out:
   - **nothing important** (documentation, CI, other people's add-ons) — move the base commit;
   - **places from this map are affected** — check the line references in this file and in
     `SDK-CHEATSHEET.md`, update the conclusions, then move the base commit;
   - **the concept changes** (a new API that replaces our workaround; an SDK change
     breaking compatibility; a maintainers' decision contradicting the OLC plan) — a question
     to the owner in `QUESTIONS.md` and an entry in `DECISIONS.md` after the answer.
5. Moving the base commit: replace the hash in both language versions of `README.md` from `docs/`
   (condition 3), this file, `SDK-CHEATSHEET.md`, `QUICKSTART.md`, `CREDITS.md`, the `ORBITER_COMMIT` variable in
   `.github/workflows/build.yml` (the full hash) and add a line to the log below.

### Check log

| Date | Range | What came in | Effect on OLC | New base commit |
|---|---|---|---|---|
| 2026-11-07 | `9721ab8d..b9bd0785` | 3 commits: automatic changelog, `.github/workflows/` files | None: code and SDK did not change, line references are the same | `b9bd0785` |
| 2026-11-08 | `b9bd0785..b0c518ab` | 1 commit: `[CORE] Hotfix mesh collider bugs (#726)` — `Vessel.cpp`, `Vessel.h`, `Vesselstatus.cpp`, `SuperVessel.cpp`, D3D9Client | The map is affected: line references for these files were recomputed; `SetState2` defers the overlap check (section 5), `SetAnimation` recomputes the collision hull (sections 11, 17). The SDK did not change, the OLC concept is the same | `b0c518ab` |
| 2026-11-09 | `b0c518ab..b0c518ab` | No new commits; no new tags or releases | None | `b0c518ab` |
| 2026-11-09 | `b0c518ab..dabade26` | A second check on the same day: 1 commit `Use ImGui for text annotations (#729)` — `Orbiter.cpp`, `GraphicsAPI.cpp`, `DlgMgr.cpp/.h`, `Orbitersdk/include/GraphicsAPI.h`, D3D9Client | The map is affected: `Orbiter.cpp` line references were recomputed (+12 up to line 1590, −9 after 1735); `oapiCreateAnnotation` labels are drawn through ImGui (section 16, cheat sheet). SDK: only deprecation comments, the ABI is the same; the OLC concept is the same | `dabade26` |
| 2026-11-10 | `dabade26..dabade26` | No new commits; no new tags or releases | None. A follow-up of the 2026-11-09 move: in sections 3 and 7 the short `:NNNN` references to `Orbiter.cpp` missed during the move were recomputed; the references to `BroadcastImmediateKeyboardEvent` and `BroadcastBufferedKeyboardEvent` had been wrong before too — now the call and the implementation are given; section 16: `clbkWriteConfig` — `:685-686`. The other references to `Orbiter.cpp`, `DlgMgr.cpp`, `GraphicsAPI.cpp` and in the plugin were checked by line content. A repeated check: section 14 — `Orbiter::Run` `:1027-1087`, `UserInput` in the main loop — line 1060 (the same in `PLAN.md`, I1) | `dabade26` |
| 2026-11-16 | `dabade26..a7a3396e` | 4 commits: `[CORE] Distance display improvement` (#620) — `Astro.cpp`, `Astro.h` (`SciStr` without the `prefix` parameter, scientific notation with `e`), `MenuInfoBar.cpp`; no new tags or releases | None: the distance format in the info panels; the SDK and places from the map are not affected, line references are the same | `a7a3396e` |
| 2026-11-17 | `a7a3396e..a7a3396e` | No new commits; no new tags or releases | None | `a7a3396e` |
| 2026-11-18 | `a7a3396e..a7a3396e` | No new commits; no new tags or releases | None | `a7a3396e` |
| 2026-11-19 | `a7a3396e..a7a3396e` | No new commits; no new tags or releases | None | `a7a3396e` |

## 14. The plugin outside the frame: pause, windows, threads

Research I1 (2026-11-07).

### The main loop

`Orbiter::Run` (`Orbiter.cpp:1027-1087`) does one of two things per iteration:

- if the Windows queue has a message, it processes it (`DispatchMessage`);
- otherwise, during a session, it makes a step: `BeginTimeStep` → `UpdateWorld` → `EndTimeStep` →
  `UserInput` (only if the window is active, line 1060) → `Render3DEnvironment`.

During a session the loop does not sleep — neither while paused nor with an inactive window (`PeekMessage`).
The only delay is the frame rate limiter in D3D9Client (`D3D9Client.cpp:1285-1290`)
or VSync; in console mode without graphics there is no limiter. So the host itself thins out
sending states to 20–30 Hz. In Launchpad the loop waits for messages (`GetMessage`).

### What is called while paused

| Callback | While paused |
|---|---|
| `clbkPreStep`, `clbkPostStep` | No: only with `bRunning` (`Orbiter.cpp:1949, 1960`) |
| `clbkProcessKeyboard*` | Yes, but only for the active window |
| `clbkDeleteVessel` | May come: vessel deletion runs every frame (`Orbiter.cpp:1964`) |
| Drawing ImGui windows (`ImGuiDialog::OnDraw`) | Yes, if there is a graphics client and the window is open |
| Messages to your own Windows window (`WM_TIMER`, `WM_APP`) | Yes, always: while paused, with an inactive window, in console mode |

`oapi::Module` has no per-frame callback that would work while paused with any window.
The reliable way is the plugin's own hidden window (`HWND_MESSAGE`) with a timer or
`PostMessage`: otherwise a paused client with an inactive window will not learn that the host released the pause.

### How time commands are applied

- `oapiSetPause` only remembers the request; the pause changes at the start of the next step, where
  `clbkPause` is also called (`Orbiter.cpp:1189-1193, 1742-1751`).
- `oapiSetTimeAcceleration` takes effect at once, including while paused, and calls
  `clbkTimeAccChanged` at once. A core bug: both callback arguments are the new value
  (`Orbiter.cpp:1418-1422`), so the plugin must remember the previous warp itself.
- `oapiSetSimMJD` performs the time jump at once and calls `clbkTimeJump`.

### Threads

- There are no locks around the core state. `oapi*` functions and `VESSEL` methods are called only
  from the main thread.
- An example of linking threads with the main one is the server console (`console_ng.cpp:75-78, 277-289`):
  a command queue under a mutex, parsed in the main loop.
- The OLC scheme has no separate thread, everything is in the main one (decision 23 in [DECISIONS.md](DECISIONS.md)).

### DLL life

- `InitModule` and `ExitModule` are called from `DllMain` (`Orbitersdk.cpp:24-31`), under
  the Windows loader lock. Creating a thread in them, waiting for it or calling
  `WSAStartup` is not allowed. The network and the hidden window are created in `clbkSimulationStart` and
  destroyed in `clbkSimulationEnd`.
- `clbkFocusChanged` and loading vessel state from the scenario come before
  `clbkSimulationStart`.

Not confirmed by the code: whether D3D9 draws a minimized window; whether the loop stops while
the user drags the main window (for dialogs the core compensates with a timer,
`DlgMgr.cpp:337-346`). Checked at stage 0.

## 15. Two Orbiter instances on one machine

Research I2 (2026-11-07).

- **There is no protection against a second copy.** All mutexes in the core are unnamed (`console_ng.cpp:277`,
  `Interpreter.cpp:70`), there is no search for another copy's window.
- **The root is the process's current folder**, not the exe folder. All paths are relative:
  `Orbiter.cfg`, `Orbiter.log`, `Config\`, `Scenarios\`, `Modules\Plugin\`
  (`Orbiter.cpp:72, 207, 396, 443`, `Config.cpp:35-42`).
- **There are no command line switches to choose a configuration or folders.** All switches
  (`cmdline.cpp:166-178`): `-S/--scenario`, `-s/--scenariox` (a scenario and exit after the
  session), `-x/--fastexit`, `-v/--openvideotab`, `-l/--keeplog`, `-f/--fixedstep`,
  `-T/--maxsystime`, `-t/--maxsimtime`, `--maxframes`, `-p/--plugin` (enable the plugin
  `Modules\Plugin\<name>.dll`), `-h/--help`.
- **Two instances from one folder get in each other's way:**
  - `Orbiter.cfg` is overwritten;
  - `Orbiter.log` is truncated by the second instance;
  - shared `Scenarios\(Current state).scn` and `Scenarios\Quicksave\` (each process has its own
    counter — files get overwritten);
  - the flight recording folder `Flights\` may be deleted entirely (`FlightRecorder.cpp:776-789`);
  - the D3D9Client log and cache.
- **The registry** is not written by Orbiter; it opens no network ports — there are no conflicts there.
- **Input.** Keyboard and mouse are non-exclusive and only for the window in focus (`Di7frame.cpp:151, 171`).
  The joystick is captured exclusively (`Di7frame.cpp:191`), but only if it is enabled in the
  settings; it must be turned off in the second instance.

**The procedure for stage 0:** two copies of the `install` folder (`host` and `client`), each
started from its own folder, in windowed mode. Folders Orbiter only reads
(textures, meshes, sound) need not be copied, they can be linked. How DirectInput and D3D9
behave with two processes is checked on a test rig.

**Checked on a test rig (2026-11-17, Windows 11 ARM, without a graphics client):** two copies
(`C:\olc\host`, `C:\olc\client`, a shared `orbiter.bsp` via a hard link) were started
at the same time with the `Checklists\Quickstart` scenario and the `-p OrbiterLanCoop` switch. Both
loaded the plugin, ran for 20 s and exited with code 0, the logs did not mix. With D3D9Client
it was checked the same day: two copies side by side on screen ([PLAN.md](PLAN.md), stage 1).

## 16. The plugin interface: Launchpad and windows in the simulation

Research I4 (2026-11-07).

### Launchpad

- The plugin adds an item to the "Extra" tab: the `LaunchpadItem` class
  (`OrbiterAPI.h:2284`), registered with `oapiRegisterLaunchpadItem` in `InitModule`, removed in
  `ExitModule`. `Name`, `Description`, `clbkOpen`, `clbkWriteConfig` are overridden.
- The item's window is an ordinary modal Win32 dialog from the DLL resources: there is no ImGui in Launchpad,
  it is created only for a session. Examples: `Orbitersdk/samples/LaunchpadParamTemplate`,
  `Src/Plugin/LaunchpadExtensions/AtmConfig`.
- `clbkWriteConfig` is called before every session starts (`Orbiter.cpp:685-686`): this is
  the only "a scenario is starting now" moment, and the start cannot be cancelled from it.
- **There is no public API to start a scenario**, but Launchpad is an ordinary Win32 dialog in the same
  process, and the plugin can select a scenario and press "Launch" itself (section 18, checked on a
  test rig). The fallback is restarting `orbiter.exe -s <scenario>`; the exe path is
  `GetModuleFileName(oapiGetOrbiterInstance())`, the Orbiter root is the process's current folder.

### Windows in the simulation (ImGui)

- The `ImGuiDialog` class (`OnDraw` is overridden), opened with `oapiOpenDialog(ImGuiDialog*)`.
  Drawing happens from `Render3DEnvironment` every frame, including while paused
  (`Orbiter.cpp:997-1006`).
- The example is `Src/Plugin/Framerate/Framerate.cpp`: the module inherits both `Module` and
  `ImGuiDialog`. The plugin includes `imgui.h` and links with ImGui from the Orbiter build.
- Opening a window for the player: `oapiRegisterCustomCmd` (the Ctrl+F4 list) and
  `oapiRegisterCustomMenuCmd` (a menu button). While an ImGui window has focus, it takes the
  keyboard (`Orbiter.cpp:2523-2531`).
- Catches: `oapiOpenDialog` crashes outside a session — there is no window manager
  (`OrbiterAPI.cpp:2182-2186`); the window manager is recreated in every session; without a
  graphics client there is no ImGui.
- Old Win32 dialogs (`oapiOpenDialog` with `DLGPROC`) still work, but new Orbiter windows
  are written with ImGui.

### Text without a window

- `oapiAddNotification` — pop-up notifications (connected, disconnected).
- `oapiCreateAnnotation` — permanent text on screen, only in a session and with a graphics
  client. Since commit `dabade26` the label is drawn through ImGui: `DialogManager::ImGuiNewFrame`
  goes over all labels (`DlgMgr.cpp:571-573`), so it is visible while paused too, but disappears
  together with the windows when the interface is hidden (`Orbiter.cpp:1000-1001`). The text is expected in UTF-8;
  a string that is not valid UTF-8 is treated as Windows-1252 and converted
  (`ScreenAnnotation::SetText`, `GraphicsAPI.cpp:837`). The SDK call and the ABI are the same; `GraphicsClient::clbkCreateAnnotation`
  is marked deprecated and no longer used by the core.
- `oapiDebugString` — one line for debugging; not recommended in released
  modules.

## 17. Vessel state beyond motion

Research I5 (2026-11-07). What else the client needs to see the same as the host.

### What `VESSELSTATUS2` contains and what `DefSetStateEx` does with it

- **Contains:** position, velocities, attitude, status, fuel per tank, thruster levels,
  dockings, transponder (`OrbiterAPI.h:1241-1369`).
- **Does not contain:** total and empty mass, autopilot modes, RCS mode, navigation frequencies,
  attachments (`AttachChild`), animations, lights and beacons, wheel brakes, the internal
  state of the vessel module.
- **Fuel** is stored in the status as a level 0…1 (`Vesselstatus.cpp:688-695`), the total
  mass is computed by the client itself. The empty mass (`emass`) is changed by add-ons themselves (DeltaGlider
  passengers, Atlantis cargo) — it must be sent separately.
- **Dockings** via `VS_DOCKINFOLIST` are only recorded, no assembly is created —
  this is for creating a vessel. During a session — `Dock` and `Undock` (section 9).
- `DefSetStateEx` does not call the vessel module's callbacks (`clbkLoadStateEx` and others).

### Thrusters, fuel and exhaust

- The exhaust is drawn by the level of each thruster (`Vessel.cpp:1385`), so the protocol carries
  thruster levels, not group levels.
- **The core zeroes a thruster without fuel every step** (`Vessel.cpp:4081`). If the client's copy of the
  vessel has an empty tank, there will be no exhaust. So fuel is applied before thrust
  and must not drift far from the host.
- The thrust on the client is real: the client's physics accelerates the vessel between state
  impositions (`Vessel.cpp:3904-3910, 4066-4087`). Since the state is imposed every frame,
  the error is no more than one step — this works as prediction.
- A vessel on the surface with non-zero thrust switches to flight by itself (`Vessel.cpp:6893-6909`).
- Autopilots must not be turned on for the vessel copy on the client: they add their own thrust on top of
  the imposed one.
- Engine sound comes from the XRSound add-on, not the core; it takes the thruster group levels and
  plays for vessels not in focus too.

### Animations, lights, add-on keys

- **Animations** are read and set without knowing the class (`GetAnimPtr`, `SetAnimation`,
  `Vessel.cpp:8055-8078, 10212-10226`), but only as a picture and a collision hull:
  the module's internal state (for example, "gear down" on the DeltaGlider) does not
  change from this, and it affects the physics (touchdown points, `GearSubsys.cpp:244-248`). Animation numbers have no general meaning — each
  class has its own.
- **Lights and beacons** are read and set without knowing the class (`GetBeacon`, `GetLightEmitter`,
  `VesselAPI.h:5112-5119, 5176`).
- **Repeating keys on the client** (`SendBufferedKey`) is dangerous: the gear, nose
  cone and airlock keys are toggles, and a lost press inverts the state forever.
  It is suitable only for known classes and "set state" commands (stage 3).
- **Attachments** (`AttachChild`) are not part of the status; an attached vessel moves together
  with its parent. An "attach/detach" event is sent, not a position.

### What the copy on the client can decide by itself (risk of drift)

- auto-docking (`Vessel.cpp:7167-7218`) — section 9;
- collisions and destruction at a relative speed above 500 m/s
  (`Vessel.cpp:6555-6559`) — collisions are turned off on the client;
- random DeltaGlider failures (`DeltaGlider.cpp:687-727`);
- Atlantis creates and deletes vessels itself (tank, boosters, MMU) — on the client this gives
  extra vessels. This is a stage 4 question (add-on vessels).

A module's internal state as a whole is passed only through a scenario (`clbkSaveState`);
there is no standard way to load it into an already living vessel on the client.

## 18. Starting a scenario sent by the host from the plugin (stage 2 reconnaissance)

Reconnaissance on 2026-11-17: the main risk of stage 2 is whether the client plugin can start the world
sent by the host. Checked with a throwaway plugin on a test rig (Windows 11 ARM, D3D9Client).

**What the core does when a session closes** (`Orbiter::CloseSession`, `Orbiter.cpp:889`), in
order:

1. saves the world to `Scenarios\(Current state).scn` (`Orbiter.cpp:898`);
2. shows Launchpad — when shown, it selects `(Current state)` in the list
   (`TabScenario.cpp:252`, `RefreshList`);
3. deletes the world and only then calls `clbkSimulationEnd` of the plugins (`Orbiter.cpp:933`).

The "Launch" button (`IDLAUNCH = 1000`, `resource.h:135`) takes the scenario selected in the list
(`IDC_SCN_LIST = 1090`) and calls `Orbiter::Launch` (`Launchpad.cpp:339-342`). Launchpad is
an ordinary Win32 dialog in the main thread, so the plugin can drive it itself.

**The checked path (it works):**

1. A session is running; the plugin closes it with a `WM_CLOSE` message to the simulation window (the thread's window titled
   `OpenOrbiter [...]`) — the same as the "Exit" button.
2. In `clbkSimulationEnd` the plugin writes the received scenario to
   `Scenarios\(Current state).scn`: the core has already saved its world there, and the file will not be
   overwritten again.
3. On a timer (Launchpad is already on screen) the plugin finds the Launchpad window (the thread's window with
   the `IDLAUNCH` button), among its child windows the `IDC_SCN_LIST` list, selects
   `(Current state)` in it (`TreeView_SelectItem`) and sends `WM_COMMAND IDLAUNCH`.
4. Orbiter starts a new session in the same process; the plugin stays loaded, its
   `clbkSimulationStart` is called again.

The result of the run: the second session has a vessel that was not in the world saved by the core —
it is exactly the received scenario that started. Between pressing "Launch" and the start of the session
about 17 s pass (the same as the first start: the world and textures are loaded again).

If the client connects from Launchpad (there is no session yet), steps 1–2 are not needed: the scenario
is written to the file, then step 3.

**What remains to be found out:**

- At the start of the second session D3D9Client writes `pDevice->Clear` errors to the log for about 0.6 s
  (code `0x88760868`, `D3DERR_DEVICELOST`), then the picture is normal. Whether this is the behaviour of a
  repeated session start in D3D9Client on the Parallels virtual graphics card or not —
  check on real hardware.
- The element numbers (`1000`, `1090`) and the simulation window title are not part of the SDK; when the
  base commit changes, check them against `resource.h`.
- It is better to write the host's scenario into a separate file (`Scenarios\OLC\...`) and select it in
  the list by path, so as not to overwrite the pilot's `(Current state)`; selecting by path is
  done in `RefreshList` (`TabScenario.cpp:215-233`) — the same tree walk.

**How it is done in the plugin (stage 2, checked 2026-11-17 on Windows 11 ARM with graphics):**

- The file is `Scenarios\OLC\Host world.scn`. Launchpad fills the list only when it is shown, so a
  file written later is not in it: the plugin adds the items `OLC` and `Host world` to the tree itself.
  Launchpad tells a file from a folder only by the "has children" flag (`GetSelScenario`,
  `TabScenario.cpp:541`), and builds the path from the item texts — a hand-added item works the same.
- The number `1090` is used twice in `resource.h`: `IDC_SCN_LIST` and the check box
  `IDC_OPT_VIS_SPECULAR` on another tab. The list is looked up by the number and the class
  `SysTreeView32`.
- "Launch" is pressed by a 0.3 s timer after the OLC dialog is closed, as in the reconnaissance, not
  from inside the modal dialog. The first attempt (from inside the dialog and looking up the list by the
  number only) started nothing; both places were fixed at once, which one was the cause was not found out.
  From "Connect" to the start of the session — 21 s.
- A vessel of an unknown class in a scenario crashes Orbiter the same way as `oapiCreateVesselEx`
  (`Psys.cpp`, `InitState` → `Vessel::Vessel` → `TerminateOnError`). So before starting it, the plugin
  removes from the received scenario the vessels of classes the client lacks
  (`core/scenario_filter.h`).

## 19. What the MFDs and DeltaGlider change in the vessel (reconnaissance 4.1)

Task 3.18 (2026-11-18), base commit `a7a3396e`. What the navigator does locally and what of it must
reach the pilot's vessel on the host.

### Standard MFDs

- Almost all MFDs (Orbit, Map, Align, Sync, Transfer, Landing, HSI, Surface) only **read** the vessel.
  The only writes into the vessel come from Comms and MFD Map.
- **Comms** (`MfdComms.cpp:171-172`) changes the NAV receiver frequencies (`Vessel::IncNavChannel`)
  and the transponder (`IncXpdrChannel`). From the SDK: `VESSEL::SetNavRecv`/`GetNavChannel`
  (`VesselAPI.h:5343`); a frequency is a step 0…NAV_RADIO_NSTEP, `freq = step*0.05 +
  NAV_RADIO_FREQ_MIN` (`Vessel.cpp:1070-1076`). A vessel has `nnav` receivers (2 by default,
  `Vessel.cpp:745`).
- **MFD Map** (`MfdMap.cpp:549`) sets the landing target `SetLandingTarget(Base*)`; it has no public
  SDK access and only affects hints (`Vessel.cpp:7037`). No need to send it.
- **Docking** changes nothing in the vessel except its own port selection (the IDS frequency of a
  port, `SetIDSChannel`, belongs to the target and is not changed by the navigator).
- Other MFD state (mode, target, reference) lives in the navigator's MFD and is not sent.

### Vessel modes

- Autopilots: `ActivateNavmode`/`DeactivateNavmode`/`GetNavmodeState` (`VesselAPI.h:898-925`) are
  readable and settable without knowing the class; the host can apply a mode to the pilot's vessel.
- RCS mode: `GetAttitudeMode`/`SetAttitudeMode` (`VesselAPI.h:3114`).
- Core autopilots must not run on the client copy (section 17), so modes are applied on the host
  and the pilot's client gets the result through the ordinary state.

### DeltaGlider

- Gear, nose cone, airlock, radiators and air brake state is kept in the vessel module and is not
  part of `VESSELSTATUS2` (section 17). It can be read and set through the class's Lua
  interface: `DG:Gear`, `Nosecone`, `Hatch`, `Retro`, `OLock`, `ILock`, `Radiator`, `ABrake`
  (`DGLua.cpp:52-61`); the plugin calls them through `oapiExecScriptCmd` (`OrbiterAPI.h:4512`).
  This only works for the DeltaGlider class.
- Panel and cockpit clicks (`clbkPanelMouseEvent`, `clbkVCMouseEvent`, `DeltaGlider.cpp:1546, 1648`)
  are called by the core only for the focus vessel with a loaded panel (`Pane.cpp:162-170`); a
  plugin cannot call another module's callback (the class is not exposed). Conclusion: **the host
  cannot apply the navigator's clicks**; a "command → Lua or SDK" path is needed.
- The DG control dialog (Ctrl+Space, `DeltaGlider.cpp:1697`) is the vessel's own dialog and is not
  moved to the host.

### What can be synchronized (candidates)

| What | How to read | How to set on the host | Needed |
|---|---|---|---|
| NAV frequencies, transponder | `GetNavChannel`, `GetXpdrChannel` | `SetNavRecv`, `SetXpdrChannel` | yes, stage 4 |
| Autopilots | `GetNavmodeState` | `ActivateNavmode` | per the answer to Q8 item 3 |
| RCS mode | `GetAttitudeMode` | `SetAttitudeMode` | pilot, not navigator |
| DG gear, cone, airlock | DG Lua | DG Lua | pilot only, stage 3 |

The fork is question Q11 in `QUESTIONS.md`.

## 20. The Orbiter flight recorder (reconnaissance 5.6)

Task 3.19 (2026-11-19), base commit `a7a3396e`. Sources: `Src/Orbiter/FlightRecorder.cpp`,
`Orbiter.cpp`, `Vessel.cpp`. Question: can the session flight be recorded so that review uses
Orbiter's stock playback.

### How recording works

- Recording is started only by `Orbiter::ToggleRecorder` (`Orbiter.cpp:1550`): the Ctrl+C key
  (`Keymap.cpp:214`, `Orbiter.cpp:2297`) or the `DlgRecorder` dialog (`DlgRecorder.cpp:38-52`).
  The SDK has no function to start recording, and a plugin cannot call a method of class `Orbiter`.
  The SDK only offers `VESSEL::Recording`/`Playback` and `VESSEL::RecordEvent`
  (`VesselAPI.h:4117-4141`, `Vessel.cpp:10244-10257`), which work when a recording is already
  running.
- The key arrives from the render window, so a server without graphics has nothing to start the
  stock recording with. The SDK does not let a plugin pass a key to the core (`SendBufferedKey`
  acts on a vessel, `VesselAPI.h:3265`). Recording during playback is forbidden (`Orbiter.cpp:1552`).
- Every vessel calls `Vessel::FRecorder_Save` each frame (`Vessel.cpp:7236`) if it has `bFRrecord`;
  only `ToggleRecorder` sets that flag.

### Files

Directory `Flights\<name>\` (`FlightRecorder.cpp:776-790`), all files are text. Times in them are
`SimT - SimT0` at the start of the recording.

| File | Contents |
|---|---|
| `<vessel>.pos` | Headers `STARTMJD`, `REF <body>`, `FRM ECLIPTIC` or `EQUATORIAL`, `CRD CARTESIAN` or `POLAR`; then lines "time x y z vx vy vz" (position and velocity relative to the `REF` body) |
| `<vessel>.att` | `STARTMJD`, `REF`, `FRM ECLIPTIC` or `HORIZON`; lines "time a0 a1 a2" (Euler angles) |
| `<vessel>.atc` | Vessel events: "time ENG 0:0.50 1:…" (engine levels), `NAVMODE`, `RCSMODE`, `UNDOCK`, `ATTACH`, `DETACH`, `LANDED`, `TAKEOFF`, `LIGHTSOURCE` and custom `RecordEvent` events |
| `system.dat` | World events: `TACC` (time acceleration), `CAMERA`, `FOCUS`, `NOTE`, `JUMPTOTIME`, `ENDSESSION` |
| `Playback\<name>.scn` | The scenario at the start of the recording (`SavePlaybackScn`, `Orbiter.cpp:1533`); recorded vessels carry a `FLIGHTDATA` line in it (`Vessel.cpp:8403`) |

- Sampling rate: in free flight a `.pos` line at least every 2 s (`max_step`), more often when the
  velocity vector turns by more than 0.5° (`cddir < 0.995`), and on a state change (landing,
  takeoff, reference body change). Playback interpolates between lines using position and
  velocity (`FlightRecorder.cpp:479+`), so denser writing is not required.
- Playback starts when the scenario has a `Playback <name>` line (`State.cpp:94`) and the vessels
  have `FLIGHTDATA`: vessels read their files (`FRecorder_Read`, `Vessel.cpp:764`), the core
  opens `system.dat` (`Orbiter.cpp:810-812`). The core drives vessels in playback itself and
  ignores their physics and input (`bFRplayback`); playback ends with the end of the file
  (`FRecorder_CheckEnd`).
- The format has no version and no checksums; unknown `.atc` lines are passed to the vessel
  module through `clbkPlaybackEvent`, others are skipped.

### Can a plugin write it

- **The stock recording cannot be started from a plugin** (no SDK function, no access to
  `Orbiter`), and a server without graphics has no key either.
- **Writing files in this format ourselves is possible**: every frame the host already knows the
  position, velocity and angles of all vessels (`GetRelativePos`/`GetRelativeVel`,
  `GetGlobalOrientation`; the reference body via `GetSurfaceRef` or `GetGravityRef`), the time,
  the time acceleration and the dockings. `CRD CARTESIAN`, `FRM ECLIPTIC` and a sample every
  1–2 s are enough. As `.atc` events, engine levels (optional) and `UNDOCK`/`ATTACH` are enough
  for now; without them vessels fly but without exhaust and mode changes. The scenario at the
  start is `oapiSaveScenario` (`OrbiterAPI.h`) plus adding the `Playback` and `FLIGHTDATA`
  lines by hand.
- Limits of this way: playback drives only vessels that existed at the start of the recording
  (vessels created during the session do not get into the start scenario); flights across
  reference bodies need a correct `REF` change; the format is undocumented and may change in new
  Orbiter releases — the daily upstream check (step 4) catches this.

### Checked on the rig (2026-11-19)

The stock recording on an OLC client with graphics works: a host without graphics (Windows 11 x64)
with the `OLC-test\Two DG 20 m` scenario, a Windows 11 ARM client connected with "Connect", Ctrl+C at
the 60th and 90th second. The client wrote `Flights\Host world\GL-01.*` and `GL-02.*` (`.pos` lines
every ~2 s, positions change) and `Scenarios\Playback\Host world.scn` with `FLIGHTDATA` lines. This
scenario, started on the same client without the plugin, plays back normally ("Replay", both
vessels). So the client also records other vessels whose state the plugin imposes.

Not checked: whether playback accepts files not written by the core (the "host writes itself" option).

### The fork

Question Q12 in `QUESTIONS.md`: how to review a flight — write recorder files ourselves
(recommended), an OLC log of our own with its own viewer, or rely on the stock Ctrl+C recording on
the flight director's client with graphics (other vessels there are driven by the received state;
whether such a recording is usable is not verified).

## 21. Vessel mechanisms and frequencies for another participant (research 4A)

Task 3.21 (2026-11-19), base commit `a7a3396e`. The question: what must be transferred so that another
participant sees the deployed gear, an open hatch, a radiator, etc., and can it be done for any vessel class.

### General animation transfer (no knowledge of the class)

- A plugin reads all animations of a vessel: `VESSEL::GetAnimPtr` returns the `ANIMATION` array and
  the number of animations (`Vessel.cpp:10222`), each with a current state 0…1 (`OrbiterAPI.h:1542`).
  The state is set by `SetAnimation(index, value)` (`Vessel.cpp:8055`); the value is just stored, and the
  appearance and the collision hull update themselves.
- Animation indices are defined by the order of `CreateAnimation` in the vessel module. An identical vessel
  (the same class and add-ons, which are in one package for us) gets the same indices on the host and the
  client. The plugin does not know what an index means — a pair "index → value" is transferred.
- Control surface animations are set by the core itself every step from the levels (`Vessel.cpp:2032`);
  transferring them is pointless, the client overwrites the value.
- A module sets its animations itself only while a mechanism is moving: for the DeltaGlider gear
  `SetAnimation` is called only when `gear_state.Process` returned `true`, i.e. the speed is not zero
  (`GearSubsys.cpp:172-178`, `AnimState2::Process`, `Instrument.cpp:221`). If the mechanism stands still on
  the vessel copy, the value set by the plugin stays.
- **What general transfer does not do:** the internal module state does not change. In the DeltaGlider
  `SetGearParameters` (touchdown points and nose wheel steering, `DeltaGlider.cpp:645-660`) is called only
  from the mechanism's own steps, so on a copy whose gear was lowered by animation the touchdown points
  stay "gear up". For a client where vessel physics is not its own and the state is imposed this is
  acceptable (the vessel on the ground keeps the host's state), but the wheels on screen may hang above or sink
  into the surface — not verified on a stand.
- The numbers of gear and other mechanisms must be told apart from noise: most DeltaGlider animations are
  virtual cockpit needles and levers (`GearSubsys.cpp:125`, `AerodynSubsys.cpp:298`, `HudCtrl.cpp:145`,
  etc.), a vessel has dozens. Transfer only the changed ones (with a threshold) — over TCP as an event,
  not every frame.

### DeltaGlider Lua commands (this class only)

- `Gear`, `Nosecone`, `Hatch`, `OLock`, `ILock`, `Retro`, `Radiator`, `ABrake` are "set state" commands,
  not toggles: `dgGear` takes 1 or 0 and calls `LowerGear` or `RaiseGear` (`DGLua.cpp:118-125`),
  `dgNosecone` — `OpenNcone`/`CloseNcone` (`DGLua.cpp:132-141`). Repeating a command is safe.
- Called on a vessel copy, a command starts the **real mechanism of the module**: the animation runs at
  its own speed, touchdown points and the nose wheel update themselves. There is no way to read the state
  through Lua — it has to be read from the animation (the index is known only from the creation order) or
  the fact of the command is transferred from the host.
- Condition: `LowerGear` does not deploy the gear if the vessel already sits on the ground
  (`GearSubsys.cpp:133-137`). On a copy with an imposed state this is possible.

### NAV and transponder frequencies

- NAV receiver frequencies: `GetNavChannel(n)` and `SetNavChannel(n, channel)` (`VesselAPI.h:3300,
  3309`), the number of receivers is `GetNavCount` (`VesselAPI.h:3288`). A channel is a step 0…639.
- Transponder: `SetTransponderChannel` (`VesselAPI.h:3343`); the current step is read through
  `GetTransponder` and `oapiGetNavChannel` (`VesselAPI.h:3385`, `OrbiterAPI.h:4387`). The transponder step is
  part of `VESSELSTATUS2` (`Vesselstatus.cpp:599, 713`), so it is already there when the vessel state is
  transferred, if we fill that field.
- The NAV frequency is not part of `VESSELSTATUS2` (section 17); it is sent as a separate small message on
  an event (changed).

### What is not verified

Everything above is reading the sources. Not verified on a stand: whether a value set by the plugin
stays on a DeltaGlider copy, how the wheels look with general transfer, and whether animation indices match
on the host and the client. This is checked on the Windows stand in a short time during implementation.

### The fork

Question Q13 in `QUESTIONS.md`: how to transfer mechanisms — general animation transfer (any class,
simple, but the internal vessel state does not change), DeltaGlider Lua commands (a real mechanism, but one
class) or a combination (recommended).
