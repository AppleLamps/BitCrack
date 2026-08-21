#!/usr/bin/env python3
"""Re-analyse where solved puzzle keys landed in their intervals.

Position is the *top* bits of the mantissa, not the wallet LSBs:

    t = (key - 2^(n-1)) / 2^(n-1)   in [0, 1)

Small n quantises t, so the high-resolution set is n >= 32.
"""
from __future__ import annotations

import json
import math
import os
import random
import statistics

HERE = os.path.dirname(os.path.abspath(__file__))
W71 = 1 << 70
S71 = 1 << 70


def load_keys():
    with open(os.path.join(HERE, "keys.json")) as f:
        raw = json.load(f)["keys_hex"]
    return {int(n): int(h, 16) for n, h in raw.items()}


def t_of(n, k):
    return (k - (1 << (n - 1))) / float(1 << (n - 1))


def top_bits(t, bits):
    x = int(t * (1 << bits))
    if x == (1 << bits):
        x -= 1
    return x


def chi2(counts, expected):
    return sum((c - expected) ** 2 / expected for c in counts)


def chi2_sf(x, df):
    # survival function via regularised gamma Q(df/2, x/2); simple series for modest df
    # use Wilson-Hilferty approximation for p
    if df <= 0:
        return 1.0
    z = ((x / df) ** (1.0 / 3.0) - (1.0 - 2.0 / (9.0 * df))) / math.sqrt(2.0 / (9.0 * df))
    return 0.5 * math.erfc(z / math.sqrt(2.0))


def kde(ts, x, bw):
    n = len(ts)
    s = 0.0
    inv = 1.0 / (bw * math.sqrt(2 * math.pi) * n)
    for t in ts:
        s += math.exp(-0.5 * ((x - t) / bw) ** 2)
    return s * inv


def kde_mode(ts, bw, grid=2000):
    best_x, best_y = 0.5, -1.0
    for i in range(grid):
        x = (i + 0.5) / grid
        y = kde(ts, x, bw)
        if y > best_y:
            best_y, best_x = y, x
    return best_x, best_y


def densest_window(ts, width, step=0.001):
    best = (0, 0.0, width)
    x = 0.0
    while x + width <= 1.0 + 1e-15:
        c = sum(1 for t in ts if x <= t < x + width)
        if c > best[0] or (c == best[0] and abs(x + width / 2 - 0.5) < abs(best[1] + width / 2 - 0.5)):
            best = (c, x, x + width)
        x += step
    return best


