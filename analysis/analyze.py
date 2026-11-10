#!/usr/bin/env python3
"""Turn the ESP32's 1-second CSV logs into the results for experiments E1-E6.

Usage
    python analysis/analyze.py data/raw            # your real logs
    python analysis/analyze.py --synthetic         # pipeline test on generated data

Input: every *.csv / *.txt / *.log file in the folder, captured from the Serial
Monitor (or with analysis/capture.py). Lines starting with '#' are ignored, and
each line carries its experiment label (set with the `label` command), e.g.
E2_light_s1. See experiments/PROTOCOL.md for the labels and marks.

Output (in --out, default analysis/output):
    summary.md            every result, with figure captions
    tables/*.csv, *.tex   one table per experiment
    figures/*.png, *.pdf  five figures in one consistent style
"""
from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

import numpy as np
import pandas as pd
import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt  # noqa: E402

# ---------------------------------------------------------------------------
# Settings
# ---------------------------------------------------------------------------
SETTLE_S = 30            # drop this long after every label change or mark
THRESHOLD = 0.80         # soiling threshold used by both rules
DUST_ORDER = ["clean", "light", "medium", "heavy"]
E4_BEFORE = (-40, -5)    # seconds relative to wiper start
E4_AFTER = (30, 60)      # seconds relative to wiper end (panel settles first)
E5_TIMEOUT_S = 120       # a fault not raised within this is "not detected"
RISE_GAP_S = 2           # a rule must be off this long before a new trigger counts

# Which fault code(s) each injected fault should raise (mark: inject_<key>).
E5_EXPECTED = {
    "ldr":    (["LDR_MISMATCH"], "One LDR pair covered"),
    "panel":  (["PANEL_DISCONNECTED", "NO_CURRENT"], "Panel wire disconnected"),
    "load":   (["NO_CURRENT"], "Test load disconnected"),
    "servo":  (["TILT_NO_RESPONSE"], "Tilt servo unplugged"),
    "temp":   (["PANEL_HOT_VS_AIR", "PANEL_OVERHEAT"], "Panel probe warmed"),
    "ina":    (["INA219_MISSING", "CURRENT_SENSOR_MISMATCH"], "INA219 SDA unplugged"),
    "acs":    (["ACS712_FAULT", "CURRENT_SENSOR_MISMATCH"], "ACS712 output disconnected"),
    "ds18b20": (["TEMP_SENSOR"], "DS18B20 unplugged"),
    "dht":    (["DHT_SENSOR"], "DHT11 unplugged"),
}

COL = {"a": "#c0504d", "b": "#1f6fb2", "before": "#9a9a9a", "after": "#2c8553",
       "track": "#d98a10", "fixed": "#5a6a75", "light": "#b8860b", "current": "#1f6fb2"}
LABEL_RE = re.compile(r"^(E\d)_([A-Za-z0-9.\-]+)_([A-Za-z0-9.\-]+)$")


# ---------------------------------------------------------------------------
# Loading
# ---------------------------------------------------------------------------
def load_folder(folder: Path) -> pd.DataFrame:
    files = sorted(p for p in folder.iterdir() if p.suffix.lower() in {".csv", ".txt", ".log"})
    if not files:
        sys.exit(f"No .csv/.txt/.log files in {folder}")
    frames = [f for f in (load_file(p) for p in files) if f is not None]
    if not frames:
        sys.exit("No CSV rows found. Did the capture include the header line (starts with 'ms,label')?")
    df = pd.concat(frames, ignore_index=True)
    parts = df["label"].str.extract(LABEL_RE)
    df["exp"], df["cond"], df["session"] = parts[0], parts[1], parts[2]
    return df


def load_file(path: Path) -> pd.DataFrame | None:
    header, rows = None, []
    for line in path.read_text(errors="replace").splitlines():
        line = line.strip()
        if not line or line.startswith("#"):
            continue
        if line.startswith("ms,label"):
            header = line.split(",")
            continue
        if header is None:
            continue
        fields = line.split(",")
        if len(fields) == len(header):
            rows.append(fields)
    if not rows:
        print(f"  skipped {path.name}: no CSV rows")
        return None
    df = pd.DataFrame(rows, columns=header)
    for c in df.columns:
        if c not in {"label", "mark", "mode", "trusted", "faults"}:
            df[c] = pd.to_numeric(df[c], errors="coerce")
    for c in ("label", "mark", "faults"):
        df[c] = df[c].fillna("").astype(str)
    # Time in seconds; millis() restarts at 0 after a reboot, so stitch the pieces.
    ms = df["ms"].to_numpy(dtype=float)
    offset, t = 0.0, np.empty_like(ms)
    for i in range(len(ms)):
        if i and ms[i] < ms[i - 1]:
            offset += ms[i - 1] + 1000
        t[i] = (ms[i] + offset) / 1000.0
    df["t"] = t
    df["file"] = path.name
    print(f"  {path.name}: {len(df)} rows")
    return df


