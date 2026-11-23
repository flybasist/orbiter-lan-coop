**English** · [Русский](ru/PILOT-GUIDE.md)

# Pilot guide: docking check

For people who check OLC (round 1, plugin `0.1`): two pilots on two computers in one network, each
flying their own DeltaGlider, then both dock; the flight director sets a shared pause and time
acceleration and records the flight (section 5). This is a prototype check: if something is wrong, write down what you
did and what you saw.

## 1. Preparation

Installation — the ready package as in [PACKAGE.md](PACKAGE.md) (building from source — [QUICKSTART.md](QUICKSTART.md), sections 5 and 6), the order of the tests — [TEST-PROCEDURE.md](TEST-PROCEDURE.md). In short:

1. `OrbiterLanCoop.dll` is in `Modules\Plugin` on both computers, and the plugin is enabled.
2. The check scenarios are in `Scenarios\OLC-test` (in the package they are already there; when building from source copy them from `tools/scenarios/`).
3. In the "OLC - LAN co-op" window choose the role `host` on the host and `client` on the client, and enter the host address.
4. The host starts a scenario, the client presses "Connect" and waits 20–40 seconds.

## 2. Scenarios

| Scenario | What is in it | What to do |
|---|---|---|
| `Station approach` | The ISS and a DeltaGlider 150 m from it on the same orbit | Approach the station and dock |
| `Two DG 20 m` | Two DeltaGliders 20 m apart | Dock nose to nose |
| `Two DG 3 km` | Two DeltaGliders 3 km apart, the second on a slightly different orbit (the numbers are an estimate) | Close in with RCS and dock |
| `Two Shuttle-A Moon` | Two Shuttle-A at the Brighton Beach lunar base: SH-03 (host, with payload) and SH-04 (client) | Take off and land on the Moon together; without Moon textures — the textures window, press OK |

By default the host flies the vessel `GL-01` — it is the host's vessel. The client gets its own vessel
by switching the focus (F3) to a free one: in the scenarios with two DeltaGliders it is `GL-02`.

## 3. Controls

The keys are the same as in ordinary Orbiter (the layout comes from `keymap.cfg`, and it must be the
same on the host and the client). What to know about OLC:

| What | How |
|---|---|
| Take a free vessel | Switch the focus to it (F3) if you have no vessel yet |
| Release your vessel | Ctrl+Alt+U |
| Someone else's vessel in focus | View only: the controls are off, a message is on the screen |
| Autopilots | As in Orbiter: `[` — Prograde, `]` — Retrograde, Num 5 — kill rotation |
| Open the DeltaGlider's nose docking port | K (without it no docking happens) |
| Undock | Ctrl+D |
| Camera | Works as usual |
| Mechanisms (gear `G`, cone `K`) and NAV frequencies | All participants see them: the pilot's vessel is in the same state for them |

If the connection is lost, the vessel stays with the pilot for 60 seconds and the controls are released.

## 4. What to write down

- Whether docking worked for both: whether both pilots see the vessels docked, and whether there are jerks or jitter.
- How many seconds after a key press the vessel responded (by eye).
- Any messages on the screen and failures. The log is `Orbiter.log` in the Orbiter folder, lines with `OLC`.

## 5. For the flight director

**Preparation.** On the host machine `Config\OrbiterLanCoop.cfg` has the line `Director = <FD name>`;
the FD connects as a client with graphics under this name. The FD has no vessel: F3 selects any vessel
for viewing only (the "flight director - view only" message), the control keys go nowhere. Vessel collisions
must be off everywhere (`VesselCollision = FALSE` in `Orbiter.cfg`, the package has it so): otherwise
Orbiter itself sets the time acceleration back to x1 while vessels are close.

| FD key | What it does |
|---|---|
| `Ctrl+P` | Pause for everybody / resume |
| `T` / `R` | Time acceleration for everybody: x10 up / down |
| `Ctrl+Alt+S` | Save the world on the host: `Scenarios\OLC\Saves\save-YYYYMMDD-HHMMSS.scn` |
| `Ctrl+C` | Flight recording (below) |

The host itself saves the world to `Scenarios\OLC\Saves\autosave.scn` every 10 minutes (`AutosaveMinutes`).
On the left of the FD screen is a summary: who is connected, in which vessel, the delay to the host.

**Recording the lesson.**

Ctrl+C on the director's client (with graphics) starts the recording, pressing it again stops it.
All ships of the world are recorded, other players' ships too. The files are `Flights\<scenario name>\`
and `Scenarios\Playback\<scenario name>.scn` on the director's machine. To review, start that
scenario in Orbiter without the OLC plugin. Ships created during the lesson are not recorded.
Details: [QUICKSTART.md](QUICKSTART.md), section 6.
