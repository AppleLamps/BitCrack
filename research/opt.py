"""Two phase mechanism/parameter search over sim_interval_dlp.

Phase 1 compares mechanisms on a common grid (herd, dp bits, mean jump), so
every arm is measured at its own optimum instead of at the incumbent's.
Phase 2 takes the winning mechanism and optimises the seed densities, which is
the one degree of freedom the folded coordinate changes: the tame density is flat
on the folded interval while the wild density spills past its edge by |x|.

CSV on stdout, one row per configuration.
"""

import argparse
import csv
import itertools
import math
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import sim_interval_dlp as sim

MECHS = {
    "classic2": dict(fold=False, charges=2),
    "charge3": dict(fold=False, charges=3),
    "fold": dict(fold=True, charges=2),
}

FIELDS = ["arm", "spectrum", "bits", "herd", "dp_bits", "stride_div", "jumps",
          "draws", "scale_t", "scale_w", "trials", "cost", "se", "median",
          "merges", "cycles", "table", "timeouts"]


def measure(name, bits, trials, seed, herd, dp, sdiv, jumps, draws, sct, scw,
            spectrum):
    root = math.sqrt(1 << bits)
    cfg = dict(name=name, herd=herd, jumps=jumps, dp_bits=dp,
               stride=max(1, int(root / sdiv)), mirror_read=True, clock=1,
               spectrum=spectrum,
               spread=(draws, draws, draws), scale=(sct, scw, scw))
    cfg.update(MECHS[name])
    r = sim.run_arm(cfg, bits, trials, seed)
    return [name, spectrum, bits, herd, dp, sdiv, jumps, draws, sct, scw,
            r.get("trials", 0), r.get("cost", ""), r.get("se", ""),
            r.get("median", ""), r.get("merges", ""), r.get("cycles", ""),
            r.get("table", ""), r.get("timeouts", "")]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--bits", type=int, default=22)
    ap.add_argument("--trials", type=int, default=120)
    ap.add_argument("--seed", type=int, default=20260818)
    ap.add_argument("--phase", type=int, default=1)
    ap.add_argument("--arms", default="classic2,charge3,fold")
    ap.add_argument("--herd", default="8,32,96")
    ap.add_argument("--dp", default="3,5")
    ap.add_argument("--stride-div", default="0.5,1,2")
    ap.add_argument("--jumps", default="512")
    ap.add_argument("--draws", default="2")
    ap.add_argument("--scale-t", default="1.0")
    ap.add_argument("--scale-w", default="1.0")
    ap.add_argument("--spectrum", default="uniform")
    a = ap.parse_args()

    out = csv.writer(sys.stdout)
    out.writerow(FIELDS)
    grid = itertools.product(
        a.arms.split(","),
        [int(v) for v in a.herd.split(",")],
        [int(v) for v in a.dp.split(",")],
        [float(v) for v in a.stride_div.split(",")],
        [int(v) for v in a.jumps.split(",")],
        [int(v) for v in a.draws.split(",")],
        [float(v) for v in a.scale_t.split(",")],
        [float(v) for v in a.scale_w.split(",")],
        a.spectrum.split(","),
    )
    for name, herd, dp, sdiv, jumps, draws, sct, scw, spec in grid:
        out.writerow(measure(name, a.bits, a.trials, a.seed, herd, dp, sdiv,
                             jumps, draws, sct, scw, spec))
        sys.stdout.flush()


if __name__ == "__main__":
    main()