def add_settle(df: pd.DataFrame) -> pd.DataFrame:
    """Seconds since the last label change or mark, per file."""
    df = df.copy()
    since = np.full(len(df), np.inf)
    for _, g in df.groupby("file", sort=False):
        idx = g.index.to_numpy()
        last_event = -np.inf
        prev_label = None
        for i, t, lab, mark in zip(idx, g["t"], g["label"], g["mark"]):
            if lab != prev_label or mark:
                last_event = t
            prev_label = lab
            since[i] = t - last_event
    df["since_event"] = since
    return df


def clean(df: pd.DataFrame) -> pd.DataFrame:
    """The junk filter from the plan: -127 temperatures, sensor errors,
    the first 30 s after a change, wiper running, self-test, panels open."""
    bad_sensor = df["faults"].str.contains("INA219_MISSING|ACS712_FAULT", regex=True)
    keep = (
        (df["since_event"] >= SETTLE_S)
        & (df["wiper"] == 0)
        & (df["selftest"] == 0)
        & (df["panel_connected"] == 1)
        & df["current_a"].notna()
        & ~bad_sensor
        & ~(df["panel_c"] <= -100)
    )
    return df[keep]


# ---------------------------------------------------------------------------
# Figure style
# ---------------------------------------------------------------------------
SYNTHETIC = False


def style():
    plt.rcParams.update({
        "font.family": "DejaVu Sans", "font.size": 8, "axes.titlesize": 8,
        "axes.labelsize": 8, "legend.fontsize": 7, "xtick.labelsize": 7, "ytick.labelsize": 7,
        "axes.spines.top": False, "axes.spines.right": False, "axes.grid": True,
        "grid.color": "#dddddd", "grid.linewidth": 0.5, "lines.linewidth": 1.1,
        "savefig.dpi": 300, "savefig.bbox": "tight",
    })


def save(fig, out: Path, name: str):
    if SYNTHETIC:
        fig.text(0.5, 0.5, "SYNTHETIC TEST DATA\nNOT EXPERIMENTAL RESULTS", ha="center", va="center",
                 fontsize=14, color="red", alpha=0.25, rotation=20, weight="bold",
                 transform=fig.transFigure, zorder=100)
    for ext in ("png", "pdf"):
        fig.savefig(out / "figures" / f"{name}.{ext}")
    plt.close(fig)


def write_table(df: pd.DataFrame, out: Path, name: str, caption: str) -> str:
    df.to_csv(out / "tables" / f"{name}.csv", index=False)
    cols = "l" + "r" * (len(df.columns) - 1)
    tex = ["\\begin{table}[t]", "\\centering", f"\\caption{{{caption}}}", f"\\label{{tab:{name}}}",
           f"\\begin{{tabular}}{{{cols}}}", "\\hline",
           " & ".join(str(c).replace("%", "\\%").replace("_", "\\_") for c in df.columns) + " \\\\", "\\hline"]
    for _, r in df.iterrows():
        tex.append(" & ".join(str(v).replace("%", "\\%").replace("_", "\\_") for v in r.values) + " \\\\")
    tex += ["\\hline", "\\end{tabular}", "\\end{table}"]
    (out / "tables" / f"{name}.tex").write_text("\n".join(tex) + "\n")
    md = "| " + " | ".join(map(str, df.columns)) + " |\n|" + "---|" * len(df.columns) + "\n"
    md += "".join("| " + " | ".join(map(str, r.values)) + " |\n" for _, r in df.iterrows())
    return md


def fmt(x, d=3):
    return "-" if x is None or (isinstance(x, float) and np.isnan(x)) else f"{x:.{d}f}"


