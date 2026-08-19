# Puzzle 71: a 1% window

The range-position of every published solved key is tabulated in
[POSITIONS.md](POSITIONS.md). A second pass that looks only at
**where** the keys sat (the top bits of each mantissa, n ≥ 32, scan
statistics on the 82% and 65% piles) is in
[REANALYSIS.md](REANALYSIS.md). Short version: **83 solved keys
average 50.40% of the way through their interval** (median 50.00%;
n ≥ 32 mean 50.41%). The two visible piles are typical of Uniform
samples (Monte Carlo p = 0.47 that some 1% window holds four keys).
That is the empirical justification for the midpoint window below.

Puzzle 71 is the lowest unsolved address-only Bitcoin puzzle.
The private key sits in `[2^70, 2^71)` and the public key is not
on chain, so kangaroo / BSGS do not apply. The interval is
`2^70 ≈ 1.1806e21` keys; 1% of that is still `≈ 1.1806e19` keys,
which is not searchable here. The job is therefore to *name* a
1%-wide subinterval that is the least-wrong place to look.

## Prediction

| | hex |
|---|---|
| address | `1PWo3JeB9jrGwfHDNpdGK54CRas7fsVzXU` |
| full range | `400000000000000000 : 7fffffffffffffffff` |
| **1% window** | **`5fae147ae147ae147b : 6051eb851eb851eb84`** |
| center | `600000000000000000` |
| fractional position | `t ∈ [0.495, 0.505)` |

The window is the unique 1% slice around the interval midpoint,
i.e. `2^70 + 2^69 ± 2^70/200`. Under the model the data actually
supports — independent Uniform mantissas — this is the
minimum-MAE choice of a 1%-wide bin.

```
cpuBitCrack -c --keyspace 5fae147ae147ae147b:6051eb851eb851eb84 \
    1PWo3JeB9jrGwfHDNpdGK54CRas7fsVzXU
```

This is **not** a cryptographic break. A Uniform key lands in any
given 1% bin with probability 1%. The rest of this note is the
argument that no tested generator or position model beats that
prior, so the midpoint is the honest 1% answer.

## What the creator said

> "There is no pattern. It is just consecutive keys from a
> deterministic wallet (masked with leading 000...0001 to set
> difficulty)."
> — saatoshi_rising, BitcoinTalk

The operational reading, used by every serious write-up of this
puzzle, is: take consecutive HD-wallet keys `K_n`, then

```
k_n = 2^(n-1) + (K_n & (2^(n-1) - 1))
```

The leading `1` at bit `n-1` is forced; the mantissa
`m_n = k_n - 2^(n-1)` is the low `n-1` bits of the wallet key.
Write `t_n = m_n / 2^(n-1) ∈ [0,1)` for the position inside the
puzzle-`n` interval. "Within 1% of the range" means a window of
width `0.01` on `t_71`.

If the wallet is a real HD construction (BIP32 / Electrum 2 /
Armory), `K_n` is HMAC-SHA512 output and the `t_n` are
independent Uniform samples. Recovering `K_71` from `{K_1..K_70}`
then requires the chain code, which is 256 bits of unobserved
entropy. That is the reason 83 leaked children do not give the
84th.

## Structural generators that die on the public keys

`analyze.py` reconstructs every published solved key
(`keys.json`: 1–70, then every 5th through 135) and tests the
generators that would actually let us *compute* `k_71`.

| generator | prediction | result |
|---|---|---|
| one 256-bit `K` truncated to `n` bits | low bits of `k_{n+1}` nest inside `k_n` | 8 failures in the first 8 consecutive pairs |
| `K_n = a n + b` | `m_n ≡ a n + b (mod 2^{n-1})` | 36/37 mismatches |
| LCG `K_n = a K_{n-1} + c (mod 2^256)` | `m_n ≡ a m_{n-1} + c (mod 2^{n-2})` | fails at every invertible window `n0 ∈ {16,24,40,56}` |
| additive `m_n = m_{n-1}+m_{n-2}` | same, reduced mod `2^{n-2}` | 66/68 mismatches |
| multiplicative `m_n = r m_{n-1}` | constant `r` on low 16 bits | new `r` every `n` |
| SHA256 / HMAC-SHA512 of `n` with small seeds | `mask(n, H(seed\|\|n)) = k_n` | 0 hits on puzzles 8..40 |
| next prime after `2^{n-1}` | — | no matches |
| Fibonacci, then masked | — | dies at `n=6` |
| LFSR on the LSB stream | Berlekamp–Massey linear complexity | `L/len = 0.500` on 504 bits (the random value) |

The truncated-output Hidden Number Problem does not apply either.
BIP32 is `k_i = k_parent + HMAC(chain_code, ·, i)`, and the HMAC
term randomises every bit we can see. Low bits of children are
not noisy multiples of a secret; they are the secret plus a
full-width pseudorandom pad.

## Position models, cross-validated

For each solved puzzle `n ≥ 40`, predict `t_n` from `{t_k : k < n}`
and ask two questions: mean absolute error, and how often the
error is `< 0.01` (a hit inside a 1% window around the point).
A Uniform key versus a point in the interior of `[0,1]` is a
1% hit with probability 0.02. Predicting the constant `0.5`
has theoretical MAE `0.25`.

