#!/usr/bin/env bash
# Search puzzle-71 PRNG cluster prefixes with a 7-hex Hamming gate.
#
# Eight blocks (0x6A..0x7D), each 2^64 keys, ~34.5% rejected by --hamming 12:16.
# Set CLUSTER_PREFIX=6A to scan one band; omit to rotate through all eight.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT"
BIN="${ROOT}/bin/cpuBitCrack"
ADDR="1PWo3JeB9jrGwfHDNpdGK54CRas7fsVzXU"
LOG_DIR="${ROOT}/research/puzzle71/prng_logs"
mkdir -p "$LOG_DIR"

if [[ ! -x "$BIN" ]]; then
    make BUILD_CPU=1
fi

PREFIXES=(6A 6B 6C 73 74 75 7C 7D)
if [[ -n "${CLUSTER_PREFIX:-}" ]]; then
    PREFIXES=("${CLUSTER_PREFIX}")
fi

run_cluster() {
    local p="$1"
    local start="${p}0000000000000000"
    local end="${p}FFFFFFFFFFFFFFFF"
    local prog="${LOG_DIR}/cluster_${p}.progress"
    local found="${LOG_DIR}/cluster_${p}.found.txt"
    local log="${LOG_DIR}/cluster_${p}.log"

    echo "[$(date -u +%Y-%m-%dT%H:%M:%SZ)] starting prefix 0x${p}" | tee -a "$log"
    stdbuf -oL "$BIN" -t "${THREADS:-4}" -p "${POINTS:-4096}" -c -f \
        --status-interval "${STATUS_INTERVAL_MS:-10000}" \
        --hamming 12:16 \
        --keyspace "${start}:${end}" \
        --continue "$prog" \
        -o "$found" \
        "$ADDR" 2>&1 | stdbuf -oL tee -a "$log"
}

for p in "${PREFIXES[@]}"; do
    run_cluster "$p"
done
