#!/usr/bin/env bash
# Scan the 1% window centred on the tightest solved-key landing cluster
# (puzzles 40, 51, 59, 80 all sat at 82.2–82.9% of their own ranges).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT"
BIN="${ROOT}/bin/cpuBitCrack"
if [[ ! -x "$BIN" ]]; then
    make BUILD_CPU=1
fi
exec "$BIN" -t "${THREADS:-4}" -p "${POINTS:-4096}" -c \
    --keyspace 748e9ea2d6f1f2bbd5:753275ad14629692de \
    --continue "${ROOT}/research/puzzle71/p71.progress" \
    -o "${ROOT}/research/puzzle71/found.txt" \
    1PWo3JeB9jrGwfHDNpdGK54CRas7fsVzXU
