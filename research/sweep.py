"""Parameter sweeps over sim_interval_dlp arms, CSV on stdout.

Comparisons are memory matched: the reported `table` column is the number of
stored distinguished points at the moment of the solve, so arms are only
comparable at similar table sizes.  Sweep dp_bits to move along that axis.
"""

import argparse
import csv
import itertools
import math
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import sim_interval_dlp as sim

BASE = dict(herd=96, jumps=32, fold=False, charges=2, mirror_read=True, clock=1)


def build(name, **kw):
    a = dict(BASE)
    a.update(kw)
    a["name"] = name
    return a


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--bits", type=int, default=24)
    ap.add_argument("--trials", type=int, default=60)
    ap.add_argument("--seed", type=int, default=20260818)
    ap.add_argument("--dp", default="3,5,7")
    ap.add_argument("--stride-div", default="0.5,1,2,4,8")
    ap.add_argument("--herd", default="96")
    ap.add_argument("--restart", default="0")
    ap.add_argument("--arms", default="classic2,charge3,fold")
    ap.add_argument("--jumps", default="32")
    ap.add_argument("--hist", default="6")
    a = ap.parse_args()

    w = 1 << a.bits
    root = math.sqrt(w)
    out = csv.writer(sys.stdout)
    out.writerow(["arm", "bits", "dp_bits", "stride_div", "herd", "restart",
                  "jumps", "hist", "trials", "cost", "se", "median", "merges",
                  "cycles", "table", "timeouts"])

    variants = {
        "classic2": dict(charges=2),
        "classic2_nomirror": dict(charges=2, mirror_read=False),
        "charge3": dict(charges=3),
        "fold": dict(fold=True, charges=2),
        "fold_nocycle": dict(fold=True, charges=2, hist=0),
        "fold_clock2": dict(fold=True, charges=2, clock=2),
        "fold_clock4": dict(fold=True, charges=2, clock=4),
        "fold3": dict(fold=True, charges=3),
    }

    grid = itertools.product(
        a.arms.split(","),
        [int(v) for v in a.dp.split(",")],
        [float(v) for v in a.stride_div.split(",")],
        [int(v) for v in a.herd.split(",")],
        [int(v) for v in a.restart.split(",")],
        [int(v) for v in a.jumps.split(",")],
        [int(v) for v in a.hist.split(",")],
    )
    for name, dp, sdiv, herd, restart, jumps, hist in grid:
        kw = dict(dp_bits=dp, stride=max(1, int(root / sdiv)), herd=herd,
                  restart=restart, jumps=jumps, hist=hist)
        kw.update(variants[name])          # arm definition wins over the grid
        cfg = build(name, **kw)
        r = sim.run_arm(cfg, a.bits, a.trials, a.seed)
        out.writerow([name, a.bits, dp, sdiv, herd, restart, jumps, hist,
                      r.get("trials", 0),
                      r.get("cost", ""), r.get("se", ""), r.get("median", ""),
                      r.get("merges", ""), r.get("cycles", ""), r.get("table", ""),
                      r.get("timeouts", "")])
        sys.stdout.flush()


if __name__ == "__main__":
    main()
