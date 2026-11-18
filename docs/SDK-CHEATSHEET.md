**English** · [Русский](ru/SDK-CHEATSHEET.md)

# Orbiter SDK cheat sheet for OLC

"What to call in order to…" — only the functions OLC needs. Why exactly so and what is inside — in
[ORBITER-INTERNALS.md](ORBITER-INTERNALS.md). Headers: `Orbitersdk/include/OrbiterAPI.h`
(`oapi*` functions), `VesselAPI.h` (the `VESSEL` class), `ModuleAPI.h` (the `oapi::Module` class).
The base commit is `a7a3396e`. Everything was obtained by reading the code; behaviour is checked on the rigs as the stages go.

## 1. Plugin life cycle

| Task | Call | Catch |
|---|---|---|
| Start a session | `clbkSimulationStart` | The plugin lives between sessions: create all session state here |
| End a session | `clbkSimulationEnd` | The world is already destroyed: do not touch vessels, only the network and tables |
| Work every frame | `clbkPreStep` | Not called while paused |
| Work while paused and with an inactive window | Your own hidden window (`HWND_MESSAGE`) with `SetTimer` or `PostMessage` | There are no per-frame callbacks while paused; window messages always arrive (`ORBITER-INTERNALS.md`, section 14) |
| Create the network and threads | In `clbkSimulationStart`, not in `InitModule` | `InitModule`/`ExitModule` run inside `DllMain` |
| A vessel appeared or was deleted | `clbkNewVessel`, `clbkDeleteVessel` | `clbkNewVessel` — only for vessels created at run time, before their setup is finished |
| Warp or pause changed | `clbkTimeAccChanged`, `clbkPause` | In `clbkTimeAccChanged` both arguments are the new value; `oapiSetPause` takes effect only in the next frame |
| A time jump | `clbkTimeJump` | — |

## 2. Vessel state

| Task | Call | Catch |
|---|---|---|
| Iterate over vessels | `oapiGetVesselCount`, `oapiGetVesselByIndex`, `oapiGetVesselInterface` | — |
| Find a vessel by name | `oapiGetVesselByName` | Case-insensitive, the first one found; the core does not check name uniqueness |
| Name and class | `VESSEL::GetName`, `VESSEL::GetClassName` | The class may contain a subfolder |
| Capture a state (host) | `VESSEL::GetStatusEx` with `VESSELSTATUS2`, `version = 2` | In `clbkPreStep`; the state time is `oapiGetSimTime`. Lists with `VS_*LIST` flags are freed by the caller |
| Impose flight (client) | `VESSEL::DefSetStateEx`, `status = 0` | **Only in `clbkPreStep`:** in `clbkPostStep` the attitude and rotation are lost. Do not put it below the terrain — it lifts it itself |
| Impose "landed" | `DefSetStateEx`, `status = 1`, `surf_lng/lat/hdg` | Once on landing, not every frame. Not inside `clbkPostStep` |
| Attitude / rotation only | `SetGlobalOrientation`, `SetRotationMatrix`, `SetAngularVel` | Light, take the superstructure into account; only in `clbkPreStep` |
| Position or velocity separately | — | Not in the SDK: only `DefSetStateEx` |
| Flight status | `VESSEL::GetFlightStatus` | Bit 0 — landed, bit 1 — part of a superstructure |
| Fuel and thruster levels | `VS_FUELLIST`, `VS_THRUSTLIST` in `VESSELSTATUS2`; `SetPropellantMass`, `SetThrusterLevel` | Fuel before thrust: the core zeroes a thruster with an empty tank every step. Send the empty mass (`SetEmptyMass`) separately |
| Animations, lights, beacons | `GetAnimPtr`/`SetAnimation`, `GetLightEmitter`, `GetBeacon` | An animation is only a picture, the internal state of the vessel module does not change (`ORBITER-INTERNALS.md`, section 17) |

Do not send `OBJHANDLE` over the network: the reference body, base and vessels go by name
(`oapiGetObjectName`, `oapiGetGbodyByName`, `oapiGetBaseByName`).

## 3. Input: a key on the client → a call on the host

The core applies the standard held keys only to the focus vessel. For a client's vessel
the host repeats them itself. `dt` is `oapiGetSimStep()`.

| Default key | Call on the host for the client's vessel |
|---|---|
| Numpad2/8/1/3/4/6 — RCS rotation (RCS mode 1) | `IncThrusterGroupLevel_SingleStep (THGROUP_ATT_PITCHUP…BANKRIGHT, 1.0)` every frame while held |
| Numpad2/8/1/3/6/9 — RCS translation (RCS mode 2) | `IncThrusterGroupLevel_SingleStep (THGROUP_ATT_UP…BACK, 1.0)` |
| Ctrl + the same — low RCS power | The same with `0.1` |
| Numpad+ / Numpad- — full main / retro while held | `IncThrusterGroupLevel_SingleStep (THGROUP_MAIN, 1.0)` and `(THGROUP_RETRO, -1.0)`; the other way round for retro |
| Ctrl+Numpad+ / Ctrl+Numpad- — smooth throttle | Our own logic using `Get/SetThrusterGroupLevel` (below), step `0.2 * dt` |
| Numpad* — kill main and retro | `SetThrusterGroupLevel (THGROUP_MAIN, 0)`, `(THGROUP_RETRO, 0)` |
| Numpad0 / Numpad. — hover up / down | `IncThrusterGroupLevel (THGROUP_HOVER, ±0.2 * dt)` |
| Delete / Insert — trim | Our own accumulator `trim ± 0.25 * dt` within ±1, then `SetControlSurfaceLevel` |
| Comma / Period — left / right wheel brake | `SetWheelbrakeLevel (1.0, 1 or 2, false)` every frame while held |
| Single keys: autopilots, RCS on/mode, undocking, add-on keys | `VESSEL::SendBufferedKey (key, true, kstate)` |
| HUD keys | Do not forward: they act on the HUD of the host's focus vessel |

