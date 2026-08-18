# Kangaroo: charge-balanced interval DLP solver

A Pollard kangaroo solver added alongside BitCrack's keyspace searcher, implementing the
charge-balanced herd.

## How this differs from BitCrack proper

| | BitCrack (`cuBitCrack` / `clBitCrack`) | `kangaroo` |
|---|---|---|
| Input | address or hash160 | **public key** |
| Method | exhaustive keyspace scan | pseudorandom walks with collision detection |
| Cost over a width `w` interval | `O(w)` | `O(sqrt(w))` |
| Memory | negligible | distinguished-point table |

The two solve different problems. If you only have an address, the public key is unknown
and a kangaroo walk has nothing to walk toward, so BitCrack is the tool. If the public key
has been exposed (a spent output, a published puzzle key), the kangaroo path is
quadratically cheaper.

## The idea

Seed walker `j` at `h^{k_j} · g^{c_j}`. Its exponent is

```
p_j(t) = k_j·x + c_j + d_j(t)
```

with `d_j` the accumulated jump distance. A jump multiplies by `g^s`, which moves `d_j` and
leaves `k_j` alone. **The charge `k_j` is a conserved quantity of the walk**, fixed at
seeding.

When two walkers collide:

```
(k_i − k_j)·x = (c_j + d_j) − (c_i + d_i)   (mod n)
```

This determines `x` if and only if `k_i ≠ k_j`. Equal charges cancel `x` and yield nothing.

A herd split 50/50 between tame (`k=0`) and wild (`k=+1`) kangaroos, which is what every
production implementation runs, therefore discards half of every collision it produces.

An interval of width `w` admits charges with `|k| ≤ 1`, since a charge-`k` walker occupies a
window of width `|k|·w`. Equivalently, the affine maps preserving an interval are the
identity and the reflection `x ↦ (a+b) − x`, so the symmetry group is `Z/2` and the usable
charge set is exactly `{0, +1, −1}`. Balancing the herd across all three raises the
productive share of collisions from `1/2` to `2/3`, and since collisions accumulate as the
square of the work, cost improves by up to `sqrt(4/3)`.

The third species costs one point negation at setup:

```
tame       k =  0    g^c
wild       k = +1    h · g^c
reflected  k = −1    h^{-1} · g^{a+b+c}
```

## Build

```
make BUILD_CPU=1 dir_kangaroo
```

The binary lands in `bin/kangaroo`. It is also built by the default `make` target.
Windows: `KangarooLib.vcxproj` and `Kangaroo.vcxproj` are included.

## Usage

```
kangaroo -k <pubkey> [--range a:b | --bits N] [options]

  -k, --pubkey <hex>     target public key, compressed or uncompressed
      --range <a:b>      hex search interval, inclusive
      --bits <N>         shorthand for [2^(N-1), 2^N - 1]
      --charges <2|3>    2 = classic tame/wild, 3 = charge balanced (default 3)
      --herd <N>         walkers in the herd (default 96)
  -t, --threads <N>      worker threads for the batch point add
  -d, --dp-bits <N>      distinguished point bits (default auto)
      --stride-bits <N>  log2 of the mean jump (default auto, log2 sqrt(w))
      --jumps <N>        jump table size, power of two (default 32)
      --mix <T:W:R>      herd composition by charge (default 1:1:1)
      --pool <N>         reseed pool size (default auto, 8x herd)
      --spread <TWR>     seed spread in pool draws per class (default 211)
      --seed <N>         PRNG seed, for reproducible runs
      --benchmark <N>    N random solves per arm, A/B the charge sets
  -o, --out <file>       append the found key to a file
```

Solve a key whose public key is known to sit in a 40 bit range:

```
$ ./bin/kangaroo -k 03a2efa402fd5268400c77c20e574ba86409ededee7c4020e4b9f0edbee53de0d4 --bits 40 --herd 128 -t 4

Herd        : 128 walkers, 3 charge classes {0,+1,-1}
Expected    : ~1,557,055 group operations

FOUND  private key : 000000000000000000000000000000000000000000000000000000E9AE4933D6

  stats   steps=1,299,584   1.7527*sqrt(w)   dp=163184  merges=1  table=163182  4.2s
```

## Reproducing the A/B result

`--benchmark N` generates `N` random keys and solves each one **twice**, once with the
classic 2-charge herd and once charge balanced, using the same key and the same herd seed
for both arms. The only difference between the arms is the charge composition.

```
./bin/kangaroo --bits 32 --benchmark 200 --herd 96 --dp-bits 5 -t 4 --seed 20260818
```

### Measured

All runs on real secp256k1 through this module. Every reported key is verified against the
target point before it counts, and `verification failures` was 0 in every run below.

