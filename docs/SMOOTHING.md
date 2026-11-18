**English** · [Русский](ru/SMOOTHING.md)

# Motion smoothing: model results

How often the host should send vessel states, how far the client should lag behind the host and how
to reconstruct motion between packets. The answer comes from the `tools/smoothing/` model
(`olc-smoothing`), without Orbiter. In the real game the numbers were checked on the rigs (measurement of task 3.0, decision 36).

> Update of 2026-11-18 (decision 36): the target environment is a wired LAN (decision 32), and a
> measurement on the rigs gave other defaults for it: 60 Hz and a 30 ms delay — half the error of 30 Hz,
> no frames without data. The values below remain for a network with delay jitter (Wi-Fi).

## Summary

| Parameter | Recommendation | Fallback |
|---|---|---|
| `WORLD_STATE` rate | **30 Hz** | 20 Hz if it is heavy for the host |
| Client buffer delay | **100 ms** (≈ 3 intervals between packets) | 150 ms at 20 Hz |
| Reconstruction method | **cubic Hermite interpolation** over position and velocity | — |
| Attitude | slerp between packets (as in the model) | see "What next" |

At 30 Hz and 100 ms the position error on the client (the worst over a run, over 5 seeds):

| Network conditions | ×1 | ×10 | ×100 |
|---|---|---|---|
| Loss 5%, delay jitter 20 ms | 3 mm | 0.3 m | 1.1 m |
| Loss 20%, delay jitter 50 ms (a very bad network) | 5 cm | 1.6 m | 2.8 m |

This is well within the prototype criterion of "a few metres" (`PLAN.md`, stage 1).

Why:

1. **Hermite is the best in all modes.** It needs only position and velocity, and they are already
   in `WORLD_STATE`. Linear interpolation is 6–40 times worse: on an orbit
   a straight line between two points cuts the arc. Extrapolation at ×1 and ×10 is 1–2 times worse than Hermite,
   and at ×100 20 times worse; besides, it needs the vessel's acceleration, which is not
   in the protocol, and it does not remove jerks when engines start and stop.
2. **The main error source is an empty buffer.** While packets arrive on time, the error is
   millimetres. When a packet is lost and the buffer runs out, the client continues the motion with the
   last velocity, and the error grows, especially under time acceleration: 1 ms of real
   time at ×100 is 0.1 s of simulation. So the delay is chosen so that the buffer
   almost never runs empty.
3. **A delay of ≈ 3 intervals between packets** covers losing two packets in a row and
   delay jitter up to 50 ms: at 5% loss the buffer is empty in fewer than 0.1% of frames.
4. **30 Hz versus 20 Hz:** at 20 Hz the same reliability needs 150 ms, while the prototype criterion
   allows the client sim time to lag by no more than 0.1 s (see below). The traffic at 30 Hz
   is negligible for a LAN: a vessel state in `WORLD_STATE` is about 110 bytes, 8 vessels are about
   1 KB per packet, 30 KB/s per client.

### Important for the implementation: the whole client world lags

Interpolation shows vessels 100 ms in the past. **The whole** client world must lag —
its sim time, the planets and all vessels — not only other players' vessels. If
a vessel is shown at moment `t − 100 ms` against a planet at moment `t`, on a low orbit
the vessel shifts by 7.7 km/s × 0.1 s ≈ 770 m. So the client sim time is kept
the delay behind the host, and the 100 ms delay fits exactly into the prototype criterion
"sim time differs by no more than 0.1 s". This agrees with
`PROTOCOL.md`, section 8.

## How the model works

- **Reference motion** (the `pilot` profile, all numbers are for it): a circular 400 km orbit,
  main engine pulses of 3 m/s² (1 s on, 1 s off) from 30 to 50 s, RCS rotation
  pulses of 0.1 rad/s² around three axes from 60 to 90 s, RCS translation pulses of 0.3 m/s²
  from 95 to 110 s. 120 s of simulation in total. The `smooth` profile (one long maneuver) is easier and
  was not part of the grid.
- **The host** sends the state (time, position, velocity, acceleration, attitude, angular
  velocity) at a given real-time rate. The start of sending is shifted by a random
  fraction of an interval: otherwise packets coincide with the start and end of the pulses, and extrapolation and
  interpolation get an unfair advantage.
- **Network**: a 1 ms delay plus a random jitter from 0 to a given value, random
  loss. Packets come in arrival order; the client drops an outdated packet.