# ---------------------------------------------------------------------------
# Experiments
# ---------------------------------------------------------------------------
def e1(df, out):
    d = df[(df["exp"] == "E1") & (df["soil_valid"] == 1)]
    if d.empty:
        return "## E1 Baseline noise\n\nNo E1 data.\n"
    rows = []
    for col, name in (("soil_light", "Rule B (light-referenced)"), ("soil_naive", "Rule A (current only)")):
        v = d[col].dropna()
        rows.append({"Ratio": name, "Mean": fmt(v.mean()), "SD": fmt(v.std()),
                     "CV %": fmt(100 * v.std() / v.mean(), 2), "2 SD noise band": f"±{fmt(2 * v.std())}",
                     "Samples": len(v), "Sessions": d["session"].nunique()})
    t = pd.DataFrame(rows)
    md = write_table(t, out, "e1_baseline", "Soiling-ratio noise on the clean panel (E1).")
    return f"## E1 Baseline noise\n\n{md}\nThe 2 SD band is the noise floor: dust must lower the ratio by more than this to be detected.\n"


def e2(df, out):
    d = df[(df["exp"] == "E2") & (df["soil_valid"] == 1)]
    if d.empty:
        return "## E2 Dust levels\n\nNo E2 data.\n"
    levels = [l for l in DUST_ORDER if l in set(d["cond"])] + sorted(set(d["cond"]) - set(DUST_ORDER))
    rows, data = [], []
    for lev in levels:
        v = d.loc[d["cond"] == lev, "soil_light"].dropna()
        vn = d.loc[d["cond"] == lev, "soil_naive"].dropna()
        data.append(v.to_numpy())
        rows.append({"Dust level": lev, "Rule B mean": fmt(v.mean()), "Rule B SD": fmt(v.std()),
                     "Rule A mean": fmt(vn.mean()), "Below 0.80 %": fmt(100 * (v < THRESHOLD).mean(), 1),
                     "Samples": len(v), "Sessions": d.loc[d["cond"] == lev, "session"].nunique()})
    md = write_table(pd.DataFrame(rows), out, "e2_dust_levels", "Light-referenced soiling ratio per dust level (E2).")
    fig, ax = plt.subplots(figsize=(3.5, 2.4))
    ax.boxplot(data, tick_labels=levels, showfliers=False, widths=0.55,
               medianprops={"color": COL["b"]}, boxprops={"color": "#333"})
    ax.axhline(THRESHOLD, color=COL["a"], ls="--", lw=0.9, label="Threshold 0.80")
    ax.set_xlabel("Dust level")
    ax.set_ylabel("Soiling ratio  I / (k·L)")
    ax.legend(loc="lower left", frameon=False)
    save(fig, out, "fig1_soiling_vs_dust")
    return (f"## E2 Dust levels\n\n{md}\n![Fig. 1](figures/fig1_soiling_vs_dust.png)\n\n"
            "*Fig. 1. Light-referenced soiling ratio for each dust level. Boxes: quartiles; "
            "whiskers: 1.5 IQR; dashed line: 0.80 alert threshold.*\n")


def rising_edges(t: np.ndarray, flag: np.ndarray) -> np.ndarray:
    """Times where a rule turns on after being off for at least RISE_GAP_S."""
    edges, last_on = [], -np.inf
    for ti, f in zip(t, flag):
        if f:
            if ti - last_on > RISE_GAP_S:
                edges.append(ti)
            last_on = ti
    return np.array(edges)