| range | trials | 2-charge `{0,+1}` | 3-charge `{0,+1,-1}` | speedup |
|---|---|---|---|---|
| `2^24`, seed 11111 | 600 | 2.3387 +/- 0.0474 | 2.2041 +/- 0.0470 | 1.061 +/- 0.031 |
| `2^24`, seed 77777 | 600 | 2.4137 +/- 0.0515 | 2.1861 +/- 0.0471 | 1.104 +/- 0.034 |
| `2^28`, seed 24680 | 250 | 2.3226 +/- 0.0773 | 2.1792 +/- 0.0735 | 1.066 +/- 0.051 |
| **pooled `2^24`** | **1200** | **2.376** | **2.195** | **1.083 +/- 0.022** |

Units are multiples of `sqrt(w)` in walk group operations. Seeding and respawn cost is
tracked separately in `setupOps` and excluded, since it is a fixed startup charge that
vanishes at realistic range sizes.

**The merge counter is the cleaner signal.** Expected same-charge merges before a solve is
`(1-alpha)/alpha`, which is 1.00 at `alpha=1/2` and 0.50 at `alpha=2/3`. Measured:

```
2-charge   1.43, 1.42 merges/solve
3-charge   0.85, 0.92 merges/solve
```

The ratio, 1.66, is the mechanism showing up directly: the charge-balanced herd wastes
substantially fewer of its collisions. Both arms run above the ideal because walkers seeded
from a shared pool are correlated, which inflates within-class merging in both.

### Why the gain is 8% and not 15%

`--benchmark` reports which charge pair supplied the solving collision. If overlap were
symmetric across pairs, each of the three would supply a third:

```
3-charge {0,+1,-1}   tame/wild 0.39   tame/refl 0.37   wild/refl 0.24
```

The wild/reflected pair is the weak one, and the geometry says why. A charge `+1` walker
occupies `[x, x+w)` and a charge `-1` walker occupies `[a+b-x, a+b-x+w)`. The two windows
are offset by `(a+b) - 2x`, which is uniform on `(-w, w)`, so for `x` near either endpoint
they barely touch. Only the tame class has a position independent of `x`, which makes it the
bridge that guarantees overlap for every `x`.

Feeding the measured pair weights `(1, 0.94, 0.49)` back into `alpha = sum over unequal
pairs` predicts a gain near 1.05 to 1.08 rather than `sqrt(4/3) = 1.155`, which is what the
runs show.

Two consequences worth knowing:

- **Never drop the tame class.** A `{+1,-1}` herd has the same nominal `alpha = 1/2` as the
  classic split, so naive theory predicts parity. In simulation it costs `3.30*sqrt(w)`,
  roughly 45% worse than either. Charge diversity without spatial control is not enough.
- **Composition is a flat knob.** Sweeping `--mix` between 33% and 50% tame moves cost by
  less than the error bars, with a shallow minimum near 43%. `1:1:1` is a fine default and
  does not need tuning.

### End to end

Both of these recover the key from the public key alone:

```
0xC0FFEE12    32 bit range   18,816 group operations
0xE9AE4933D6  40 bit range    1.3M group operations, 4.2s on 4 CPU threads
```


## Implementation notes

**Jump index and DP test read disjoint bits.** The distinguished point test reads the low
bits of word 0 of the x coordinate; the jump index is a mix of words 1, 2, 5 and 6. If both
read the same bits, every distinguished point would take the same jump and the walk would
degenerate. This is an easy bug to ship and a hard one to notice, since the solver still
works, only slower.

**One inversion per chunk, not per step.** The herd steps in lockstep through
`secp256k1::addPointsIndependent`, which Montgomery-batches the modular inversions across
the whole herd and splits the batch over OpenMP workers. Larger herds amortize the inversion
better, so `--herd` is a throughput knob as well as a statistical one.

**Respawns are cheap.** A same-charge collision merges two walkers permanently, so one of
them becomes dead weight and has to be respawned. A naive respawn costs a scalar
multiplication, roughly `1.5·log2(w)` point operations, which at small ranges swamps the
walk itself. Instead the solver precomputes a pool of random `(u, u·G)` pairs at startup, so
a respawn costs one or two point additions. Seeding cost is reported separately from walk
steps.

**The charge solve needs no general modular inverse.** Charge differences are `±1` and `±2`.
`n` is odd, so `inv(2) = (n+1)/2` and everything else is a negation.

**Every recovered key is verified** against the target point before it is reported.

## Limitations and next steps

- CPU only. The charge logic is independent of the arithmetic backend, so a CUDA or OpenCL
  herd is a mechanical port: it changes where points are added, not how charges are assigned
  or how collisions are solved.
- The field arithmetic is BitCrack's generic `uint256`, not specialised secp256k1 code, so
  raw throughput is well below tuned kangaroo implementations. The A/B ratio is unaffected,
  since both arms share the same arithmetic.
- The negation map (`P ~ −P`) spends the same symmetry harder and wins where it is
  available, at `sqrt(2)`, but it introduces fruitless cycles that need detection and escape
  machinery. The two do not stack: folding collapses charges `+1` and `−1` into one class.
  The reflected herd is cycle free and works in any group.
- Multi-dimensional (GLV-decomposed) ranges have a much larger symmetry group than `Z/2`,
  so the charge gain there should be strictly larger than what this module measures.
