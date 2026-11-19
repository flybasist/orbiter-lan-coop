**English** · [Русский](ru/TEST-PROCEDURE.md)

# Test procedure

For those who check OLC on real computers: who does what, which keys to press, what is checked,
what result is expected and how to record the outcome so that the developers understand it.
Installation — [PACKAGE.md](PACKAGE.md), controls — [PILOT-GUIDE.md](PILOT-GUIDE.md).

**Round 1 — stage 3 "Two vessels"** (decisions 43 and 44). Package of 2026-11-18: Orbiter of the base
commit `a7a3396e`, plugin `0.1-stage3`. For the next round the document is updated: the round
number, the versions and the list of tests change.

## 1. What is checked

The main thing — **can two people on two computers meet in orbit and dock**, and does it look the
same for both. Along the way — what the developers cannot check themselves: smoothness of motion by
eye, response to keys, work over the real classroom network.

## 2. Who is needed

| Role | Who | What they do |
|---|---|---|
| **Flight director (FD)** | The lesson leader | Keeps the order of the tests and the time, fills in the form (section 6), collects the logs |
| **Pilot 1** | A student at computer 1 | Starts the server (`host`) with graphics and flies `GL-01` |
| **Pilot 2** | A student at computer 2 | Connects (`client`) and flies `GL-02` |

The minimum is two computers and two people: the FD can be one of the pilots. If there are more
pairs, each pair goes through the tests separately, with its own server.

## 3. Preparation (before the lesson)

1. **Network.** Both computers are on one local network: preferably by cable through one switch or
   router. Wi-Fi works too (see item 5).
2. **Installation.** On both computers — the package as in [PACKAGE.md](PACKAGE.md), section 2, into
   a folder without spaces, for example `C:\OLC`. On the first start Windows asks about the
   firewall — allow access on **private** networks.
3. **Address of computer 1.** On computer 1: `Win+R` → `cmd` → `ipconfig`; the "IPv4 Address" line,
   for example `192.168.1.10`. Write it into the form.
4. **OLC settings.** In the Launchpad: the "Extra" tab → "OLC - LAN co-op" → "Edit".
   - Computer 1: role `host`, player name — for example `Pilot1`.
   - Computer 2: role `client`, host address — from item 3, name — for example `Pilot2`.
   - The port is the same on both (47800). **The names must differ.**
5. **If Wi-Fi.** On both computers set `StateRate = 30` and `DisplayDelay = 0.1` in
   `C:\OLC\Config\OrbiterLanCoop.cfg` (by cable keep 60 and 0.03). Note in the form which
   network it is.
6. **Check.** Computer 1 starts any scenario, computer 2 presses "Connect". If after a minute
   computer 2 is not in the world of computer 1 — section 7.

## 4. Tests

The order is top to bottom. Each test takes 5–10 minutes. Before each one the FD reads aloud what we
do, after it the pilots say what they saw and the FD writes it into the form (section 6). Keys are the
standard Orbiter ones: digits are on the numeric keypad (`Num`), which must be on (`Num Lock`).

### T1. Connecting

| Who | What they do |
|---|---|
| Pilot 1 | In the Launchpad selects the scenario `OLC-test` → `Two DG 20 m` and presses "Launch Orbiter" |
| Pilot 2 | When pilot 1 has the picture: "Extra" → "OLC - LAN co-op" → "Edit" → "Connect" |
| Pilot 2 | Waits 20–40 s: Orbiter loads the world of computer 1 by itself. Presses F3 and chooses `GL-02` |

**Expected:** pilot 2 has the same world, two DeltaGliders side by side; after choosing `GL-02` the
screen shows "you fly 'GL-02'". Pilot 1 has their own vessel `GL-01`.

### T2. Own vessel

Each pilot in turn, the other one watches their vessel from their own screen.

| Key | What must happen |
|---|---|
| `Num 2` / `Num 8` | Nose up / down (attitude thrusters) |
| `Num 1` / `Num 3`, `Num 4` / `Num 6` | Yaw left / right, bank left / right |
| `Num 5` | Rotation is killed (the Kill rotation autopilot) |
| `Num /` | Attitude thruster mode: rotation ↔ translation; in translation mode `Num 2`/`Num 8` — up/down, `Num 6`/`Num 9` — forward/back |
| `Ctrl` + `Num +` (a few seconds) | The main engine smoothly gains thrust; `Num *` — off |
| `Num 0` / `Num .` | Hover engines: more / less |
| `[` / `]` | Autopilot: nose prograde / retrograde |
| `G` | The landing gear goes down / up |

**Expected:** the vessel responds at once (by eye — without delay), and for the other pilot it moves
the same way, without jerks. Write down if something did not work or worked with a noticeable delay.

**Known limitation:** for pilot 2 the retro engines (`Num −`) do not work: on the DeltaGlider they
are closed by doors, and the doors open from the cockpit panel, whose clicks the plugin does not pass
yet (stage 4). Pilot 1 can check it: `Ctrl+Space` → the DeltaGlider window → open the doors (Retro
Doors → Open), then `Num −`.

**Mechanisms:** the gear (`G`), the nose cone (`K`) and other mechanisms of a vessel are shown to everyone (stage 4A): when pilot 1 opens the cone of `GL-01`, pilot 2 sees it open within a second. Write down if a mechanism stays in the old position.