def e3(df, out):
    d = df[(df["exp"] == "E3")]
    if d.empty:
        return "## E3 Cloud test\n\nNo E3 data.\n"
    rows = []
    first = None
    for (f, ses), g in d.groupby(["file", "session"], sort=False):
        g = g[(g["since_event"] >= 0) & (g["panel_connected"] == 1) & (g["wiper"] == 0)]
        t = g["t"].to_numpy()
        a = rising_edges(t, g["rule_a"].to_numpy() == 1)
        b = rising_edges(t, g["rule_b"].to_numpy() == 1)
        passes = int(g["mark"].str.startswith("cloud").sum())
        rows.append({"Session": ses, "Shade passes": passes or "-", "Rule A triggers": len(a),
                     "Rule B triggers": len(b), "Duration s": int(t[-1] - t[0]) if len(t) else 0})
        if first is None:
            first = (g, a, b)
    t3 = pd.DataFrame(rows)
    tot = {"Session": "Total", "Shade passes": sum(r["Shade passes"] for r in rows if r["Shade passes"] != "-") or "-",
           "Rule A triggers": int(t3["Rule A triggers"].sum()), "Rule B triggers": int(t3["Rule B triggers"].sum()),
           "Duration s": int(t3["Duration s"].sum())}
    t3 = pd.concat([t3, pd.DataFrame([tot])], ignore_index=True)
    md = write_table(t3, out, "e3_cloud_false_triggers",
                     "False soiling triggers during shade passes: Rule A (current only) vs Rule B (light-referenced) (E3).")

    g, a, b = first
    t0 = g["t"].iloc[0]
    x = g["t"] - t0
    fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(7.16, 3.6), sharex=True, height_ratios=[1, 1])
    ax1.plot(x, g["current_a"] * 1000, color=COL["current"], label="Panel current")
    ax1.set_ylabel("Current (mA)")
    ax1b = ax1.twinx()
    ax1b.plot(x, g["photo_mv"], color=COL["light"], lw=0.9, label="Light reference")
    ax1b.set_ylabel("Light (mV)")
    ax1b.grid(False)
    ax1b.spines["right"].set_visible(True)
    h1, l1 = ax1.get_legend_handles_labels()
    h2, l2 = ax1b.get_legend_handles_labels()
    ax1.legend(h1 + h2, l1 + l2, loc="lower center", bbox_to_anchor=(0.5, 1.0), frameon=False, ncol=2)
    ax2.plot(x, g["soil_naive"].clip(upper=1.6), color=COL["a"], label="Rule A: I / I₀")
    ax2.plot(x, g["soil_light"].clip(upper=1.6), color=COL["b"], label="Rule B: I / (k·L)")
    ax2.axhline(THRESHOLD, color="#333", ls="--", lw=0.8, label="Threshold 0.80")
    ax2.plot(a - t0, np.full(len(a), 0.10), "v", color=COL["a"], ms=4, label=f"Rule A trigger ({len(a)})")
    ax2.plot(b - t0, np.full(len(b), 0.03), "v", color=COL["b"], ms=4, label=f"Rule B trigger ({len(b)})")
    ax2.set_ylim(0, 1.65)
    ax2.set_ylabel("Soiling ratio (clipped at 1.6)")
    ax2.set_xlabel("Time (s)")
    ax2.legend(loc="upper center", bbox_to_anchor=(0.5, -0.32), frameon=False, ncol=5)
    save(fig, out, "fig2_cloud_test")
    return (f"## E3 Cloud test\n\n{md}\n![Fig. 2](figures/fig2_cloud_test.png)\n\n"
            "*Fig. 2. Shade passes over the panels and the light reference together. Top: panel current and light. "
            "Bottom: Rule A (current only) and Rule B (light-referenced) ratios; triangles mark each rule's soiling triggers. "
            "Every trigger here is false, because the panel was clean.*\n")


