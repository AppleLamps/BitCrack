#!/usr/bin/env python3
"""Range-position table for every published solved Bitcoin puzzle key.

Puzzle n lives in [2^(n-1), 2^n).  Position inside that interval is

    t = (key - 2^(n-1)) / 2^(n-1)   in [0, 1)

which is the fraction of the way from the start of the range to the end.
100*t is the "percent of the key range" the found key sat at.

Tiny n makes t coarse (puzzle 1 has a 1-key range), so summaries are
reported both on the full set and on n>=20 / n>=32 / consecutive 1-70 /
every-5th (kangaroo) subsets.
"""
from __future__ import annotations

import json
import math
import os
import statistics

HERE = os.path.dirname(os.path.abspath(__file__))


def load_keys():
    with open(os.path.join(HERE, "keys.json")) as f:
        raw = json.load(f)["keys_hex"]
    return {int(n): int(h, 16) for n, h in raw.items()}


def row(n, k):
    start = 1 << (n - 1)
    width = start
    end = (1 << n) - 1
    assert start <= k <= end, (n, hex(k), hex(start), hex(end))
    t = (k - start) / float(width)
    return {
        "n": n,
        "key_hex": format(k, "x"),
        "start_hex": format(start, "x"),
        "end_hex": format(end, "x"),
        "t": t,
        "pct": 100.0 * t,
        "hw": k.bit_count(),
        "hw_expected": 1 + 0.5 * (n - 1),
        "decade": (n - 1) // 10 * 10 + 1,  # 1-10, 11-20, ...
        "half": "upper" if t >= 0.5 else "lower",
        "quartile": min(4, int(t * 4) + 1),
        "every5": n % 5 == 0,
        "consecutive": n <= 70,
    }


def mean(xs):
    return sum(xs) / len(xs) if xs else None


def pstdev(xs):
    return statistics.pstdev(xs) if len(xs) > 1 else 0.0


def median(xs):
    return statistics.median(xs) if xs else None


def quantile(xs, q):
    s = sorted(xs)
    if not s:
        return None
    if len(s) == 1:
        return s[0]
    pos = q * (len(s) - 1)
    lo = int(math.floor(pos))
    hi = int(math.ceil(pos))
    if lo == hi:
        return s[lo]
    return s[lo] * (hi - pos) + s[hi] * (pos - lo)


def summarize(rows, label):
    ts = [r["t"] for r in rows]
    pcts = [r["pct"] for r in rows]
    n = len(rows)
    if n == 0:
        return {"label": label, "n": 0}
    n_upper = sum(1 for r in rows if r["t"] >= 0.5)
    # 10-percentile histogram
    bins = [0] * 10
    for t in ts:
        b = min(9, int(t * 10))
        bins[b] += 1
    qbins = [0] * 4
    for r in rows:
        qbins[r["quartile"] - 1] += 1
    # KS vs Uniform
    s = sorted(ts)
    d = 0.0
    for i, t in enumerate(s, 1):
        d = max(d, abs(i / n - t), abs((i - 1) / n - t))
    # lag-1 among this subset in n-order
    lag = None
    if n >= 3:
        xs = ts[:-1]
        ys = ts[1:]
        mx, my = mean(xs), mean(ys)
        vx = sum((x - mx) ** 2 for x in xs)
        vy = sum((y - my) ** 2 for y in ys)
        if vx > 0 and vy > 0:
            lag = sum((x - mx) * (y - my) for x, y in zip(xs, ys)) / math.sqrt(vx * vy)
    # corr with n
    ns = [r["n"] for r in rows]
    mn, mt = mean(ns), mean(ts)
    vn = sum((x - mn) ** 2 for x in ns)
    vt = sum((x - mt) ** 2 for x in ts)
    corr_n = None
    if vn > 0 and vt > 0:
        corr_n = sum((a - mn) * (b - mt) for a, b in zip(ns, ts)) / math.sqrt(vn * vt)
    return {
        "label": label,
        "n": n,
        "mean_pct": mean(pcts),
        "median_pct": median(pcts),
        "stdev_pct": 100.0 * pstdev(ts),
        "min_pct": min(pcts),
        "max_pct": max(pcts),
        "q1_pct": 100.0 * quantile(ts, 0.25),
        "q3_pct": 100.0 * quantile(ts, 0.75),
        "upper_half": n_upper,
        "upper_half_pct": 100.0 * n_upper / n,
        "decile_counts": bins,
        "quartile_counts": qbins,
        "ks_D": d,
        "ks_crit_5pct": 1.36 / math.sqrt(n),
        "lag1": lag,
        "corr_n": corr_n,
        "uniform_mean": 50.0,
        "uniform_stdev": 100.0 / math.sqrt(12),
    }


def main():
    keys = load_keys()
    rows = [row(n, keys[n]) for n in sorted(keys)]
    groups = {
        "all_solved": rows,
        "consecutive_1_70": [r for r in rows if r["consecutive"]],
        "every_5th": [r for r in rows if r["every5"]],
        "n_ge_20": [r for r in rows if r["n"] >= 20],
        "n_ge_32": [r for r in rows if r["n"] >= 32],
        "n_ge_50": [r for r in rows if r["n"] >= 50],
        "brute_66_69": [r for r in rows if 66 <= r["n"] <= 69],
        "kangaroo_65_plus_5": [r for r in rows if r["n"] >= 65 and r["every5"]],
    }
    # by decade of n
    decades = {}
    for r in rows:
        decades.setdefault(r["decade"], []).append(r)
    decade_summ = [summarize(decades[d], f"n_{d}-{d+9}") for d in sorted(decades)]

    out = {
        "rows": rows,
        "summaries": {k: summarize(v, k) for k, v in groups.items()},
        "decades": decade_summ,
        "note": (
            "t = (key - 2^(n-1)) / 2^(n-1).  Uniform keys have mean 50%, "
            "stdev 28.87%, equal mass in each quartile."
        ),
    }

    json_path = os.path.join(HERE, "positions.json")
    with open(json_path, "w") as f:
        json.dump(out, f, indent=2)

    # human table
    print("n   pct_of_range     key")
    print("--  ------------     ---")
    for r in rows:
        tag = "  5th" if r["every5"] else ""
        print(f"{r['n']:3d}  {r['pct']:8.4f}%     {r['key_hex']}{tag}")

    print("\n=== averages ===")
    for name, s in out["summaries"].items():
        print(
            f"{s['label']:22s}  n={s['n']:3d}  mean={s['mean_pct']:6.2f}%  "
            f"median={s['median_pct']:6.2f}%  stdev={s['stdev_pct']:5.2f}%  "
            f"Q1={s['q1_pct']:5.1f}  Q3={s['q3_pct']:5.1f}  "
            f"upper={s['upper_half']}/{s['n']}  "
            f"KS={s['ks_D']:.3f}/{s['ks_crit_5pct']:.3f}  "
            f"lag1={s['lag1'] if s['lag1'] is None else round(s['lag1'],3)}  "
            f"corr_n={s['corr_n'] if s['corr_n'] is None else round(s['corr_n'],3)}"
        )
    print("\n=== by decade of n ===")
    for s in decade_summ:
        print(
            f"{s['label']:22s}  n={s['n']:3d}  mean={s['mean_pct']:6.2f}%  "
            f"median={s['median_pct']:6.2f}%  deciles={s['decile_counts']}"
        )
    print("\n=== quartile counts (1=0-25%, 4=75-100%) ===")
    for name, s in out["summaries"].items():
        print(f"  {name:22s}  {s['quartile_counts']}")
    print(f"\nwrote {json_path}")
    return out


if __name__ == "__main__":
    main()