- **The client** draws 60 frames per second and reconstructs the position in four ways:
  - `snap_gravity` — the last state continued under gravity only (like
    a plain `DefSetStateEx` without smoothing);
  - `extrapolate` — the last state continued with the acceleration from the packet;
  - `interp_linear` — linear interpolation between packets with a buffer delay;
  - `interp_hermite` — cubic Hermite interpolation over positions and velocities with the same
    delay.
- **Metrics** per frame after the first 5 s of simulation. For interpolation the shown state
  is compared with the true state at the same (delayed) moment: the lag by itself
  is not counted as an error, it is shown separately in the `lag_sim_s` column.
  - `pos_mean_m`, `pos_max_m` — the mean and the worst position error;
  - `att_max_deg` — the worst attitude error;
  - `jump_max_m` — jerk: how much the vessel's shift per frame differs from the real one;
  - `starved_pct` — the share of frames with an empty buffer.

Simplifications: one vessel, the client sim time is estimated from the last packet (real
synchronization is a separate task), host frames are even, engine pulses are
synthetic.

## Grid

Rate 10/20/30 Hz × loss 0/5/20% × delay jitter 1/20/50 ms × warp ×1/×10/×100,
for each point a buffer delay of 50/100/150/200/300 ms and 5 seeds — 2025 runs.
Summary over seeds: the mean error is averaged, the worst is the worst over all seeds.

To repeat:

```bash
cmake -S . -B orbiter-lan-coop.out && cmake --build orbiter-lan-coop.out
tools/smoothing/run-grid.sh
```

The result is `orbiter-lan-coop.out/smoothing/grid-raw.csv` and `grid-summary.csv`. The tables
below are taken from `grid-summary.csv`.

### Share of frames with an empty buffer, %

Interpolation, ×1 (it hardly depends on warp: both the packets and the delay are in real
time). Rows — rate, loss and delay jitter; columns — buffer delay.

| Rate | Loss | Jitter | 50 ms | 100 ms | 150 ms | 200 ms | 300 ms |
|---|---|---|---|---|---|---|---|
| 10 Hz | 0% | 1 ms | 50.00 | 0 | 0 | 0 | 0 |
| 10 Hz | 0% | 50 ms | 50.00 | 8.54 | 0 | 0 | 0 |
| 10 Hz | 5% | 20 ms | 52.55 | 8.53 | 2.71 | 0.47 | 0.03 |
| 10 Hz | 20% | 50 ms | 60.23 | 25.82 | 12.36 | 5.44 | 1.31 |
| 20 Hz | 0% | 1 ms | 0 | 0 | 0 | 0 | 0 |
| 20 Hz | 0% | 50 ms | 16.93 | 0 | 0 | 0 | 0 |
| 20 Hz | 5% | 20 ms | 11.17 | 0.62 | 0.03 | 0 | 0 |
| 20 Hz | 20% | 50 ms | 30.54 | 6.10 | 1.29 | 0.30 | 0.01 |
| 30 Hz | 0% | 1 ms | 0 | 0 | 0 | 0 | 0 |
| 30 Hz | 0% | 50 ms | 7.01 | 0 | 0 | 0 | 0 |
| 30 Hz | 5% | 20 ms | 2.78 | 0.04 | 0 | 0 | 0 |
| 30 Hz | 20% | 50 ms | 17.36 | 1.62 | 0.16 | 0.01 | 0 |

### Comparison of methods

30 Hz, loss 5%, delay jitter 20 ms, buffer delay 100 ms.

| Warp | Method | Mean error, m | Worst error, m | Attitude, ° | Jerk, m |
|---|---|---|---|---|---|
| ×1 | `snap_gravity` | 0.0001 | 0.017 | 0.033 | 0.017 |
| ×1 | `extrapolate` | 0.0000 | 0.006 | 0.033 | 0.006 |
| ×1 | `interp_linear` | 0.0011 | 0.019 | 0.007 | 0.021 |
| ×1 | `interp_hermite` | 0.0000 | 0.003 | 0.007 | 0.003 |
| ×10 | `snap_gravity` | 0.0091 | 1.38 | 0.92 | 1.38 |
| ×10 | `extrapolate` | 0.0020 | 0.35 | 0.92 | 0.35 |
| ×10 | `interp_linear` | 0.107 | 1.93 | 0.28 | 2.11 |
| ×10 | `interp_hermite` | 0.0003 | 0.30 | 0.28 | 0.30 |
| ×100 | `snap_gravity` | 0.67 | 23.5 | 11.2 | 23.5 |
| ×100 | `extrapolate` | 0.70 | 26.9 | 11.2 | 26.9 |
| ×100 | `interp_linear` | 10.3 | 48.5 | 3.27 | 36.3 |
| ×100 | `interp_hermite` | 0.059 | 1.13 | 3.27 | 0.82 |

