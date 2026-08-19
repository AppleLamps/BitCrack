# Folded-orbit interval walks

Research notes for the `--fold` mechanism added to the Kangaroo solver. This file
records the derivation, the measurements that support it, and the ideas that were
tried and abandoned. Everything below was measured with the code in this
repository; the simulator numbers are labelled as such and are not evidence about
production behaviour.

## Problem and cost model

Given `H = xG` on secp256k1 and an interval `x in [a,b]` of width `w`, recover
`x`. The unit of cost is one group operation (one point addition), because the
walk is nothing but a chain of additions and every mechanism below performs the
same batched-affine addition per step. The solver reports:

- `steps` — walk additions,
- `setupOps` — seeding and reseeding additions, of which `poolOps` is the one-off
  generator-multiple pool,
- `walkOps() = steps + (setupOps - poolOps)` — the metric used for mechanism
  comparison. The pool is excluded because both arms pay it identically and it is
  amortised to nothing on any range worth solving.

The baseline in this repository is the charge-balanced herd: each walker carries a
charge `k in {0,+1,-1}` and an exponent `p_j(t) = k_j x + c_j + d_j(t)`. A
collision between two walkers of different charge yields
`(k_i - k_j) x = (c_j + d_j) - (c_i + d_i) mod n`; equal charges cancel `x` and
are wasted work (a "merge"). Classic Pollard kangaroo is the two-charge case
`{0,+1}`.

## Bottleneck

Expected work is `Theta(sqrt(w))` group operations and the whole game is the
constant in front. That constant is set by how much of the interval the herd must
cover before two differently-charged walkers land on the same point. Charge
balancing improves the constant by raising the fraction of collisions that are
productive; it does not shrink the domain being searched. The domain itself is
the larger lever, and it has an unused symmetry.

## Mechanism: walk the negation orbit

On an elliptic curve, `P` and `-P` share an x-coordinate. Everything the walk
uses to decide its next move is a function of the x-coordinate only: the jump
index and the distinguished-point test. So the walk is already, in effect, a walk
on the orbit `{P,-P}` — but the bookkeeping is not, and mirrored walkers are
treated as unrelated points.

Folding makes that explicit. After every step the walker is canonicalised to the
even-`y` representative of its orbit, and the exponent bookkeeping follows the
same map:

```text
(k, offset) -> (-k, -offset)   when the point is negated
```

The walk now takes place on the quotient of the curve by negation, whose relevant
domain has about `w/2` elements instead of `w`, so the expected work falls by a
factor of `sqrt(2)`: a ceiling of **1.4142x**, which the benchmark prints as
`predicted ceiling`.

### Centering is not optional

Negation is a symmetry of the exponent interval only when that interval is
centred on zero: `x -> -x` maps `[-m, m]` to itself but maps `[a,b]` somewhere
else entirely. The implementation therefore solves a shifted problem:

```text
mid   = a + floor(w/2)
H_eff = H - mid*G
x'    = x - mid          (so x' lies in a zero-centred interval)
x     = x' + mid
```

The recovered candidate is verified against `H_eff` and then, after translation,
re-verified against the original `H`, so a centering mistake cannot produce a
wrong answer — only a failure to find one. An earlier version of this work folded
the raw interval `[a,b]` and was *slower* than the baseline; the centering
transform is what turned it into a win.

### Fruitless cycles, and why they must be handled

Folding introduces a failure mode absent from the plain walk. If a step is
followed by a canonicalising negation, the next jump can undo the previous one and
the walker enters a short cycle, most often a 2-cycle. The per-step probability of
entering one is about `1/(2J)` for a jump table of `J` entries, so a walk of `S`
steps expects roughly `S/(2J)` cycles. Measured against that model:

| range | jumps `J` | folded steps | cycles/solve predicted | measured |
|---|---|---|---|---|
| 2^30 | 4096 | 4.9e4 | 6.0 | 4.9 |
| 2^34 | 4096 | 2.1e5 | 25.9 | 19.1 |

The model holds to within a small constant. Each walker keeps a short history of
orbit digests; on a repeat it takes a deterministic escape jump derived from the
minimum digest in the history (deterministic so that two walkers meeting in the
same cycle still escape to the same place, preserving collisions), then clears its
history. Without cycle handling the folded walk does not terminate in reasonable
time — see the negative results.

Cycle-history length turns out not to matter: at 2^30 with `J = 4096`, history 2
and history 8 gave *identical* step counts (1.4354 sqrt(w)), confirming that the
cycles encountered in practice are almost entirely 2-cycles.

## Production results

secp256k1, real point arithmetic, 8 threads, `walkOps` accounting, cost in units
of `sqrt(w)` group operations, `+/-` is one standard error over trials. All runs
reported zero verification failures.

