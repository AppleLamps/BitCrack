# Puzzle 71 — PRNG cluster search (Hamming-filtered)

Target: `1PWo3JeB9jrGwfHDNpdGK54CRas7fsVzXU`  
Full interval: `400000000000000000 : 7fffffffffffffffff`

## Hypothesis

Keys from a flawed PRNG concentrate in eight high-byte **prefix bands** within
the puzzle-71 interval. Each band is one byte wide and holds `2^64` candidates.
Before the expensive secp256k1 + HASH160 pipeline, keep only keys whose **top
7 hex digits** (28 bits) have Hamming weight in `[12, 16]` — the binomial
core of a random 28-bit string.

## Prefix blocks

| Prefix | Keyspace start | Approx. position `t` |
|--------|----------------|----------------------|
| `0x6A` | `6a0000000000000000` | 65.6% |
| `0x6B` | `6b0000000000000000` | 68.8% |
| `0x6C` | `6c0000000000000000` | 71.9% |
| `0x73` | `730000000000000000` | 78.1% |
| `0x74` | `740000000000000000` | 81.2% |
| `0x75` | `750000000000000000` | 84.4% |
| `0x7C` | `7c0000000000000000` | 93.8% |
| `0x7D` | `7d0000000000000000` | 96.9% |

## Hamming filter

| | |
|---|---|
| Window | top 7 hex digits (28 bits) |
| Keep | popcount ∈ [12, 16] |
| Pass rate | **65.51%** (Binomial(28, ½)) |
| Reject | **34.49%** (~33% cost reduction) |

## Cost model

| | |
|---|---|
| Raw blocks | 8 × 2^64 keys |
| After Hamming | ≈ 5.31 × 10^18 keys |
| Fleet (assumed) | 1,000 × RTX 4090 |
| Estimated time | **≈ 45 days** @ ~25 GKey/s/GPU (CUDA + Hamming gate) |
| Conservative | ≈ 746 days @ 1.5 GKey/s/GPU |

Regenerate numbers: `python3 research/puzzle71/prng_clusters.py`

## Run

All eight bands sequentially (CPU):

```bash
research/puzzle71/run_prng_clusters.sh
```

One band:

```bash
CLUSTER_PREFIX=7C research/puzzle71/run_prng_clusters.sh
```

Logs: `research/puzzle71/prng_logs/cluster_<PREFIX>.log`  
Progress: `research/puzzle71/prng_logs/cluster_<PREFIX>.progress`  
Hits: `research/puzzle71/prng_logs/cluster_<PREFIX>.found.txt`

Status every 10 s with `| no match` or `| MATCH FOUND`.

GPU cluster: partition each `2^64` band with `--share M/N` across nodes.

```bash
cuBitCrack -c --hamming 12:16 \
  --keyspace 740000000000000000:74ffffffffffffff \
  --share 3/1000 --continue shard_740_003.progress \
  1PWo3JeB9jrGwfHDNpdGK54CRas7fsVzXU
```

(`--hamming` is implemented on the CPU build today; wire the same gate into the
CUDA kernel for fleet runs.)