def silverman_bw(ts):
    n = len(ts)
    sd = statistics.pstdev(ts)
    iqr = statistics.quantiles(ts, n=4, method="inclusive")
    # fallback if quantiles odd
    s = sorted(ts)
    q1 = s[max(0, (n - 1) // 4)]
    q3 = s[min(n - 1, (3 * (n - 1)) // 4)]
    iqr = q3 - q1
    scale = min(sd, iqr / 1.34) if iqr > 0 else sd
    if scale <= 0:
        scale = 0.2
    return 0.9 * scale * n ** (-0.2)


def beta_mom(ts):
    mu = sum(ts) / len(ts)
    v = statistics.pvariance(ts)
    if v <= 0 or mu <= 0 or mu >= 1:
        return None
    tmp = mu * (1 - mu) / v - 1
    if tmp <= 0:
        return {"alpha": None, "beta": None, "note": "over-dispersed vs Bernoulli bound"}
    return {"alpha": mu * tmp, "beta": (1 - mu) * tmp, "mean": mu, "var": v}


def nn_count(ts, t0, rad):
    return sum(1 for t in ts if abs(t - t0) <= rad)


def main():
    keys = load_keys()
    rows = []
    for n in sorted(keys):
        k = keys[n]
        t = t_of(n, k)
        rows.append({"n": n, "k": k, "t": t, "pct": 100 * t, "every5": n % 5 == 0})

    sets = {
        "all": rows,
        "n1_70": [r for r in rows if r["n"] <= 70],
        "n_ge_20": [r for r in rows if r["n"] >= 20],
        "n_ge_32": [r for r in rows if r["n"] >= 32],
        "n_ge_40": [r for r in rows if r["n"] >= 40],
        "n_ge_50": [r for r in rows if r["n"] >= 50],
        "every5": [r for r in rows if r["every5"]],
        "every5_ge_40": [r for r in rows if r["every5"] and r["n"] >= 40],
        "near_71": [r for r in rows if r["n"] in range(60, 71) or (r["every5"] and r["n"] >= 65)],
        "addr_only_66_69": [r for r in rows if 66 <= r["n"] <= 69],
        "kanga_65_135": [r for r in rows if r["every5"] and r["n"] >= 65],
    }

    report = {"n_keys": len(rows)}
    summaries = {}
    for name, rs in sets.items():
        ts = [r["t"] for r in rs]
        if len(ts) < 3:
            summaries[name] = {"n": len(ts), "pcts": [r["pct"] for r in rs]}
            continue
        dec = [0] * 10
        for t in ts:
            dec[min(9, int(t * 10))] += 1
        quin = [0] * 20
        for t in ts:
            quin[min(19, int(t * 20))] += 1
        half = sum(1 for t in ts if t >= 0.5)
        q = [0] * 4
        for t in ts:
            q[min(3, int(t * 4))] += 1
        bw = silverman_bw(ts)
        mode, dens = kde_mode(ts, bw)
        d1 = densest_window(ts, 0.01, 0.0005)
        d5 = densest_window(ts, 0.05, 0.001)
        d10 = densest_window(ts, 0.10, 0.001)
        summaries[name] = {
            "n": len(ts),
            "mean_pct": 100 * sum(ts) / len(ts),
            "median_pct": 100 * statistics.median(ts),
            "stdev_pct": 100 * statistics.pstdev(ts),
            "min_pct": 100 * min(ts),
            "max_pct": 100 * max(ts),
            "upper_half": half,
            "quartiles": q,
            "deciles": dec,
            "twentieths": quin,
            "chi2_decile": chi2(dec, len(ts) / 10.0),
            "p_decile": chi2_sf(chi2(dec, len(ts) / 10.0), 9),
            "chi2_quartile": chi2(q, len(ts) / 4.0),
            "p_quartile": chi2_sf(chi2(q, len(ts) / 4.0), 3),
            "beta": beta_mom(ts),
            "silverman_bw": bw,
            "kde_mode_pct": 100 * mode,
            "kde_density": dens,
            "densest_1pct": {"count": d1[0], "lo": d1[1], "hi": d1[2], "expected": len(ts) * 0.01},
            "densest_5pct": {"count": d5[0], "lo": d5[1], "hi": d5[2], "expected": len(ts) * 0.05},
            "densest_10pct": {"count": d10[0], "lo": d10[1], "hi": d10[2], "expected": len(ts) * 0.10},
            "mean_abs_dev_from_half": 100 * sum(abs(t - 0.5) for t in ts) / len(ts),
        }
    report["summaries"] = summaries

    # Position MSBs for n>=32: top 3 and top 4 bits of t
    hi = [r for r in rows if r["n"] >= 32]
    for bits in (3, 4, 5):
        bins = [0] * (1 << bits)
        for r in hi:
            bins[top_bits(r["t"], bits)] += 1
        exp = len(hi) / float(1 << bits)
        report[f"top{bits}_bits_n_ge_32"] = {
            "counts": bins,
            "expected": exp,
            "chi2": chi2(bins, exp),
            "p": chi2_sf(chi2(bins, exp), (1 << bits) - 1),
        }

    # Neighbourhood of each large-n landing: how many other large-n keys within ±5%
    nb = []
    ts32 = [r["t"] for r in hi]
    for r in hi:
        nb.append(
            {
                "n": r["n"],
                "pct": r["pct"],
                "within_5pct": nn_count(ts32, r["t"], 0.05) - 1,
                "within_10pct": nn_count(ts32, r["t"], 0.10) - 1,
            }
        )
    nb.sort(key=lambda d: (-d["within_5pct"], d["n"]))
    report["busiest_landings_n_ge_32"] = nb[:12]

    # Bootstrap KDE mode on n>=32
    rng = random.Random(71)
    ts = [r["t"] for r in hi]
    bw = silverman_bw(ts)
    modes = []
    for _ in range(400):
        sample = [ts[rng.randrange(len(ts))] for _ in range(len(ts))]
        m, _ = kde_mode(sample, bw, grid=400)
        modes.append(m)
    modes.sort()
    report["bootstrap_kde_mode_n_ge_32"] = {
        "bw": bw,
        "point": summaries["n_ge_32"]["kde_mode_pct"],
        "boot_mean_pct": 100 * sum(modes) / len(modes),
        "boot_p10_pct": 100 * modes[int(0.10 * len(modes))],
        "boot_p50_pct": 100 * modes[int(0.50 * len(modes))],
        "boot_p90_pct": 100 * modes[int(0.90 * len(modes))],
        "frac_in_45_55": sum(1 for m in modes if 0.45 <= m < 0.55) / len(modes),
        "frac_in_40_60": sum(1 for m in modes if 0.40 <= m < 0.60) / len(modes),
    }

    # Implied puzzle-71 keys from each historical t (n>=32)
    implied = []
    for r in rows:
        if r["n"] < 32:
            continue
        k71 = S71 + int(r["t"] * W71)
        implied.append(
            {
                "from_n": r["n"],
                "t_pct": r["pct"],
                "implied_key_hex": format(k71, "x"),
            }
        )
    report["implied_p71_from_n_ge_32"] = implied

    # Vote: 5% bins of implied t, n>=32
    votes = [0] * 20
    for r in hi:
        votes[min(19, int(r["t"] * 20))] += 1
    report["vote_5pct_n_ge_32"] = votes

    # Closest puzzles explicit
    report["closest"] = [
        {"n": r["n"], "pct": r["pct"], "key": format(r["k"], "x")}
        for r in rows
        if r["n"] >= 60
    ]

    # ASCII histogram helper data: 50 bins on n>=32
    hist50 = [0] * 50
    for r in hi:
        hist50[min(49, int(r["t"] * 50))] += 1
    report["hist_2pct_n_ge_32"] = hist50

    # Sorted landings n>=32
    report["sorted_n_ge_32"] = sorted(
        [{"n": r["n"], "pct": r["pct"]} for r in hi], key=lambda d: d["pct"]
    )

    out = os.path.join(HERE, "reanalysis.json")
    with open(out, "w") as f:
        json.dump(report, f, indent=2)

    def show_summ(name):
        s = summaries[name]
        if "mean_pct" not in s:
            print(name, s)
            return
        print(
            f"{name:18s} n={s['n']:3d}  mean={s['mean_pct']:6.2f}  med={s['median_pct']:6.2f}  "
            f"sd={s['stdev_pct']:5.2f}  |t-50|={s['mean_abs_dev_from_half']:5.2f}  "
            f"Q={s['quartiles']}  KDE={s['kde_mode_pct']:.1f}%  "
            f"dens1%={s['densest_1pct']['count']}@{100*s['densest_1pct']['lo']:.1f}-{100*s['densest_1pct']['hi']:.1f}  "
            f"dens5%={s['densest_5pct']['count']}@{100*s['densest_5pct']['lo']:.1f}-{100*s['densest_5pct']['hi']:.1f}  "
            f"pQ={s['p_quartile']:.2f} pD={s['p_decile']:.2f}"
        )
        b = s["beta"]
        if b and b.get("alpha"):
            print(f"{'':18s} Beta(α={b['alpha']:.3f}, β={b['beta']:.3f})  dec={s['deciles']}")

    print("=== subset summaries ===")
    for name in sets:
        show_summ(name)

    print("\n=== 5% votes n>=32 (expected {:.2f}) ===".format(len(hi) / 20))
    for i, c in enumerate(votes):
        lo, hi_p = i * 5, (i + 1) * 5
        bar = "#" * c
        print(f"  {lo:3d}-{hi_p:3d}%  {c:2d} {bar}")

    print("\n=== 2% hist n>=32 ===")
    m = max(hist50) or 1
    for i, c in enumerate(hist50):
        print(f"  {i*2:3d}-{i*2+2:3d}%  {c:2d} {'#' * c}")

    print("\n=== top bits n>=32 ===")
    for bits in (3, 4, 5):
        d = report[f"top{bits}_bits_n_ge_32"]
        print(f"  {bits} bits  chi2={d['chi2']:.2f} p={d['p']:.3f}  counts={d['counts']}")

    print("\n=== bootstrap KDE mode n>=32 ===")
    print(report["bootstrap_kde_mode_n_ge_32"])

    print("\n=== busiest landings (other n>=32 within ±5%) ===")
    for d in nb[:10]:
        print(f"  n={d['n']:3d}  {d['pct']:7.3f}%   neighbors5={d['within_5pct']}  neighbors10={d['within_10pct']}")

    print("\n=== closest puzzles ===")
    for d in report["closest"]:
        print(f"  {d['n']:3d}  {d['pct']:8.4f}%  {d['key']}")

    print(f"\nwrote {out}")


if __name__ == "__main__":
    main()
