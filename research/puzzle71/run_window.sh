#!/usr/bin/env bash
# Scan the predicted 1% window for Bitcoin puzzle 71.
# Range is midpoint ± 0.5% of [2^70, 2^71): about 1.1806e19 keys.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT"
BIN="${ROOT}/bin/cpuBitCrack"
if [[ ! -x "$BIN" ]]; then
    make BUILD_CPU=1
fi
exec "$BIN" -t "${THREADS:-4}" -p "${POINTS:-4096}" -c \
    --keyspace 5fae147ae147ae147b:6051eb851eb851eb84 \
    --continue "${ROOT}/research/puzzle71/p71.progress" \
    -o "${ROOT}/research/puzzle71/found.txt" \
    1PWo3JeB9jrGwfHDNpdGK54CRas7fsVzXU