RCS mode: `GetAttitudeMode`, `SetAttitudeMode` (0 — off, 1 — rotation, 2 — translation).
Aerodynamic control mode: `GetADCtrlMode`, `SetADCtrlMode`.

Smooth throttle (like `Vessel::IncMainRetroLevel`, `Vessel.cpp:2101`), `d = ±0.2 * dt`:

- `d > 0`: if retro > 0 — decrease retro by `d` (not below 0) and main = 0;
  otherwise main = min(main + `d`, 1);
- `d < 0`: if main > 0 — decrease main by `|d|` (not below 0) and retro = 0;
  otherwise retro = min(retro + `|d|`, 1).

What cannot be done: deliver held keys to `clbkConsumeDirectKey` of a vessel that is
not in focus. Add-ons' own held keys will not work on the host.

### Intercepting input on the client

| Task | Call | Catch |
|---|---|---|
| Held keys | `clbkProcessKeyboardImmediate (kstate, simRunning)` | `true` turns off standard processing for the whole frame, including vessel keys |
| Single presses | `clbkProcessKeyboardBuffered (key, kstate, simRunning)` | Press only; `true` removes the key from the queue |
| Joystick: RCS | `GetManualControlLevel (group, MANCTRL_ANYMODE, MANCTRL_JOYSTICK)` | Values of the previous frame; works only if the joystick is enabled in Orbiter |
| Joystick: throttle | `GetThrusterGroupLevel (THGROUP_MAIN)` of the focus vessel | The throttle lever sets a constant level |
| Keyboard outside callbacks | `oapiGetKeyState` | The state of the previous frame; empty when an ImGui window took the keyboard |
| Mouse | `clbkProcessMouse` | `true` hides the event from the core |

## 4. Time

| Task | Call | Catch |
|---|---|---|
| Time, step | `oapiGetSimTime`, `oapiGetSimStep`, `oapiGetSimMJD` | `oapiGetSimTime` is the start of the current step |
| Warp | `oapiGetTimeAcceleration`, `oapiSetTimeAcceleration` | Through the SDK it changes at once (transition delay 0); a smooth transition exists only inside the core |
| Pause | `oapiGetPause`, `oapiSetPause` | Applied at the start of the next frame |
| A large time difference | `oapiSetSimMJD (mjd, pmode)` | A heavy operation, only for large differences |

## 5. Scenarios and vessels

| Task | Call | Catch |
|---|---|---|
| Save the world | `oapiSaveScenario (fname, desc)` | Only to a file in `Scenarios`, subfolders must exist |
| Load a scenario at run time | — | There is no SDK function; the plugin can close the session and start a scenario through Launchpad itself (`ORBITER-INTERNALS.md`, section 18) |
| Create a vessel | `oapiCreateVesselEx (name, classname, &vs2)` | **No class file — Orbiter terminates.** First check `Config\Vessels\<class>.cfg` |
| Delete a vessel | `oapiDeleteVessel (h)` | Deleted at the end of the frame; the last vessel that can have the focus cannot be deleted |
| Change the focus | `oapiSetFocusObject (h)` | — |

## 6. Docking

| Task | Call | Catch |
|---|---|---|
| Learn the dockings | `GetStatusEx` with `VS_DOCKINFOLIST` → `dockinfo[].idx/ridx/rvessel` | `rvessel` is a handle, over the network — the name |
| Dock on the client | `VESSEL::Dock (target, n, tgtn, 0)` | Mode 0 — no teleport, the vessels must already be aligned. Codes: 0 — success, 1/2 — port busy, 3 — already in a superstructure |
| Undock | `VESSEL::Undock (n)` | Pushes the vessels apart and turns off their collisions for 5 s |
| Drive a superstructure | `DefSetStateEx` + `SetGlobalOrientation` + `SetAngularVel` on one vessel | Do not set the state of every part |
| `VS_DOCKINFOLIST` in `DefSetStateEx` | — | Do not use at run time: pointers only, without the superstructure |

## 7. Interface

| Task | Call | Catch |
|---|---|---|
| A settings item in Launchpad | `LaunchpadItem` + `oapiRegisterLaunchpadItem` in `InitModule` | The window is a Win32 dialog, not ImGui; there is no ImGui in Launchpad |
| Save settings before start | `LaunchpadItem::clbkWriteConfig` | Called before every session, the start cannot be cancelled |
| Start a scenario | Not in the SDK | Select it in the Launchpad list and send `WM_COMMAND IDLAUNCH` (`ORBITER-INTERNALS.md`, section 18); the fallback is restarting `orbiter.exe -s <scenario>` |
| A window in the simulation | `ImGuiDialog` + `oapiOpenDialog` | Crashes outside a session; drawn while paused too; no windows without a graphics client |
| Open a window for the player | `oapiRegisterCustomCmd`, `oapiRegisterCustomMenuCmd` | While an ImGui window has focus, the keyboard goes to it |
| A short message | `oapiAddNotification` | — |
| Permanent text on screen | `oapiCreateAnnotation` | Only in a session and with a graphics client; drawn through ImGui, hidden together with the windows; text in UTF-8 |

Details — `ORBITER-INTERNALS.md`, section 16.
