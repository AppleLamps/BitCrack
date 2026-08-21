#!/usr/bin/env python3
"""PRNG cluster search geometry for Bitcoin puzzle 71.

Eight 2-hex prefix bands (flawed-PRNG landing hypothesis), each 2^64 keys,
with an optional runtime Hamming gate on the top 7 hex digits (28 bits).
"""
from __future__ import annotations

import json
import math
from math import comb
from pathlib import Path

PUZZLE = 71
ADDRESS = "1PWo3JeB9jrGwfHDNpdGK54CRas7fsVzXU"
FULL_START = 0x400000000000000000
FULL_END = int("7fffffffffffffffff", 16)
PREFIXES = ["6A", "6B", "6C", "73", "74", "75", "7C", "7D"]
HAMMING_MIN = 12
HAMMING_MAX = 16
PREFIX_HEX_DIGITS = 7
BLOCK_WIDTH = 1 << 64


def popcount28_from_key(k: int) -> int:
    prefix = (k >> 44) & 0x0FFFFFFF
    return bin(prefix).count("1")


def hamming_keep_fraction(n_bits: int = 28, lo: int = HAMMING_MIN, hi: int = HAMMING_MAX) -> float:
    return sum(comb(n_bits, k) for k in range(lo, hi + 1)) / float(1 << n_bits)


def prefix_range(prefix: str) -> tuple[int, int]:
    start = int(prefix + "0" * 16, 16)
    end = int(prefix + "F" * 16, 16)
    start = max(start, FULL_START)
    end = min(end, FULL_END)
    if start > end:
        raise ValueError(f"prefix {prefix} outside puzzle {PUZZLE} interval")
    return start, end


def position_t(k: int) -> float:
    w = FULL_END - FULL_START + 1
    return (k - FULL_START) / w


def cluster_report() -> dict:
    keep = hamming_keep_fraction()
    raw = len(PREFIXES) * BLOCK_WIDTH
    filtered = int(raw * keep)
    # User fleet model: 1,000 × RTX 4090 @ ~25 GKey/s effective (post-filter CUDA path)
    gpu_rate_optimistic = 25e9
    fleet = 1000
    days_optimistic = filtered / (gpu_rate_optimistic * fleet) / 86400.0
    # Conservative reference: ~1.5 GKey/s per 4090
    gpu_rate_conservative = 1.5e9
    days_conservative = filtered / (gpu_rate_conservative * fleet) / 86400.0

    clusters = []
    for p in PREFIXES:
        start, end = prefix_range(p)
        clusters.append(
            {
                "prefix": f"0x{p}",
                "start": f"{start:018x}",
                "end": f"{end:018x}",
                "width": end - start + 1,
                "t_start_pct": round(position_t(start) * 100, 4),
                "t_end_pct": round(position_t(end) * 100, 4),
            }
        )

    return {
        "puzzle": PUZZLE,
        "address": ADDRESS,
        "prefixes": PREFIXES,
        "hamming": {
            "prefix_hex_digits": PREFIX_HEX_DIGITS,
            "min_ones": HAMMING_MIN,
            "max_ones": HAMMING_MAX,
            "keep_fraction": round(keep, 6),
            "reject_fraction": round(1.0 - keep, 6),
        },
        "cost": {
            "blocks": len(PREFIXES),
            "keys_per_block": BLOCK_WIDTH,
            "raw_keys": raw,
            "filtered_keys": filtered,
            "fleet_gpus": fleet,
            "estimated_days_4090_25Gkeys": round(days_optimistic, 1),
            "estimated_days_4090_1.5Gkeys": round(days_conservative, 1),
        },
        "clusters": clusters,
    }


def main() -> None:
    report = cluster_report()
    out = Path(__file__).with_name("clusters.json")
    out.write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report, indent=2))
    print(f"\nWrote {out}")


if __name__ == "__main__":
    main()
