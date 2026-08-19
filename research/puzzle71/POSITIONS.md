# Solved-puzzle range positions

Every Bitcoin puzzle `n` is constrained to `[2^(n-1), 2^n)`. The number
that matters for "where in the range was the key" is

```
t = (key − 2^(n-1)) / 2^(n-1)     ∈ [0, 1)
percent of range = 100 t
```

`0%` is the first key in the interval, `100%` would be the last.
A fair random key has mean `50%`, median `50%`, stdev `28.87%`, and
about a quarter of the mass in each quartile.

83 published solved keys: **1–70**, then every 5th through **135**.

## Averages

| set | n | mean | median | stdev | Q1 | Q3 | in upper half |
|---|---:|---:|---:|---:|---:|---:|---:|
| **all solved** | **83** | **50.40%** | **50.00%** | 26.90% | 29.9% | 70.6% | 42 / 83 |
| consecutive 1–70 | 70 | 50.78% | 50.09% | 27.72% | 29.3% | 71.5% | 36 / 70 |
| every 5th (kangaroo) | 27 | 51.67% | 62.20% | 28.28% | 30.1% | 69.6% | 15 / 27 |
| n ≥ 20 (t well resolved) | 64 | 53.78% | 56.85% | 26.49% | 33.3% | 72.2% | 34 / 64 |
| n ≥ 32 | 52 | 50.41% | 47.56% | 26.77% | 28.1% | 70.3% | 25 / 52 |
| n ≥ 50 | 34 | 53.95% | 56.85% | 28.12% | 30.9% | 79.1% | 19 / 34 |
| brute-force 66–69 | 4 | 38.78% | 37.32% | 29.19% | 19.4% | 56.7% | 1 / 4 |
| Uniform ideal | — | 50.00% | 50.00% | 28.87% | 25.0% | 75.0% | 50% |

The headline number: **the found keys sit, on average, at 50% of their
range.** The median is the same. That is exactly what independent
uniform mantissas look like.

## Pattern check

There isn't one that survives a sample of this size.

- **No drift with puzzle number.** Correlation of `t` with `n` is
  `+0.12` on all 83 keys, `+0.19` on 1–70, `−0.05` on `n ≥ 20`.
  Noise.
- **No consecutive memory.** Lag-1 correlation is `+0.06` on 1–70 and
  `+0.02` on the full set. Puzzle 70 at 64.4% does not say anything
  about 71. Puzzle 69 at 0.72% is an independent draw that happened
  to land near the left edge — which is why it was found in a month
  of sequential search, not a rule.
- **Quartiles are flat.** All 83 keys: 17 / 24 / 24 / 18 in
  `[0–25)`, `[25–50)`, `[50–75)`, `[75–100)`. Uniform expects ~20.75
  each; χ² ≈ 2.1 on 3 df, p ≈ 0.56.
- **KS test accepts Uniform.** D = 0.075 vs 5% critical 0.149 (all
  83); D = 0.068 vs 0.163 (1–70).
- **Decade means bounce around 50%** with n = 10 per bin, as they
  must: 39, 42, 66, 46, 43, 63, 57%. The 21–30 cluster at 66% and
  51–60 at 63% look like a "high" run. Ten Uniform samples have
  stdev of the mean `28.87/√10 ≈ 9.1%`, so a 66% decade is a 1.8σ
  bump, not a mechanism.
- **Every-5th (kangaroo) keys are the unbiased subset** — they were
  solved from a revealed public key, so nobody preferentially found
  the ones near 0%. Their mean is still 51.7%. The median 62% is a
  27-point sample wobble; the mean is the stable summary.

What is *not* a generating pattern, just search bias in the
story-telling: 66–69 (the recent address-only brute-force solves)
average 39% because 69 sat at 0.72%. Four points. The next
address-only target is 71; that cluster does not move the 50%
average of the other 79 keys.

## Full table

`5th` marks the kangaroo / dummy-spend series (every 5th puzzle).

