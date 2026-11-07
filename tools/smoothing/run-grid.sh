#!/usr/bin/env bash
# Прогон модели сглаживания по сетке параметров (задача Д2 плана).
#
# Запуск из корня репозитория после сборки:
#   cmake -S . -B orbiter-lan-coop.out && cmake --build orbiter-lan-coop.out
#   tools/smoothing/run-grid.sh
#
# Результат пишется в orbiter-lan-coop.out/smoothing/:
#   grid-raw.csv     - строка на каждый метод каждого прогона (с номером seed);
#   grid-summary.csv - то же, сведённое по seed: средние ошибки усредняются,
#                      максимальные берутся худшими из всех seed.
# Выводы по этим файлам - в docs/SMOOTHING.md.

set -euo pipefail

BIN="orbiter-lan-coop.out/tools/smoothing/olc-smoothing"
OUT_DIR="orbiter-lan-coop.out/smoothing"

if [[ ! -x "$BIN" ]]; then
    echo "Error: $BIN not found, build the project first" >&2
    exit 1
fi
mkdir -p "$OUT_DIR"

# Сетка из PLAN.md, Д2. Задержка до сервера в LAN - 1 мс, к ней добавляется разброс.
RATES="10 20 30"            # частота отправки, Гц
LOSSES="0 5 20"             # потери, %
JITTERS="1 20 50"           # разброс задержки, мс
WARPS="1 10 100"            # ускорение времени
DELAYS="50 100 150 200 300" # задержка буфера интерполяции на клиенте, мс
SEEDS="1 2 3 4 5"           # разные случайные потери и разброс

RAW="$OUT_DIR/grid-raw.csv"
SUMMARY="$OUT_DIR/grid-summary.csv"

echo "seed,$("$BIN" --csv-header)" > "$RAW"

for rate in $RATES; do
  for loss in $LOSSES; do
    for jitter in $JITTERS; do
      for warp in $WARPS; do
        for delay in $DELAYS; do
          for seed in $SEEDS; do
            # Перед каждой строкой вывода дописываем номер seed.
            "$BIN" --csv --rate "$rate" --loss "$loss" --jitter "$jitter" --warp "$warp" \
                --delay "$delay" --seed "$seed" | sed "s/^/$seed,/" >> "$RAW"
          done
        done
      done
    done
  done
done

# Сводка по seed. Колонки grid-raw.csv:
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
