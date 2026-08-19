"""
Exponent-space simulator for interval-DLP solvers (research harness).

Why this is faithful.  A walker's state is a group element h^k g^c, uniquely
determined by its exponent p = k*x + c + d.  Everything an implementation reads
off the point (jump index, distinguished-point test, the canonical choice
between P and -P) is a pseudorandom function of the point, hence of p.
Replacing "hash of the x coordinate" by "hash of the exponent orbit" preserves
the collision statistics while costing an integer multiply instead of a field
inversion, which is what makes mechanism search affordable.

Coordinates are centred: the target x is uniform on [-w/2, w/2), so the interval
reflection e -> -e is exactly negation, and the x-only table key is exactly the
orbit representative under it.

Arms
  plain   monotone positive jumps, table keyed by |e| (x-only storage), charges
          {0,+1} or {0,+1,-1}.  This is KangarooLib's mechanism.
  fold    negation-map folded walk: the walker holds the canonical point of the
          orbit {P,-P}, the jump index reads the orbit, and the canonical sign
          bit b(f) is a pseudorandom function of the orbit.  Mirrored walkers
          coalesce, which is what makes the folded region half as wide; fruitless
          cycles are the price and are detected/escaped explicitly.

Cost is reported in walk group operations divided by sqrt(w).
"""

import argparse
import json
import math
import random
import statistics
import sys

MASK64 = (1 << 64) - 1


def mix64(z):
    z = (z + 0x9E3779B97F4A7C15) & MASK64
    z = ((z ^ (z >> 30)) * 0xBF58476D1CE4E5B9) & MASK64
    z = ((z ^ (z >> 27)) * 0x94D049BB133111EB) & MASK64
    return z ^ (z >> 31)


class Oracle:
    """Pseudorandom functions of the point orbit, keyed per run.

    The jump index, the DP test and the canonical sign bit must read independent
    bits: if the jump index shared bits with the DP test, every distinguished
    point would take the same jump and the walk would degenerate.
    """

    def __init__(self, seed, jump_count, dp_bits):
        self.key = mix64(seed ^ 0xA5A5A5A5A5A5A5A5)
        self.jmask = jump_count - 1
        self.dmask = (1 << dp_bits) - 1

    def jump(self, f):
        return mix64((f & MASK64) ^ self.key) & self.jmask

    def canon_positive(self, f):
        # b(f) = 0 means the canonical representative of {P,-P} is +f
        return (mix64((f & MASK64) ^ (self.key ^ 0x1234567891ABCDEF)) >> 19) & 1 == 0

    def is_dp(self, f):
        return ((mix64((f & MASK64) ^ (self.key ^ 0x9E3779B97F4A7C15)) >> 13) & self.dmask) == 0


class Walker:
    __slots__ = ("k", "o", "hist", "age")

    def __init__(self, k, o):
        self.k = k
        self.o = o
        self.hist = []
        self.age = 0


class Result:
    def __init__(self):
        self.steps = 0
        self.setup = 0
        self.dp = 0
        self.merges = 0
        self.cycles = 0
        self.table = 0
        self.pair = None
        self.solved = False


