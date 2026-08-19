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

## Regression rig: solved puzzles

`ladder.sh` runs the solver against real Bitcoin puzzle targets whose keys are
public (40, 45, 50) and compares the recovered key exactly, so a mechanism has to
prove it is *correct* on real secp256k1 targets before its speed is worth
discussing. `--fold` and `--gs` both clear puzzles 40 and 45.

## Gaudry-Schost set geometry: a negative result

The folded walk sits at ~1.45 sqrt(w). The published constants for interval-DLP
methods that use equivalence classes are lower — Galbraith-Ruprai reach
1.36 sqrt(N) by putting the negation map inside a Gaudry-Schost *birthday search*
rather than a kangaroo walk, and narrowing the wild set improves it further. So
the next lever tried was set geometry, not a new symmetry.

`gs_sim.py` is an idealised harness for that question: one sample costs one group
operation, samples are i.i.d. uniform on their set, collisions are detected
exactly, and recovery is checked against the true logarithm. That is the model the
published constants are stated in, so the harness can be validated against them
before it is trusted — and it reproduces them:

| geometry (2^32, 400 paired trials)              | measured   | published |
| ----------------------------------------------- | ---------- | --------- |
| tame/wild, no classes                           | 2.077      | 2.08      |
| classes, tame and wild both full width          | 1.3400     | 1.36      |
| classes, wild width `w/2`                       | 1.2226     | ~1.275    |
| classes, wild width `w/8`                       | 1.2166     | —         |

Narrowing the wild set is worth ~1.1x in that model and saturates once the wild
width is `w/2` or below. Censoring matters here: geometries that shrink the *tame*
set stop covering some logarithms, so their trials time out; those rows print
their give-up count and are excluded, because dropping unsolved trials from a mean
flatters exactly the geometries that fail.

The production implementation is `--gs` (`--gs-wild-shift s` sets the wild seed
window to `w >> s`; it implies `--fold`, since the geometry is only meaningful on
the centred interval). **It does not reproduce the idealised gain.** Paired A/B
against `fold` at the simulator's best plateau (`s = 1`), same jump table, zero
verification failures throughout:

| range | trials | seed | fold        | gs          | speedup |
| ----- | -----: | ---: | ----------- | ----------- | ------- |
| 2^26  |    800 |    5 | 1.4821      | 1.4166      | 1.0437  |
| 2^26  |    800 |    7 | 1.3924      | 1.4019      | 0.9936  |
| 2^26  |    800 |   11 | 1.4018      | 1.4524      | 0.9669  |
| 2^28  |    400 |    5 | 1.4741      | 1.4435      | 1.0206  |
| 2^28  |    400 |    7 | 1.4595      | 1.4982      | 0.9747  |
| 2^28  |    400 |   11 | 1.4491      | 1.4820      | 0.9783  |

Per-arm standard errors are 0.027-0.042 sqrt(w), so every entry above is inside
noise and the sign of the effect flips with the seed. The honest conclusion is
that narrow wild seeding is worth nothing in this solver.

The reason is that the idealised model does not describe our sampler. Gaudry-Schost
gets its constant from samples that are uniform on their set; a herd of long
pseudorandom walks is not that. Each walker leaves its seed window after
`O(window/mean jump)` steps and its position is then set by the walk, not by where
it was seeded, so seed geometry is washed out long before the first collision. The
bounded-walk restart that would enforce the set boundary is implemented and
instrumented (`gsRestarts`), and it confirms the same thing from the other side: at
2^26 with `s = 1` it never fires once, and forcing it to fire needs windows so
narrow (`s >= 7`) that the arm is 3-4x *worse* than `fold`.

Getting the Gaudry-Schost constant would therefore mean replacing the walk
structure with short bounded walks that restart inside their set — a different
solver, not a knob on this one. `--gs` is kept as an opt-in, documented dead end;
the default walk is untouched.

### A correctness bug the extreme settings exposed

Forcing restarts to fire (`--gs-wild-shift >= 7`) produced nonzero
`verificationFailures`, which is supposed to be structurally impossible in folded
mode. It was worth chasing, because a broken invariant there would also silently
destroy real collisions. Instrumentation showed the walker equation
`pos == charge*Heff + off*G` held after seeding, jumps, canonicalisation and
restarts alike; what failed was table canonicalisation:

```text
x collision has differing points (gs=1 old-y=0 new-y=1)
```

The distinguished-point loop runs over the whole herd *after* the folding loop, so
the restart branch's `continue` skipped only the folding loop — the freshly seeded,
possibly odd-`y` point still reached the table that same iteration. An odd-`y`
entry and a later even-`y` entry with the same x then read as a collision between
two different points, and the recovered candidate failed verification. Fixed by
canonicalising the walker immediately after a restart reseeds it; canonicalisation
is now a single named step so that no future reseed path can skip it. The
merge-driven reseed was checked and was never exposed: it reseeds from inside the
distinguished-point loop and its new point is canonicalised on the next step
before it can be tested.

Note what this does *not* affect: `--fold`, which is the merged mechanism, has no
restart path, and none of its runs (including the 800-trial paired benchmarks and
the puzzle ladder) ever reported a verification failure.

## Where this stands

- **Kept, measured, merged:** the folded negation-orbit walk, ~1.3x fewer group
  operations than the charge-balanced herd and ~1.58x fewer than classic
  tame/wild, correct on real puzzle targets.
- **Kept as a negative result:** Gaudry-Schost seed geometry (`--gs`). The
  idealised gain is real and reproduces the literature; it does not survive
  contact with a long-walk herd.
- **Open, in order of expected value:** (1) a bounded-walk sampler that actually
  realises the Gaudry-Schost constant, which is a new solver rather than a knob;
  (2) scaling `J` and the DP parameters with the range, which the fold limitations
  above show is the largest unexploited factor for big intervals; (3) resumable
  distinguished-point state, without which a long run cannot accumulate progress
  across restarts.
- **Tried and rejected:** stacking the secp256k1 endomorphism orbit on the
  negation orbit was considered and not implemented — `lambda` maps the centred
  interval onto a set that is not an interval, so the third symmetry does not
  shrink the domain being searched and the `sqrt(6)` figure it suggests is not
  attainable this way.

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