### T3. Smoothness

Pilot 2 manoeuvres for a minute (rotation and translation), pilot 1 watches `GL-02` from an outside
camera (F1 switches the camera). Then the other way round.

**Expected:** the other vessel moves smoothly, without twitching, jumps or freezes. If it twitches —
write down how often and at what moment.

### T4. Someone else's vessel

| Who | What they do |
|---|---|
| Pilot 2 | F3 → `GL-01` (pilot 1's vessel), tries `Num 2` |
| Pilot 2 | `Ctrl+F3` — focus back on `GL-02`, `Num 2` again |
| Pilot 1 | The same with `GL-02` |

**Expected:** on someone else's vessel — the "view only" message, the keys move **neither** it nor the
own vessel. After `Ctrl+F3` the own vessel responds again.

### T5. Docking together (the main test)

The same scenario, `Two DG 20 m` (if the vessels have drifted apart — restart the scenario on
pilot 1's computer and connect again, T1).

1. Both pilots open the nose docking port: the `K` key.
2. The pilots turn the vessels nose to nose (`Num 5` kills the rotation) and slowly close in using
   translation mode (`Num /`, then `Num 6`). Closing speed — below 0.5 m/s.
3. The ports touch — Orbiter docks the vessels by itself.

**Expected:** for **both** pilots the vessels are docked (the stack flies as one), without shaking or
drift apart. Write down how long it took and what each one saw.

### T6. Undocking

Pilot 1 presses `Ctrl+D`, both back off (`Num 9` in translation mode).

**Expected:** for both the vessels separate and drift apart the same way.

### T7. Lost connection

1. The vessels fly side by side. Pilot 2 **closes Orbiter** (the window's close button).
2. Pilot 1 watches `GL-02`. Within a minute pilot 2 starts Orbiter again and presses "Connect"
   with the same name, then F3 → `GL-02`.
3. If the network cable of computer 2 can be pulled — repeat with the cable instead of closing:
   after 15 seconds the server disconnects pilot 2; then plug the cable back and connect.

**Expected:** without its pilot `GL-02` does not keep firing its engines by itself; the pilot who comes
back with the same name gets `GL-02` back. 60 seconds after leaving, the vessel becomes free.

### T8. Connecting to a docked stack (if there is time)

Dock (T5), then pilot 2 closes Orbiter and connects again.

**Expected:** the returning pilot sees the vessels docked right away.

### T9. Extra (if there is time)

- `Station approach`: one pilot (on computer 1 or 2) approaches the ISS station and docks.
- `Two DG 3 km`: the vessels are 3 km apart, close in and dock — long, needs orbital rendezvous
  skills.

## 5. What to save after the lesson

From **each** computer:

1. `C:\OLC\Orbiter.log` — the log (Orbiter overwrites it at every start, so copy it right after the
   tests, without starting Orbiter again). Rename it: `Orbiter-PC1.log`, `Orbiter-PC2.log`.
2. `C:\OLC\Config\OrbiterLanCoop.cfg` — the settings (rename the same way).
3. Screenshots or a video of the screen if something went wrong (`Win+Shift+S` or a phone).

Everything together with the form (section 6) — into one folder or archive, and pass it to the developer.

## 6. Report form

Copy and fill in. Mark: **yes** — worked as expected, **no** — did not work, **partly** — worked, but
with a remark (describe it).

```text
OLC TEST, ROUND 1 (stage 3)
Date and place:
FD:                     Pilot 1:                 Pilot 2:
Computer 1: Windows __, graphics card __________, IP __________
Computer 2: Windows __, graphics card __________
Network: cable / Wi-Fi (StateRate __, DisplayDelay __)
Earth textures: yes / no

T1 Connecting ............. yes / no / partly  Waited: __ s
T2 Own vessel, pilot 1 .... yes / no / partly  What did not work:
T2 Own vessel, pilot 2 .... yes / no / partly  What did not work:
T3 Smoothness ............. yes / no / partly  How often it twitched:
T4 Someone else's vessel .. yes / no / partly
T5 Docking together ....... yes / no / partly  Took: __ min
T6 Undocking .............. yes / no / partly
T7 Lost connection ........ yes / no / partly  Closing / cable
T8 Connecting to a stack .. yes / no / partly / not done
T9 Extra .................. yes / no / partly / not done

Remarks (what was done, what was seen, what was expected):

What was good / what was awkward:
```

## 7. If something is wrong

| What you see | What to do |
|---|---|
| "Connect" does nothing for a long time, then "Not connected" | Check the address and the port; allow Orbiter in the firewall (private network); computer 1 must already be in a scenario |
| "Not connected: rejected by host: ... version ..." | The computers have different package versions — install the same one |
| "Not connected: rejected by host: no free player slots" | The player limit is reached (`MaxPlayers` on computer 1) |
| The Earth is grey | No Earth textures — this is fine for the tests ([PACKAGE.md](PACKAGE.md), section 2, step 5) |
| The other vessel twitches over Wi-Fi | Set `StateRate = 30`, `DisplayDelay = 0.1` (section 3, item 5) |
| Orbiter crashed | Save `Orbiter.log` right away (section 5) and write down what was done before |