def solve(x, w, arm, seed, max_steps):
    rng = random.Random(seed)
    herd_size = arm["herd"]
    jump_count = arm["jumps"]
    fold = arm["fold"]
    charges = arm["charges"]
    restart = arm.get("restart", 0)          # 0 = never (pure kangaroo)
    spread = arm.get("spread", (2, 1, 1))
    hist_len = arm.get("hist", 6)            # cycle detection window (fold only)
    mirror_read = arm.get("mirror_read", True)
    clock = arm.get("clock", 1)              # >1: time-varying disjoint jump sets

    root = math.sqrt(w)
    # mean jump.  plain walks want mean ~ sqrt(w)/herd-ish so the herd spans the
    # interval; folded walks diffuse, so the knob is swept separately.
    stride = arm.get("stride", 0) or max(1, int(root / 2))
    dp_bits = arm.get("dp_bits", 0)
    if not dp_bits:
        dp_bits = max(0, int(round(math.log2(max(1.0, root / 512.0)))))
    orc = Oracle(seed, jump_count, dp_bits)
    # clock > 1 gives each time slot its own disjoint magnitude set, which makes
    # immediate reversal (the dominant fruitless cycle) impossible by
    # construction; magnitudes are kept distinct across slots by construction.
    #
    # Jump magnitude spectrum.  "uniform" is what every kangaroo implementation
    # uses: magnitudes uniform in [1, 2*stride], mean = stride.  A folded walk is
    # diffusive rather than monotone (the canonical sign is unbiasable, see
    # README), so with a uniform spectrum it only covers stride*sqrt(t) ground in
    # t steps, and that sqrt eats most of the folding gain.  "logu" draws
    # magnitudes log-uniformly over [1, 2*stride], i.e. a scale free spectrum:
    # small steps keep the local visit density that catching needs, while the
    # rare large steps restore near ballistic coverage.
    spectrum = arm.get("spectrum", "uniform")

    def draw_mag():
        if spectrum == "logu":
            top = math.log(2.0 * stride + 1.0)
            return max(1, int(math.exp(rng.random() * top)))
        return rng.randrange(1, 2 * stride + 1)

    magsets = []
    for c in range(clock):
        magsets.append([clock * draw_mag() + c for _ in range(jump_count)])
    mags = magsets[0]

    half = w // 2

    # Seed density shaping.  scale[c] multiplies the half width a class draws
    # from; draws[c] uniforms are summed, so 1 gives a box and 2 a triangle.
    # Under folding the tame density on the folded interval is flat while the
    # wild density spills past w/2 by |x|, so the two are not matched and the
    # scales are worth optimising.
    scale = arm.get("scale", (1.0, 1.0, 1.0))

    def draw(n_draws, sc):
        span = max(1, int(half * sc))
        s = 0
        for _ in range(n_draws):
            s += rng.randrange(-span, span)
        return s

    def seed_walker(k):
        idx = 0 if k == 0 else (1 if k == 1 else 2)
        return Walker(k, draw(spread[idx], scale[idx]))

    if arm.get("mix"):
        pattern = []
        for k, c in zip((0, 1, -1), arm["mix"]):
            pattern += [k] * c
    else:
        pattern = [0, 1] if charges == 2 else [0, 1, -1]
    herd = [seed_walker(pattern[j % len(pattern)]) for j in range(herd_size)]

    res = Result()
    res.setup += herd_size
    table = {}

    def check(k1, o1, k2, o2):
        """Readings of a table hit.  The two exponents are equal, or (because
        x-only storage cannot tell P from -P) they are negatives of each other."""
        readings = [(k1 - k2, o2 - o1)]
        if mirror_read:
            readings.append((k1 + k2, -(o1 + o2)))
        for dk, num in readings:
            if dk != 0 and num % dk == 0 and num // dk == x:
                return True
        return False

    round_t = 0
    while res.steps < max_steps:
        mags = magsets[round_t % clock]
        round_t += 1
        for j in range(herd_size):
            wk = herd[j]
            e = wk.k * x + wk.o
            f = abs(e)

            if fold:
                # walk from the canonical representative of the orbit
                if not orc.canon_positive(f):
                    if e > 0:
                        wk.k, wk.o, e = -wk.k, -wk.o, -e
                elif e < 0:
                    wk.k, wk.o, e = -wk.k, -wk.o, -e
                i = orc.jump(f)
                m = mags[i]
                if wk.hist and len(wk.hist) >= 2 and f in wk.hist[:-1]:
                    # fruitless cycle: escape from a point that is a function of
                    # the cycle itself, so every walker trapped in the same cycle
                    # escapes to the same place and coalescence survives
                    res.cycles += 1
                    anchor = min(wk.hist + [f])
                    m = mags[(orc.jump(anchor) + 1) & (jump_count - 1)]
                    wk.hist = []
                wk.o += m
                e += m
            else:
                i = orc.jump(f)
                m = mags[i]
                wk.o += m
                e += m

            res.steps += 1
            wk.age += 1
            f = abs(e)
            if fold:
                wk.hist.append(f)
                if len(wk.hist) > hist_len:
                    wk.hist.pop(0)

            if restart and wk.age >= restart:
                herd[j] = seed_walker(wk.k)
                res.setup += 1
                continue

            if not orc.is_dp(f):
                continue
            res.dp += 1

            hit = table.get(f)
            if hit is None:
                table[f] = (wk.k, wk.o)
                continue
            k2, o2 = hit
            if check(wk.k, wk.o, k2, o2):
                res.solved = True
                res.table = len(table)
                res.pair = tuple(sorted((abs(k2), abs(wk.k))))
                return res
            res.merges += 1
            herd[j] = seed_walker(wk.k)
            res.setup += 1

    res.table = len(table)
    return res


def run_arm(arm, bits, trials, seed, max_mult=400.0):
    w = 1 << bits
    root = math.sqrt(w)
    rng = random.Random(seed ^ (bits << 8))
    costs, merges, cycles, tables, fails = [], [], [], [], 0
    for t in range(trials):
        x = rng.randrange(-(w // 2), w // 2)
        r = solve(x, w, arm, seed + 7919 * t, int(max_mult * root))
        if not r.solved:
            fails += 1
            continue
        costs.append(r.steps / root)
        merges.append(r.merges)
        cycles.append(r.cycles)
        tables.append(r.table)
    if not costs:
        return {"arm": arm["name"], "bits": bits, "trials": 0, "timeouts": fails}
    mean = statistics.fmean(costs)
    se = statistics.stdev(costs) / math.sqrt(len(costs)) if len(costs) > 1 else 0.0
    return {
        "arm": arm["name"],
        "bits": bits,
        "trials": len(costs),
        "cost": round(mean, 4),
        "se": round(se, 4),
        "median": round(statistics.median(costs), 4),
        "merges": round(statistics.fmean(merges), 2),
        "cycles": round(statistics.fmean(cycles), 2),
        "table": int(statistics.fmean(tables)),
        "timeouts": fails,
    }


def arm(name, **kw):
    base = dict(name=name, herd=96, jumps=32, fold=False, charges=2)
    base.update(kw)
    return base


ARMS = {
    "classic2": arm("classic2", charges=2),
    "charge3": arm("charge3", charges=3),
    "fold": arm("fold", fold=True, charges=2),
}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--bits", type=int, default=24)
    ap.add_argument("--trials", type=int, default=100)
    ap.add_argument("--seed", type=int, default=20260818)
    ap.add_argument("--arms", default="classic2,charge3,fold")
    ap.add_argument("--stride-div", type=float, default=0.0,
                    help="mean jump = sqrt(w)/div, overrides the arm default")
    ap.add_argument("--herd", type=int, default=0)
    ap.add_argument("--restart", type=int, default=0)
    a = ap.parse_args()
    w = 1 << a.bits
    for name in a.arms.split(","):
        arm_cfg = dict(ARMS[name])
        if a.stride_div:
            arm_cfg["stride"] = max(1, int(math.sqrt(w) / a.stride_div))
        if a.herd:
            arm_cfg["herd"] = a.herd
        if a.restart:
            arm_cfg["restart"] = a.restart
        print(json.dumps(run_arm(arm_cfg, a.bits, a.trials, a.seed)))
        sys.stdout.flush()


if __name__ == "__main__":
    main()