### Hermite: rate, warp and buffer delay

The worst position error, m, and in parentheses the share of frames with an empty buffer, %.

Loss 5%, delay jitter 20 ms:

| Rate | Warp | 100 ms | 150 ms | 300 ms |
|---|---|---|---|---|
| 10 Hz | ×1 | 0.36 (8.5) | 0.24 (2.7) | 0.03 (0.03) |
| 10 Hz | ×10 | 16.4 (7.8) | 9.1 (2.6) | 0.21 (0) |
| 10 Hz | ×100 | 403 (9.5) | 93 (2.1) | 3.7 (0) |
| 20 Hz | ×1 | 0.028 (0.6) | 0.004 (0.03) | 0.0003 (0) |
| 20 Hz | ×10 | 2.7 (0.8) | 0.36 (0.06) | 0.17 (0) |
| 20 Hz | ×100 | 0.89 (0.6) | 0.91 (0) | 0.83 (0) |
| 30 Hz | ×1 | 0.003 (0.04) | 0.0004 (0) | 0.0004 (0) |
| 30 Hz | ×10 | 0.30 (0.06) | 0.047 (0) | 0.048 (0) |
| 30 Hz | ×100 | 1.1 (0) | 1.5 (0) | 1.5 (0) |

Loss 20%, delay jitter 50 ms:

| Rate | Warp | 100 ms | 150 ms | 300 ms |
|---|---|---|---|---|
| 10 Hz | ×1 | 1.45 (25.8) | 1.21 (12.4) | 0.62 (1.3) |
| 10 Hz | ×10 | 61.8 (27.0) | 46.5 (13.3) | 13.6 (1.1) |
| 10 Hz | ×100 | 1843 (22.0) | 1057 (9.9) | 20.9 (0.4) |
| 20 Hz | ×1 | 0.24 (6.1) | 0.15 (1.3) | 0.005 (0.01) |
| 20 Hz | ×10 | 9.3 (6.5) | 4.0 (1.4) | 0.52 (0) |
| 20 Hz | ×100 | 196 (6.1) | 2.8 (0.3) | 2.9 (0) |
| 30 Hz | ×1 | 0.054 (1.6) | 0.017 (0.16) | 0.0008 (0) |
| 30 Hz | ×10 | 1.6 (1.7) | 0.076 (0.09) | 0.074 (0) |
| 30 Hz | ×100 | 2.8 (0.3) | 2.7 (0) | 2.8 (0) |

10 Hz does not work: avoiding large errors needs a 300 ms delay, three times the
sim time criterion.

## What next

- **Interpolation buffer (task V4, done)** — `core/interpolation.h`: Hermite over position and
  velocity, slerp for attitude, when the buffer is empty — motion with the last velocity, as in the
  model. A difference from the model: with an empty buffer the attitude is not continued with the angular
  velocity but stays the last one, until it is checked on a test rig in which coordinate frame
  Orbiter gives `vrot`. The display delay is the plugin setting `DisplayDelay`: 30 ms at 60 Hz by default for a wired LAN (decision 36), 100 ms at 30 Hz over Wi-Fi.
- **Attitude under time acceleration.** slerp does not take angular velocity into account, so at ×100 and
  with RCS pulses the attitude error reaches 3–6°. The angular velocity is already in the packet
  (`vrot`); cubic attitude interpolation using it is a candidate improvement if at
  stage 1 the error is visible to the eye.
- **The check in the real Orbiter** — on the rigs of stages 1–3: uneven host frames, real
  sim time synchronization, several vessels; the result for a wired LAN is the measurement of task 3.0.
- **A real network.** An `olc-netcheck` measurement over Wi-Fi ([STANDS.md](STANDS.md), section 10):
  no loss, but the delay jitter is rare spikes of up to ~190 ms round trip, not
  an even spread as in the model. The 100 ms buffer survived them. If at stage 1 there
  are jerks over Wi-Fi, such spikes should be added to the model and the buffer delay recalculated.
