"""Idealised interval-DLP birthday search, in exponent space.

Counts group operations only: one sample = one operation, samples are
i.i.d. uniform on their set, and collisions are detected exactly.  This is
the optimistic model used for the published Gaudry-Schost constants.

The geometry is parameterised rather than hard-coded:

    tame  samples   t  uniform on [-tau*N, tau*N]
    wild  samples   n + u, u uniform on [-omega*N, omega*N]
    wild2 samples   n + u, with an optional offset by shift*N

With classes enabled, a sample is stored under abs(value), modelling the
negation map.  Recovery is checked explicitly against the true n, so a
collision that cannot yield n is not counted as a solve.

Usage:
    python gs_sim.py                 # validate against published constants
    python gs_sim.py sweep           # search (tau, omega) for the class variant
"""

import math
import random
import sys


N_BITS = 32
N = 1 << N_BITS


def run_trial(rng, classes, tau, omega, wild_frac, shift=None, cap_mult=40.0):
    """One instance. Returns group operations used, or None if it gave up."""
    n = rng.randrange(-N // 2, N // 2 + 1)

    tame_half = int(tau * N)
    wild_half = int(omega * N)
    shift_amt = int(shift * N) if shift is not None else None

    tame_seen = {}
    wild_seen = {}

    cap = int(cap_mult * math.sqrt(N))
    ops = 0
    while ops < cap:
        ops += 1
        if rng.random() >= wild_frac:
            t = rng.randrange(-tame_half, tame_half + 1)
            key = abs(t) if classes else t
            if key in wild_seen:
                u = wild_seen[key]
                for cand in (t - u, -t - u):
                    if cand == n:
                        return ops
            tame_seen.setdefault(key, t)
        else:
            u = rng.randrange(-wild_half, wild_half + 1)
            if shift_amt is not None and rng.random() < 0.5:
                u += shift_amt
            v = n + u
            key = abs(v) if classes else v
            if key in tame_seen:
                t = tame_seen[key]
                for cand in (t - u, -t - u):
                    if cand == n:
                        return ops
            if key in wild_seen:
                u2 = wild_seen[key]
                s = -(u + u2)
                if s % 2 == 0 and s // 2 == n:
                    return ops
            wild_seen.setdefault(key, u)
    return None


def measure(trials, classes, tau, omega, wild_frac, shift=None, seed=1):
    rng = random.Random(seed)
    root = math.sqrt(N)
    results = []
    gave_up = 0
    for _ in range(trials):
        ops = run_trial(rng, classes, tau, omega, wild_frac, shift)
        if ops is None:
            gave_up += 1
        else:
            results.append(ops / root)
    if not results:
        return None
    mean = sum(results) / len(results)
    variance = sum((value - mean) ** 2 for value in results) / len(results)
    return mean, math.sqrt(variance / len(results)), gave_up


def line(label, result):
    if result is None:
        print("%-38s  no solves" % label)
        return
    mean, se, gave_up = result
    suffix = "   gave up %d" % gave_up if gave_up else ""
    print("%-38s  %.4f +/- %.4f sqrt(N)%s" % (label, mean, se, suffix))


def validate(trials):
    print("N = 2^%d, %d trials per arm\n" % (N_BITS, trials))
    print("published constants: GS basic 2.08, GS + negation classes 1.36\n")
    line("GS basic tame/wild (no classes)",
         measure(trials, False, 0.5, 0.5, 0.5))
    line("GS + classes, same geometry",
         measure(trials, True, 0.5, 0.5, 0.5))
    line("GS + classes, narrow wild (0.25)",
         measure(trials, True, 0.5, 0.25, 0.5))
    line("GS + classes, narrow wild (0.125)",
         measure(trials, True, 0.5, 0.125, 0.5))
    line("GS + classes, wild-heavy 2:1",
         measure(trials, True, 0.5, 0.5, 0.667))


def sweep(trials):
    print("N = 2^%d, %d trials per point\n" % (N_BITS, trials))
    print("%-8s %-8s %-8s %s" % ("tau", "omega", "wildfrac", "k"))
    best = None
    for tau in (0.25, 0.5, 0.75, 1.0):
        for omega in (0.125, 0.25, 0.375, 0.5, 0.75, 1.0):
            for wild_frac in (0.5, 0.667):
                result = measure(trials, True, tau, omega, wild_frac)
                if result is None:
                    continue
                mean, se, gave_up = result
                print("%-8.4f %-8.4f %-8.3f %.4f +/- %.4f   gave up %d/%d"
                      % (tau, omega, wild_frac, mean, se, gave_up, trials))
                if gave_up == 0 and (best is None or mean < best[0]):
                    best = (mean, tau, omega, wild_frac)
    if best:
        print("\nbest uncensored: k=%.4f at tau=%.4f omega=%.4f wild_frac=%.3f"
              % (best[0], best[1], best[2], best[3]))


if __name__ == "__main__":
    mode = sys.argv[1] if len(sys.argv) > 1 else "validate"
    count = int(sys.argv[2]) if len(sys.argv) > 2 else 3
    if mode == "sweep":
        sweep(count)
    else:
        validate(count)