| range | trials | jumps | baseline | folded | speedup |
|---|---|---|---|---|---|
| 2^30 | 200 | 4096 | 1.9609 +/- 0.0739 (3-charge) | 1.4945 +/- 0.0581 | **1.3121 +/- 0.0710** |
| 2^30 | 200 | 4096 | 2.2786 +/- 0.0893 (classic 2-charge) | 1.4465 +/- 0.0537 | **1.5753 +/- 0.0850** |
| 2^32 | 100 | 4096 | 1.8949 +/- 0.1064 (3-charge) | 1.5240 +/- 0.0863 | 1.2434 +/- 0.0992 |
| 2^34 | 60 | 4096 | 1.9632 +/- 0.1276 (3-charge) | 1.6166 +/- 0.1185 | 1.2144 +/- 0.1189 |

Including reseed cost changes nothing material: at 2^30 the step speedup 1.3121
becomes 1.3012 on `walkOps`, even though folding merges far more often (15.5
merges/solve versus 2.1 for the baseline) because folding maps wild walkers onto
reflected ones and so puts more of the herd in collision-compatible classes.

Per-step cost was checked separately, since folding adds a conditional field
negation, a 64-bit digest and a few comparisons per step. At 2^48 on a single
key, throughput was 3.40M ops/s for the baseline and 3.35M for the folded walk
(3 seeds each, spread 2.8-4.0M) — parity within measurement noise, so the
step-count win carries over to wall clock.

The gap between the measured 1.21-1.31x and the 1.4142x ceiling is the cost of
cycle escapes and extra merges, both of which scale with `steps/J`. Raising `J`
buys back part of it: at 2^30, folded cost falls from 1.6036 sqrt(w) at `J = 1024`
(18.9 cycles/solve) to 1.3424 at `J = 16384` (1.6 cycles/solve). For this reason
folded mode raises the jump table to at least 1024 entries unless `--jumps` was
given explicitly, and says so on stdout.

Two results that constrain the design:

- **The third charge class should be dropped when folding.** `fold` versus
  `fold3` at 2^30: 1.3558 versus 1.5850 sqrt(w) — the reflected class is worse
  than useless once canonicalisation already moves walkers between `+1` and `-1`.
  Folded mode therefore uses two classes.
- **The 1.8259x measured at `J = 16384` is not a real 1.8x.** It exceeds the
  `sqrt(2)` ceiling only because the *baseline* degrades at that table size
  (2.4511 sqrt(w) versus 1.9609 at `J = 4096`). The headline number is the
  matched-configuration run at each arm's sane setting, not this one.

End-to-end check outside the benchmark harness: a 40-bit key solved through the
normal CLI (`-k <pubkey> --bits 40 --fold`) returned the correct private key in
550,272 steps (0.7422 sqrt(w), 248 cycle escapes).

## Simulator and negative results

`sim_interval_dlp.py` is an exponent-space model (it approximates the orbit by
`abs(exponent)`) used to triage mechanisms cheaply; `opt.py` and `sweep.py` drive
it. It is not evidence about production, and where the two disagree, production
wins. It did correctly rank folding above both charge-balanced and classic walks,
which is why folding was implemented.

Ideas tried and rejected:

- **Folding without cycle handling** — times out or is far worse than the
  baseline. This is the single most important failure mode of the mechanism.
- **Log-uniform ("Levy") jump-magnitude spectra** — no consistent gain, and
  higher cycle counts, since small jumps are exactly the ones that cycle.
- **Time-varying / clocked jump schedules** — reduced cycles as intended, but
  the reduction in productive collisions cancelled the benefit.
- **Per-class seed-density shaping** — the knob exists in the simulator but was
  not swept far enough to claim anything; no production implementation.

## Limitations

- The negation map is well known in the ECDLP literature (it is standard in
  Pollard rho on curves, and interval methods such as Gaudry-Schost exploit
  interval symmetry). The contribution here is a correct, cycle-handled,
  centre-shifted implementation inside this solver plus matched-accounting
  measurements — not a new idea about curves.
- All measurements are at 2^30-2^48 on one 8-thread machine. The `sqrt(w)`
  normalisation is expected to make the constant range-independent, and 2^30 to
  2^34 is consistent with that, but nothing here has been measured near
  cryptographic sizes.
- The observed speedup shrinks slightly as the range grows (1.31 -> 1.21 from
  2^30 to 2^34) because cycle and merge overhead scale with `steps/J` while `J`
  is fixed. Solving large ranges well would mean scaling `J` (and the DP
  parameters) with the range, which this work has not tuned.
- No claim of asymptotic improvement: the mechanism changes the constant, not
  the `Theta(sqrt(w))` exponent.
