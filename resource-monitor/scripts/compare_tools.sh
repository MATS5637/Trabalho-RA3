#!/usr/bin/env bash
set -euo pipefail

if [ $# -ne 3 ]; then
    echo "Uso: $0 <PID> <intervalo_ms> <amostras>" >&2
    exit 1
fi

PID="$1"
INTERVAL_MS="$2"
SAMPLES="$3"

OUTDIR="docs"
mkdir -p "$OUTDIR"

SLEEP_SEC=$(awk "BEGIN { printf \"%.3f\", $INTERVAL_MS/1000 }")

CPU_RM="$OUTDIR/cpu_rm.csv"
CPU_PS="$OUTDIR/cpu_ps.csv"
MEM_RM="$OUTDIR/mem_rm.csv"
MEM_PS="$OUTDIR/mem_ps.csv"
IO_RM="$OUTDIR/io_rm.csv"

echo "==> Coletando CPU com resource-monitor..."
bin/resource-monitor cpu "$PID" "$INTERVAL_MS" "$SAMPLES" > "$CPU_RM"

echo "==> Coletando CPU/Mem com ps..."
echo "timestamp,amostra,cpu_percent,mem_percent,rss_kb,vsz_kb" > "$CPU_PS"
for ((i=0; i< SAMPLES; i++)); do
    TS=$(date +"%Y-%m-%d %H:%M:%S")
    read CPU MEM RSS VSZ <<<"$(ps -p "$PID" -o %cpu,%mem,rss,vsz --no-headers || echo '0 0 0 0')"
    echo "$TS,$i,$CPU,$MEM,$RSS,$VSZ" >> "$CPU_PS"
    if [ "$i" -lt $((SAMPLES-1)) ]; then
        sleep "$SLEEP_SEC"
    fi
done

echo "==> Coletando Memória com resource-monitor..."
bin/resource-monitor mem "$PID" "$INTERVAL_MS" "$SAMPLES" > "$MEM_RM"

echo "==> Derivando mem_ps.csv a partir de cpu_ps.csv..."
echo "timestamp,amostra,mem_percent,rss_kb,vsz_kb" > "$MEM_PS"
awk -F',' 'NR>1 {print $1","$2","$4","$5","$6}' "$CPU_PS" >> "$MEM_PS"

echo "==> Coletando I/O com resource-monitor..."
bin/resource-monitor io "$PID" "$INTERVAL_MS" "$SAMPLES" > "$IO_RM"

echo
echo "Arquivos gerados em $OUTDIR:"
ls -1 "$OUTDIR"/cpu_*.csv "$OUTDIR"/mem_*.csv "$OUTDIR"/io_*.csv