| n | % of range | private key (hex) | |
|---:|---:|---|---|
| 1 | 0.0000 | `1` | range is one key |
| 2 | 50.0000 | `3` | |
| 3 | 75.0000 | `7` | |
| 4 | 0.0000 | `8` | first key in the interval |
| 5 | 31.2500 | `15` | 5th |
| 6 | 53.1250 | `31` | |
| 7 | 18.7500 | `4c` | |
| 8 | 75.0000 | `e0` | |
| 9 | 82.4219 | `1d3` | |
| 10 | 0.3906 | `202` | 5th |
| 11 | 12.7930 | `483` | |
| 12 | 31.0059 | `a7b` | |
| 13 | 27.3438 | `1460` | |
| 14 | 28.7109 | `2930` | |
| 15 | 63.9832 | `68f3` | 5th |
| 16 | 57.1960 | `c936` | |
| 17 | 46.2143 | `1764f` | |
| 18 | 51.5724 | `3080d` | |
| 19 | 36.3888 | `5749f` | |
| 20 | 64.6646 | `d2c55` | 5th |
| 21 | 72.7833 | `1ba534` | |
| 22 | 43.4089 | `2de40f` | |
| 23 | 33.4858 | `556e52` | |
| 24 | 72.0032 | `dc2a04` | |
| 25 | 97.8010 | `1fa5ee5` | 5th |
| 26 | 62.5385 | `340326e` | |
| 27 | 66.8184 | `6ac3875` | |
| 28 | 69.6009 | `d916ce8` | |
| 29 | 49.2757 | `17e2551e` | |
| 30 | 92.4414 | `3d94cd64` | 5th |
| 31 | 95.8002 | `7d4fe747` | |
| 32 | 44.0511 | `b862a62e` | |
| 33 | 66.1814 | `1a96ca8d8` | |
| 34 | 64.5306 | `34a65911d` | |
| 35 | 17.0723 | `4aed21170` | 5th |
| 36 | 23.3646 | `9de820a7c` | |
| 37 | 45.8852 | `1757756a93` | |
| 38 | 6.9359 | `22382facd0` | |
| 39 | 17.7705 | `4b5f8303e9` | |
| 40 | 82.5631 | `e9ae4933d6` | 5th |
| 41 | 32.6273 | `153869acc5b` | |
| 42 | 31.6664 | `2a221c58d8f` | |
| 43 | 68.4796 | `6bd3b27c591` | |
| 44 | 75.1319 | `e02b35a358f` | |
| 45 | 13.6667 | `122fca143c05` | 5th |
| 46 | 46.1122 | `2ec18388d544` | |
| 47 | 70.0566 | `6cd610b53cba` | |
| 48 | 35.8607 | `ade6d7ce3b9b` | |
| 49 | 45.3482 | `174176b015f4d` | |
| 50 | 8.5604 | `22bd43c2e9354` | 5th |
| 51 | 82.8555 | `75070a1a009d4` | |
| 52 | 87.2500 | `efae164cb9e3c` | |
| 53 | 50.1840 | `180788e47e326c` | |
| 54 | 10.7387 | `236fb6d5ad1f43` | |
| 55 | 66.7854 | `6abe1f9b67e114` | 5th |
| 56 | 22.7317 | `9d18b63ac4ffdf` | |
| 57 | 91.8545 | `1eb25c90795d61c` | |
| 58 | 38.7617 | `2c675b852189a21` | |
| 59 | 82.1704 | `7496cbb87cab44f` | |
| 60 | 96.8983 | `fc07a1825367bbe` | 5th |
| 61 | 23.6674 | `13c96a3742f64906` | |
| 62 | 69.4986 | `363d541eb611abee` | |
| 63 | 95.0096 | `7cce5efdaccf6808` | |
| 64 | 92.9844 | `f7051f27b09112d4` | |
| 65 | 65.7115 | `1a838b13505b26867` | 5th |
| 66 | 25.6217 | `2832ed74f2b5e35ee` | |
| 67 | 79.7837 | `730fc235c1942c1ae` | |
| 68 | 49.0089 | `bebb3940cd0fc1491` | |
| 69 | 0.7205 | `101d83275fb2bc7e0c` | |
| 70 | 64.3984 | `349b84b6431a6c4ef1` | 5th |
| 75 | 19.3169 | `4c5ce114686a1336e07` | 5th |
| 80 | 82.8929 | `ea1a5c66dcc11b5ad180` | 5th |
| 85 | 9.0344 | `11720c4f018d51b8cebba8` | 5th |
| 90 | 40.2349 | `2ce00bb2136a445c71e85bf` | 5th |
| 95 | 28.8725 | `527a792b183c7f64a0e8b1f4` | 5th |
| 100 | 36.9812 | `af55fc59c335c8ec67ed24826` | 5th |
| 105 | 43.3914 | `16f14fc2054cd87ee6396b33df3` | 5th |
| 110 | 67.9790 | `35c0d7234df7deb0f20cf7062444` | 5th |
| 115 | 51.4942 | `60f4d11574f5deee49961d9609ac6` | 5th |
| 120 | 38.3274 | `b10f22572c497a836ea187f2e1fc23` | 5th |
| 125 | 77.0320 | `1c533b6bb7f0804e09960225e44877ac` | 5th |
| 130 | 62.1997 | `33e7665705359f04f28b88cf897c603c9` | 5th |
| 135 | 71.2132 | `6d9392a16883f90903d5f78da57af07eb2` | 5th |

Nearest the left edge (excluding the 1-key puzzle 1): **10 at
0.39%**, **69 at 0.72%**, **4 at 0%**, **38 at 6.94%**, **50 at
8.56%**, **85 at 9.03%**. Nearest the right edge: **25 at 97.80%**,
**60 at 96.90%**, **31 at 95.80%**, **63 at 95.01%**, **64 at
92.98%**.

## Implication for puzzle 71

The empirical average of every solved key is 50% of its range, and
nothing in the sequence prefers another bin. A 1% window centred
on that average is `t ∈ [0.495, 0.505)`:

```
5fae147ae147ae147b : 6051eb851eb851eb84
```

Reproduce: `python3 research/puzzle71/positions.py` (writes
`positions.json`).