| model | 1% hits, n=40..70 | MAE |
|---|---:|---:|
| **constant 0.5** | **2/31** | **0.2537** |
| KDE mode | 1/31 | 0.2542 |
| train mean | 0/31 | 0.2558 |
| linear in `n` | 0/31 | 0.2576 |
| train median | 0/31 | 0.2580 |
| AR(1/2/3) | 0/31 | 0.263–0.273 |
| Weyl `{α n + β}` | 1/31 | 0.2804 |
| cubic | 0/31 | 0.3073 |
| persist `t_{n-1}` | 1/31 | 0.3884 |
| local-linear map `t_n ↦ t_{n+1}` | 1/44 | 0.2549 |

Nothing beats the constant. Persist (the "71 is near 70" novel
theory) is the worst of the lot: `t_70 = 0.644` and `t_69 = 0.0072`
already show that consecutive positions do not stick. Lag-1
correlation on puzzles 1–70 is `0.058`.

## The Uniform check, in numbers

Puzzles 1–70, which are a complete consecutive sample and so
not biased by "which every-5th key had a dummy spend":

| statistic | observed | Uniform`[0,1]` |
|---|---:|---:|
| mean `t` | 0.5078 | 0.5000 |
| median `t` | 0.5009 | 0.5000 |
| stdev `t` | 0.2772 | 0.2887 |
| KS `D` | 0.0684 | 5% crit 0.1626 |
| upper-half count | 36/70 | `p ≈ 0.81` |
| bit biases with `\|z\| > 2.5` | 0 of ~62 | ≈ 0.8 expected |
| `corr(n, t)` | 0.189 | 0 |

The every-5th series (kangaroo-solved, so no "found because it
was near 0" selection effect) is the same story: `t` runs from
0.4% (puzzle 10) to 97.8% (puzzle 25) with no drift toward 71.
Hamming weights sit on `1 + (n-1)/2`, the curve of a random
`n`-bit string with the top bit stuck on.

Puzzles 60–70, for the record, are not a pattern:

```
60  96.90%    61  23.67%    62  69.50%    63  95.01%
64  92.98%    65  65.71%    66  25.62%    67  79.78%
68  49.01%    69   0.72%    70  64.40%
```

Puzzle 69 sitting at 0.72% is why it fell in a month of
sequential search. It is not a hint that 71 is also near the
left edge; the btcpuzzle.info pool is already assigning 71's
ranges essentially uniformly and has covered ~0.91% of them.

## Novel theories that were tried and parked

1. **Weyl / Beatty sequence.** `t_n ≈ {α n + β}` with a grid
   search over `α, β` including `φ-1`, `√2-1`, `{e}`, `{π}`.
   In-sample MSE looks fine; causal MAE is *worse* than the
   constant. Classic overfit of 70 noisy points.
2. **KDE densest 1% bin.** With 70 Uniform samples the "densest"
   1% window is a noise bump. It does not survive leave-one-out.
3. **1-NN continuation from `t_70`.** Nearest historical
   predecessor of 0.644 maps to whatever that predecessor's
   successor was. Persist/1-NN lost the causal race.
4. **Top-7-bit MLE from per-bit empirical `p_1`.** No bit has
   `|z| > 2.5`; the MLE is 0.5 on every high bit.
5. **Same truncated integer, high bits instead of low bits.**
   Contradicted by the creator's "leading 000…0001" and by
   the hex form of every published key.
6. **Parent recovery from truncated BIP32 children.** See HNP
   paragraph above; the pad is the same width as the secret.
7. **Endomorphism / folded-orbit kangaroo.** This repository's
   `--fold` solver wants a public key. Puzzle 71 has not spent.

## Why the midpoint, not "we refuse to pick"

A Uniform prior makes every 1% bin equally likely. The extra
structure we *do* have is a well-defined loss: the causal MAE
of a point predictor. That loss is minimised at `t = 0.5`
(sample mean 0.5078, sample median 0.5009, both inside the
standard error `0.289/√70 ≈ 0.035`). The 1% window around
that point is

```
[2^70 + 2^69 - 2^70/200,  2^70 + 2^69 + 2^70/200)
= [0x5fae147ae147ae147b, 0x6051eb851eb851eb84]
```

If a later puzzle in 1–70 had shown a surviving LCG, a short
LFSR, or a causal predictor with 1% hit-rate ≫ 0.02, the
window would have moved. None of them did.

## Reproduce

```
python3 research/puzzle71/analyze.py
```

Writes `research/puzzle71/analysis.json` with the full tables.
`window.txt` is the keyspace line for BitCrack.

## Sources

- Creator quote and solved-key table: [privatekeys.pw puzzle tx](https://privatekeys.pw/puzzles/bitcoin-puzzle-tx), [HomelessPhD/BTC32](https://github.com/HomelessPhD/BTC32)
- Pool status (0.91% of 33,554,432 ranges, random assignment): [btcpuzzle.info/puzzle/71](https://btcpuzzle.info/puzzle/71)
- Puzzle 70 key: [privatekeyfinder.io/bitcoin-puzzle/70](https://privatekeyfinder.io/bitcoin-puzzle/70)
- Prior seed/PRNG exhaustion (32-bit MT, glibc `rand`, Java LCG, BIP32 passphrases): [mlartab/bitcoin-puzzle-systematic-analysis](https://github.com/mlartab/bitcoin-puzzle-systematic-analysis)