def e4(df, out):
    d = df[df["exp"] == "E4"]
    if d.empty:
        return "## E4 Cleaning\n\nNo E4 data.\n"
    runs = []
    for (f, lab), g in d.groupby(["file", "label"], sort=False):
        w = g["wiper"].to_numpy()
        t = g["t"].to_numpy()
        starts = t[1:][(w[1:] == 1) & (w[:-1] == 0)]
        ends = t[1:][(w[1:] == 0) & (w[:-1] == 1)]
        valid = g[(g["soil_valid"] == 1)]
        for s in starts:
            e = ends[ends > s]
            if not len(e):
                continue
            e = e[0]
            bef = valid[(valid["t"] >= s + E4_BEFORE[0]) & (valid["t"] <= s + E4_BEFORE[1])]["soil_light"]
            aft = valid[(valid["t"] >= e + E4_AFTER[0]) & (valid["t"] <= e + E4_AFTER[1])]["soil_light"]
            if len(bef) < 5 or len(aft) < 5:
                continue
            runs.append({"level": g["cond"].iloc[0], "session": g["session"].iloc[0],
                         "before": bef.mean(), "after": aft.mean()})
    if not runs:
        return "## E4 Cleaning\n\nNo complete wiper runs with 40 s before and 60 s after.\n"
    r = pd.DataFrame(runs)
    r["recovery"] = r["after"] - r["before"]
    levels = [l for l in DUST_ORDER if l in set(r["level"])] + sorted(set(r["level"]) - set(DUST_ORDER))
    rows = []
    for lev in levels:
        g = r[r["level"] == lev]
        rows.append({"Dust level": lev, "Runs": len(g), "Before": f"{fmt(g['before'].mean())} ± {fmt(g['before'].std())}",
                     "After": f"{fmt(g['after'].mean())} ± {fmt(g['after'].std())}",
                     "Recovery": f"{fmt(g['recovery'].mean())} ± {fmt(g['recovery'].std())}",
                     "Back above 0.80": f"{(g['after'] >= THRESHOLD).sum()}/{len(g)}"})
    md = write_table(pd.DataFrame(rows), out, "e4_cleaning", "Soiling ratio before and after each wiper cycle (mean ± SD) (E4).")
    r.round(4).to_csv(out / "tables" / "e4_cleaning_runs.csv", index=False)
    fig, ax = plt.subplots(figsize=(3.5, 2.4))
    xs = np.arange(len(levels))
    for k, (col, off) in enumerate((("before", -0.18), ("after", 0.18))):
        m = [r.loc[r["level"] == l, col].mean() for l in levels]
        s = [r.loc[r["level"] == l, col].std() for l in levels]
        ax.bar(xs + off, m, 0.36, yerr=s, capsize=2, color=COL[col], label=col.capitalize())
    ax.axhline(THRESHOLD, color=COL["a"], ls="--", lw=0.9, label="Threshold 0.80")
    ax.set_xticks(xs, levels)
    ax.set_xlabel("Dust level")
    ax.set_ylabel("Soiling ratio")
    ax.set_ylim(0, 1.2)
    ax.legend(loc="upper right", frameon=False, ncol=3)
    save(fig, out, "fig3_cleaning")
    return (f"## E4 Cleaning\n\n{md}\n![Fig. 3](figures/fig3_cleaning.png)\n\n"
            "*Fig. 3. Soiling ratio before and after one wiper cycle at each dust level (mean ± SD over runs). "
            f"Before: {abs(E4_BEFORE[0])}–{abs(E4_BEFORE[1])} s before the wipe; after: {E4_AFTER[0]}–{E4_AFTER[1]} s after it.*\n")


def e5(df, out):
    d = df[df["exp"] == "E5"]
    marks = d[d["mark"].str.startswith("inject_")]
    if marks.empty:
        return "## E5 Fault injection\n\nNo `mark inject_<fault>` lines in E5 data.\n"
    trials = []
    for _, m in marks.iterrows():
        key = m["mark"][len("inject_"):]
        codes, name = E5_EXPECTED.get(key, ([], key))
        g = d[(d["file"] == m["file"]) & (d["t"] > m["t"]) & (d["t"] <= m["t"] + E5_TIMEOUT_S)]
        nxt = g[g["mark"].str.startswith("inject_")]
        if not nxt.empty:  # stop at the next injection
            g = g[g["t"] < nxt["t"].iloc[0]]
        hit = g[g["faults"].apply(lambda f: any(c in f.split("|") for c in codes))] if codes else g.iloc[0:0]
        trials.append({"key": key, "fault": name, "detected": not hit.empty,
                       "delay": (hit["t"].iloc[0] - m["t"]) if not hit.empty else np.nan})
    tr = pd.DataFrame(trials)
    tr.to_csv(out / "tables" / "e5_fault_trials.csv", index=False)
    rows = []
    for key, g in tr.groupby("key", sort=False):
        dl = g["delay"].dropna()
        rows.append({"Injected fault": g["fault"].iloc[0], "Expected alert": " / ".join(E5_EXPECTED.get(key, ([key],))[0]),
                     "Trials": len(g), "Detected": f"{int(g['detected'].sum())}/{len(g)}",
                     "Rate %": fmt(100 * g["detected"].mean(), 0),
                     "Delay s (mean ± SD)": f"{fmt(dl.mean(), 1)} ± {fmt(dl.std(), 1)}" if len(dl) else "-",
                     "Max delay s": fmt(dl.max(), 1) if len(dl) else "-"})
    md = write_table(pd.DataFrame(rows), out, "e5_faults", "Fault detection rate and delay from injection to alert (E5).")
    return (f"## E5 Fault injection\n\n{md}\nDelay = time from `mark inject_<fault>` to the first CSV line showing the expected "
            f"fault code (limit {E5_TIMEOUT_S} s). Per-trial results: `tables/e5_fault_trials.csv`.\n")


