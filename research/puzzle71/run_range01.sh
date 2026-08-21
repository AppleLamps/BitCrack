#!/usr/bin/env bash
# Scan RANGE_01 for Bitcoin puzzle 71 (address-only; brute force required).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT"
BIN="${ROOT}/bin/cpuBitCrack"
if [[ ! -x "$BIN" ]]; then
    make BUILD_CPU=1
fi
exec "$BIN" -t "${THREADS:-4}" -p "${POINTS:-4096}" -c \
    --keyspace 7CC000000000000000:7CCFFFFFFFFFFFFFFF \
    --continue "${ROOT}/research/puzzle71/range01.progress" \
    -o "${ROOT}/research/puzzle71/range01.found.txt" \
    1PWo3JeB9jrGwfHDNpdGK54CRas7fsVzXU
