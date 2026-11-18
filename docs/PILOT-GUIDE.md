**English** · [Русский](ru/PILOT-GUIDE.md)

# Pilot guide: docking check

For people who check stage 3: two pilots on two computers in one network, each flying their own
DeltaGlider, then both dock. This is a prototype check: if something is wrong, write down what you
did and what you saw.

## 1. Preparation

For building and installing the plugin see [QUICKSTART.md](QUICKSTART.md), sections 5 and 6. In short:

1. `OrbiterLanCoop.dll` is in `Modules\Plugin` on both computers, and the plugin is enabled.
2. The check scenarios are in `tools/scenarios/` — copy them to `Scenarios\OLC-test` on the host.
3. In the "OLC - LAN co-op" window choose the role `host` on the host and `client` on the client, and enter the host address.
4. The host starts a scenario, the client presses "Connect" and waits 20–40 seconds.

## 2. Scenarios

| Scenario | What is in it | What to do |
|---|---|---|
| `Station approach` | The ISS and a DeltaGlider 150 m from it on the same orbit | Approach the station and dock |
| `Two DG 20 m` | Two DeltaGliders 20 m apart | Dock nose to nose |
| `Two DG 3 km` | Two DeltaGliders 3 km apart, the second on a slightly different orbit (the numbers are an estimate) | Close in with RCS and dock |

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
| Undock | Ctrl+D |
| Camera | Works as usual |

If the connection is lost, the vessel stays with the pilot for 60 seconds and the controls are released.

## 4. What to write down

- Whether docking worked for both: whether both pilots see the vessels docked, and whether there are jerks or jitter.
- How many seconds after a key press the vessel responded (by eye).
- Any messages on the screen and failures. The log is `Orbiter.log` in the Orbiter folder, lines with `OLC`.