def e6(df, out):
    d = df[(df["exp"] == "E6") & df["cond"].isin(["track", "fixed"])]
    if d.empty:
        return "## E6 Tracking vs fixed\n\nNo E6 data.\n"
    blk = (d.groupby(["cond", "session"])
             .agg(current=("current_a", "mean"), power=("power_w", "mean"), n=("current_a", "size"))
             .reset_index())
    piv = blk.pivot(index="session", columns="cond", values="current").dropna()
    rows = []
    for ses, r in piv.iterrows():
        rows.append({"Block pair": ses, "Tracking mA": fmt(1000 * r["track"], 1), "Fixed mA": fmt(1000 * r["fixed"], 1),
                     "Gain %": fmt(100 * (r["track"] - r["fixed"]) / r["fixed"], 1)})
    if len(piv):
        g = 100 * (piv["track"] - piv["fixed"]) / piv["fixed"]
        rows.append({"Block pair": "Mean", "Tracking mA": fmt(1000 * piv["track"].mean(), 1),
                     "Fixed mA": fmt(1000 * piv["fixed"].mean(), 1), "Gain %": f"{fmt(g.mean(), 1)} ± {fmt(g.std(), 1)}"})
    md = write_table(pd.DataFrame(rows), out, "e6_tracking", "Mean panel current, tracking vs fixed flat, per block pair (E6).")
    fig, ax = plt.subplots(figsize=(3.5, 2.4))
    xs = np.arange(len(piv))
    ax.bar(xs - 0.18, 1000 * piv["fixed"], 0.36, color=COL["fixed"], label="Fixed (flat)")
    ax.bar(xs + 0.18, 1000 * piv["track"], 0.36, color=COL["track"], label="Tracking")
    ax.set_xticks(xs, list(piv.index))
    ax.set_xlabel("Block pair")
    ax.set_ylabel("Mean panel current (mA)")
    ax.legend(loc="upper left", frameon=False, ncol=2)
    save(fig, out, "fig4_tracking_vs_fixed")
    return (f"## E6 Tracking vs fixed\n\n{md}\n![Fig. 4](figures/fig4_tracking_vs_fixed.png)\n\n"
            "*Fig. 4. Mean panel current in alternating tracking and fixed-flat blocks at the same time of day. "
            "Raw current is compared because the light sensors tilt with the panel.*\n")


# ---------------------------------------------------------------------------
def main():
    global SYNTHETIC
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("folder", nargs="?", help="folder with the raw Serial logs")
    ap.add_argument("--out", default=None, help="output folder (default analysis/output)")
    ap.add_argument("--synthetic", action="store_true", help="generate fake data and test the pipeline")
    args = ap.parse_args()
    here = Path(__file__).resolve().parent

    if args.synthetic:
        SYNTHETIC = True
        from synthetic import generate  # noqa: E402
        folder = here / "synthetic_input"
        generate(folder)
        out = Path(args.out) if args.out else here / "output_synthetic"
    else:
        if not args.folder:
            ap.error("give the folder with your logs, or --synthetic")
        folder = Path(args.folder)
        out = Path(args.out) if args.out else here / "output"
    (out / "figures").mkdir(parents=True, exist_ok=True)
    (out / "tables").mkdir(parents=True, exist_ok=True)

    style()
    print(f"Loading {folder}")
    raw = add_settle(load_folder(folder))
    cl = clean(raw)
    print(f"{len(raw)} rows loaded, {len(cl)} kept after cleaning")
    print("Labels found:", ", ".join(sorted(raw["label"].unique())))

    parts = []
    if SYNTHETIC:
        parts.append("> **SYNTHETIC TEST DATA. These numbers were generated by analysis/synthetic.py to test the "
                     "pipeline. They are not experimental results and must not be reported.**\n")
    parts.append(f"# Results\n\nRows loaded: {len(raw)}; kept after cleaning: {len(cl)} "
                 f"(dropped: first {SETTLE_S} s after each change or mark, wiper running, self-test, sensor errors, -127 °C).\n")
    parts.append(e1(cl, out))
    parts.append(e2(cl, out))
    parts.append(e3(raw, out))          # uses all rows: a cloud pass is itself a change
    parts.append(e4(raw, out))          # needs the wiper rows to find each run
    parts.append(e5(raw, out))          # needs fault rows and marks
    parts.append(e6(cl, out))
    (out / "summary.md").write_text("\n".join(parts))
    print(f"Done: {out / 'summary.md'}")


if __name__ == "__main__":
    main()
