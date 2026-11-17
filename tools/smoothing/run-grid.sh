#!/usr/bin/env bash
# Runs the smoothing model over a parameter grid (plan task D2).
#
# Run from the repository root after building:
#   cmake -S . -B orbiter-lan-coop.out && cmake --build orbiter-lan-coop.out
#   tools/smoothing/run-grid.sh
#
# The result goes to orbiter-lan-coop.out/smoothing/:
#   grid-raw.csv     - a line per method of every run (with the seed number);
#   grid-summary.csv - the same, merged over seeds: mean errors are averaged,
#                      maximum errors take the worst over all seeds.
# Conclusions from these files - in docs/SMOOTHING.md.

set -euo pipefail

BIN="orbiter-lan-coop.out/tools/smoothing/olc-smoothing"
OUT_DIR="orbiter-lan-coop.out/smoothing"

if [[ ! -x "$BIN" ]]; then
    echo "Error: $BIN not found, build the project first" >&2
    exit 1
fi
mkdir -p "$OUT_DIR"

# The grid from PLAN.md, D2. Delay to the server on a LAN is 1 ms, jitter is added to it.
RATES="10 20 30"            # send rate, Hz
LOSSES="0 5 20"             # loss, %
JITTERS="1 20 50"           # delay jitter, ms
WARPS="1 10 100"            # time acceleration
DELAYS="50 100 150 200 300" # client interpolation buffer delay, ms
SEEDS="1 2 3 4 5"           # different random loss and jitter

RAW="$OUT_DIR/grid-raw.csv"
SUMMARY="$OUT_DIR/grid-summary.csv"

echo "seed,$("$BIN" --csv-header)" > "$RAW"

for rate in $RATES; do
  for loss in $LOSSES; do
    for jitter in $JITTERS; do
      for warp in $WARPS; do
        for delay in $DELAYS; do
          for seed in $SEEDS; do
            # The seed number is prepended to each output line.
            "$BIN" --csv --rate "$rate" --loss "$loss" --jitter "$jitter" --warp "$warp" \
                --delay "$delay" --seed "$seed" | sed "s/^/$seed,/" >> "$RAW"
          done
        done
      done
    done
  done
done

# Summary over seeds. Columns of grid-raw.csv:
#  1 seed, 2 rate, 3 warp, 4 loss, 5 latency_ms, 6 jitter_ms, 7 delay_ms, 8 profile, 9 method,
# 10 pos_mean_m, 11 pos_p95_m, 12 pos_max_m, 13 att_max_deg, 14 jump_max_m, 15 starved_pct,
# 16 lag_sim_s
awk -F, '
NR == 1 { next }
{
    key = $2 "," $3 "," $4 "," $6 "," $7 "," $9
    if (!(key in runs)) { order[++count] = key }
    runs[key]++
    mean[key] += $10
    if ($11 > p95[key]) p95[key] = $11
    if ($12 > pmax[key]) pmax[key] = $12
    if ($13 > att[key]) att[key] = $13
    if ($14 > jump[key]) jump[key] = $14
    starvedText[key] = $15
    if ($15 != "-") starved[key] += $15
    lag[key] = $16
}
END {
    print "rate,warp,loss,jitter_ms,delay_ms,method,pos_mean_m,pos_p95_max_m,pos_max_m,att_max_deg,jump_max_m,starved_pct,lag_sim_s"
    for (i = 1; i <= count; i++) {
        k = order[i]
        s = (starvedText[k] == "-") ? "-" : sprintf("%.2f", starved[k] / runs[k])
        printf "%s,%.4f,%.4f,%.4f,%.4f,%.4f,%s,%s\n", k, mean[k] / runs[k], p95[k], pmax[k], att[k], jump[k], s, lag[k]
    }
}' "$RAW" > "$SUMMARY"

echo "Wrote $RAW and $SUMMARY"
