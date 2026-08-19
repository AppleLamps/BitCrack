# Where the solved keys actually landed

Position inside puzzle `n` is the **top bits of the mantissa**, not the
wallet LSBs:

```
t = (key − 2^(n−1)) / 2^(n−1)     0% = start of the interval, 100% = end
```

Tiny puzzles quantise `t` (puzzle 4 has eight possible keys), so the
clean sample is **n ≥ 32**: 52 keys with at least 31 bits of place
information. That is the set used for histograms, scan statistics, and
the KDE.

## Headline

They land **all over the interval**. Mean **50.4%**, median **47.6%**
(n ≥ 32). Two piles jump out of the histogram — one at **~82.5%** (four
keys inside 0.72 points) and one at **~65%** (five keys, including 70).
A 20,000-draw Monte Carlo of 52 uniform points says both piles are
ordinary: **P(some 1% window holds ≥ 4 keys) = 0.47**. The 1% window
for puzzle 71 stays at the midpoint.

## Histogram, n ≥ 32 (5% bins, 52 keys, 2.6 expected each)

```
  0–  5%   1  #
  5– 10%   3  ###
 10– 15%   2  ##
 15– 20%   3  ###
 20– 25%   3  ###
 25– 30%   2  ##
 30– 35%   2  ##
 35– 40%   4  ####
 40– 45%   3  ###
 45– 50%   4  ####
 50– 55%   2  ##
 55– 60%   0
 60– 65%   3  ###
 65– 70%   6  ######     <- widest pile
 70– 75%   2  ##
 75– 80%   3  ###
 80– 85%   4  ####     <- tightest pile (all four sit in 82.2–82.9)
 85– 90%   1  #
 90– 95%   2  ##
 95–100%   2  ##
```

Quartiles 12 / 15 / 13 / 12. Decile χ² p = 0.73. Top-3 bits of `t`
(eight 12.5% octants) χ² p = 0.49. Nothing rejects Uniform.

A Beta fit on the same 52 points is **Beta(1.26, 1.23)** — slightly
tighter than Uniform(1,1), which if anything *adds* mass at 50%, not
away from it.

## The two piles, named

**82.5% cluster** — four keys in a 0.72-point span:

| n | % of its range | key |
|---:|---:|---|
| 59 | 82.1704 | `7496cbb87cab44f` |
| 40 | 82.5631 | `e9ae4933d6` |
| 51 | 82.8555 | `75070a1a009d4` |
| 80 | 82.8929 | `ea1a5c66dcc11b5ad180` |

Mean `t = 0.8262`. Implied 1% window on puzzle 71:
`748e9ea2d6f1f2bbd5 : 753275ad14629692de`.
Scan statistic: **p = 0.26** that 52 random points pack 4 into some
0.73% window, **p = 0.47** for a 1% window. Not a signal.

**65% pile** — five n ≥ 32 keys, and it contains the nearest solved
neighbour (70):

| n | % of its range | key |
|---:|---:|---|
| 70 | 64.3984 | `349b84b6431a6c4ef1` |
| 34 | 64.5306 | `34a65911d` |
| 65 | 65.7115 | `1a838b13505b26867` |
| 33 | 66.1814 | `1a96ca8d8` |
| 55 | 66.7854 | `6abe1f9b67e114` |

Mean `t = 0.6552`. Implied 1% window:
`699d1e6aa461c90601 : 6a40f574e1d26cdd0a`.
A 5% window that holds 7 of 52 has Monte Carlo **p = 0.70**. Following
puzzle 70 itself was the *worst* causal predictor of the next `t`
(MAE 0.39 vs 0.25 for the constant 50%).

The empty 10-point gap at 52–62% looks equally dramatic and has
**p = 0.14** as a max-gap. Same story: Uniform samples have holes.

KDE with Silverman bandwidth (0.11) peaks near 71% on this sample.
Bootstrap of that mode (400 resamples) runs from **30% to 74%**
(10th–90th percentile) and lands in 45–55% only 2.5% of the time. The
mode is chasing whichever bump the resample keeps. The mean does not.

## Closest puzzles to 71

| n | % of range | how it was solved |
|---:|---:|---|
| 60 | 96.90 | kangaroo (pubkey) |
| 61 | 23.67 | brute |
| 62 | 69.50 | brute |
| 63 | 95.01 | brute |
| 64 | 92.98 | brute |
| 65 | 65.71 | kangaroo |
| 66 | 25.62 | brute |
| 67 | 79.78 | brute |
| 68 | 49.01 | brute |
| 69 | 0.72 | brute, sequential from the left |
| 70 | 64.40 | kangaroo |
| 75 | 19.32 | kangaroo |
| 80 | 82.89 | kangaroo |

Mean of 65–70: **47.5%**. Mean of 66–69 (the recent address-only
brute-force run): **38.8%**, which is four points plus 69 sitting at
0.72% — search-time selection, not a generator.

## Averages, again, with the high-resolution cut

| set | n | mean | median | stdev |
|---|---:|---:|---:|---:|
| all 83 | 83 | 50.40 | 50.00 | 26.90 |
| 1–70 | 70 | 50.78 | 50.09 | 27.72 |
| **n ≥ 32** | **52** | **50.41** | **47.56** | 26.77 |
| n ≥ 40 | 44 | 53.08 | 50.84 | 26.81 |
| every 5th | 27 | 51.67 | 62.20 | 28.28 |
| kangaroo ≥ 65 | 15 | 50.61 | 51.49 | 21.13 |
| Uniform | — | 50.00 | 50.00 | 28.87 |

## Sorted landings, n ≥ 32

| % of range | n | % of range | n | % of range | n |
|---:|---:|---:|---:|---:|---:|
| 0.72 | 69 | 36.98 | 100 | 66.79 | 55 |
| 6.94 | 38 | 38.33 | 120 | 67.98 | 110 |
| 8.56 | 50 | 38.76 | 58 | 68.48 | 43 |
| 9.03 | 85 | 40.23 | 90 | 69.50 | 62 |
| 10.74 | 54 | 43.39 | 105 | 70.06 | 47 |
| 13.67 | 45 | 44.05 | 32 | 71.21 | 135 |
| 17.07 | 35 | 45.35 | 49 | 75.13 | 44 |
| 17.77 | 39 | 45.89 | 37 | 77.03 | 125 |
| 19.32 | 75 | 46.11 | 46 | 79.78 | 67 |
| 22.73 | 56 | 49.01 | 68 | 82.17 | 59 |
| 23.36 | 36 | 50.18 | 53 | 82.56 | 40 |
| 23.67 | 61 | 51.49 | 115 | 82.86 | 51 |
| 25.62 | 66 | 62.20 | 130 | 82.89 | 80 |
| 28.87 | 95 | 64.40 | 70 | 87.25 | 52 |
| 31.67 | 42 | 64.53 | 34 | 91.85 | 57 |
| 32.63 | 41 | 65.71 | 65 | 92.98 | 64 |
| 35.86 | 48 | 66.18 | 33 | 95.01 | 63 |
| | | | | 96.90 | 60 |

## What this does to the 1% window

Targeting either pile would be reading noise. The unique stable 1%
slice is still the interval midpoint:

```
5fae147ae147ae147b : 6051eb851eb851eb84
```

`cpuBitCrack` stays on that range. Reproduce with
`python3 research/puzzle71/reanalyze.py`.
