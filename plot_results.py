#!/usr/bin/env python3
"""
Plot RTT, SRTT and RTO comparison across three algorithms.

Reads:  {mode}_data_{scenario}.csv  (columns: Time,NodeId,FlowId,RTT,SRTT,RTTVAR,RTO)
Writes: figure2_increase.png / figure3_decrease.png
       
        table1_deviation.csv / table1_deviation.md
"""

import argparse
from pathlib import Path

import numpy as np
import pandas as pd
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

ALGO_STYLE = {
    "standard": dict(
        rto_color="#c0392b", srtt_color="#e08080",
        label="Standard (Jacobson)"),
    "improved": dict(
        rto_color="#e67e22", srtt_color="#f0b060",
        label="Improved (Xiao-Zhang)"),
    "elrto": dict(
        rto_color="#27ae60", srtt_color="#70d090",
        label="EL-RTO (Jude 2022)"),
}
RTT_COLOR = "#2980b9"
ALL_ALGOS = ["standard", "improved", "elrto"]


def load_data(prefix: str, scenario: str) -> pd.DataFrame:
    """Load unified CSV, filter to flow 0 for single representative curve."""
    p = Path(f"{prefix}_data_{scenario}.csv")
    if not p.exists():
        raise FileNotFoundError(str(p))

    df = pd.read_csv(p, comment="#")
    required = {"Time", "RTT(ms)", "SRTT(ms)", "RTTVAR(ms)", "RTO(ms)"}
    missing = required - set(df.columns)
    if missing:
        raise ValueError(f"{p} missing columns: {missing}")

    # Filter to flow 0 for a clean single-line plot
    if "FlowId" in df.columns:
        df = df[df["FlowId"] == 0].copy()

    for col in df.columns:
        df[col] = pd.to_numeric(df[col], errors="coerce")

    df = df.dropna(subset=["Time", "RTT(ms)", "RTO(ms)"])
    df = df.sort_values("Time").reset_index(drop=True)

    # Gamma = 100 * |RTO - RTT| / RTT
    df["Gamma(%)"] = (
        100.0 * np.abs(df["RTO(ms)"] - df["RTT(ms)"])
        / df["RTT(ms)"]
    )
    return df


def try_load(prefix: str, scenario: str):
    try:
        df = load_data(prefix, scenario)
        print(f"  Loaded {prefix}/{scenario}: {len(df)} rows")
        return df
    except FileNotFoundError:
        print(f"  [skip] {prefix}/{scenario}: file not found")
        return None
    except Exception as e:
        print(f"  [warn] {prefix}/{scenario}: {e}")
        return None


def sample_n(df: pd.DataFrame, n: int = 21) -> pd.DataFrame:
    if df.empty:
        return df
    idx = np.linspace(0, len(df) - 1, min(n, len(df)), dtype=int)
    out = df.iloc[idx].copy().reset_index(drop=True)
    out.insert(0, "Sample", np.arange(1, len(out) + 1))
    return out


def load_throughput(prefix: str, scenario: str) -> float:
    p = Path(f"{prefix}_throughput_{scenario}.csv")
    if not p.exists():
        return float("nan")
    df = pd.read_csv(p)
    if "Throughput(Mbps)" not in df.columns or df.empty:
        return float("nan")
    return float(df.iloc[0]["Throughput(Mbps)"])


# ── Figure: side-by-side subplots (one per algorithm) ─────────────

def plot_figure(scenario: str, datasets: dict) -> None:
    """
    Each subplot shows:
      - Raw RTT (blue dashed)  — what the network measured
      - SRTT   (light solid)   — smoothed estimate
      - RTO    (dark solid)    — what triggers retransmission

    The GAP between RTO and RTT is what the paper measures (gamma %).
    Smaller gap = better algorithm = less unnecessary retransmission.
    """
    fig_num = "2" if scenario == "increase" else "3"
    title   = "RTT rapid increase" if scenario == "increase" else "RTT rapid decrease"

    n = len(datasets)
    fig, axes = plt.subplots(1, n, figsize=(6 * n, 5), sharey=True)
    if n == 1:
        axes = [axes]

    for ax, (prefix, df) in zip(axes, datasets.items()):
        s   = sample_n(df, 21)
        sty = ALGO_STYLE[prefix]

        ax.plot(s["Sample"], s["RTO(ms)"],
                color=sty["rto_color"], linewidth=2.5,
                label="RTO")
        if "SRTT(ms)" in s.columns:
            ax.plot(s["Sample"], s["SRTT(ms)"],
                    color=sty["srtt_color"], linewidth=1.8,
                    linestyle="-.", label="SRTT")
        ax.plot(s["Sample"], s["RTT(ms)"],
                color=RTT_COLOR, linewidth=1.8,
                linestyle="--", label="RTT (raw)")

        ax.fill_between(s["Sample"], s["RTT(ms)"], s["RTO(ms)"],
                        alpha=0.12, color=sty["rto_color"],
                        label="RTO–RTT gap (γ)")

        ax.set_xlabel("Sampling sequence", fontsize=10)
        ax.set_title(sty["label"], fontsize=10, pad=8)
        ax.grid(True, linestyle="--", alpha=0.35)
        ax.legend(fontsize=8, loc="upper left")

    axes[0].set_ylabel("Time (ms)", fontsize=10)
    fig.suptitle(f"Figure {fig_num} — {title}", fontsize=12, y=1.02)
    fig.tight_layout()

    out = Path(f"figure{fig_num}_{scenario}.png")
    fig.savefig(out, dpi=220, bbox_inches="tight")
    plt.close(fig)
    print(f"Generated: {out}")


# ── Figure: all algorithms overlaid ───────────────────────────────

def plot_comparison(scenario: str, datasets: dict) -> None:
    """All RTO lines on one plot with shared RTT for direct comparison."""
    fig, ax = plt.subplots(figsize=(10, 5))
    rtt_plotted = False

    for prefix, df in datasets.items():
        s   = sample_n(df, 21)
        sty = ALGO_STYLE[prefix]
        ax.plot(s["Sample"], s["RTO(ms)"],
                color=sty["rto_color"], linewidth=2.2,
                label=f"RTO — {sty['label']}")
        if not rtt_plotted:
            ax.plot(s["Sample"], s["RTT(ms)"],
                    color=RTT_COLOR, linewidth=1.8,
                    linestyle="--", label="RTT (actual)")
            rtt_plotted = True

    title = "RTT rapid increase" if scenario == "increase" else "RTT rapid decrease"
    ax.set_xlabel("Sampling sequence", fontsize=11)
    ax.set_ylabel("Time (ms)", fontsize=11)
    ax.set_title(f"Algorithm comparison — {title}", fontsize=12)
    ax.grid(True, linestyle="--", alpha=0.35)
    ax.legend(fontsize=9)
    fig.tight_layout()

    out = Path(f"comparison_{scenario}.png")
    fig.savefig(out, dpi=220, bbox_inches="tight")
    plt.close(fig)
    print(f"Generated: {out}")


# ── Gamma deviation table ──────────────────────────────────────────

def build_table(all_data: dict, scenarios: list) -> pd.DataFrame:
    cols = {}
    for scen in scenarios:
        for prefix, df in all_data.get(scen, {}).items():
            s = sample_n(df[["Time", "Gamma(%)"]], 21)
            cols[f"{scen}_{prefix}_gamma(%)"] = \
                s["Gamma(%)"].reset_index(drop=True)

    if not cols:
        raise RuntimeError("No data for table.")

    n = min(len(v) for v in cols.values())
    table = pd.DataFrame({"sampling_sequence": np.arange(1, n + 1)})
    for col, series in cols.items():
        table[col] = series.iloc[:n].round(2).values

    avg = {"sampling_sequence": "avg"}
    for col in table.columns[1:]:
        avg[col] = round(table[col].mean(), 2)
    table = pd.concat([table, pd.DataFrame([avg])], ignore_index=True)

    tp = {"sampling_sequence": "throughput(Mbps)"}
    for scen in scenarios:
        for prefix in all_data.get(scen, {}):
            tp[f"{scen}_{prefix}_gamma(%)"] = \
                round(load_throughput(prefix, scen), 3)
    table = pd.concat([table, pd.DataFrame([tp])], ignore_index=True)
    return table


def write_table(table: pd.DataFrame) -> None:
    table.to_csv("table1_deviation.csv", index=False)
    hdr   = list(table.columns)
    lines = ["| " + " | ".join(hdr) + " |",
             "| " + " | ".join(["---"] * len(hdr)) + " |"]
    for _, row in table.iterrows():
        lines.append("| " + " | ".join(str(row[h]) for h in hdr) + " |")
    Path("table1_deviation.md").write_text(
        "\n".join(lines) + "\n", encoding="utf-8")
    print("Generated: table1_deviation.csv")
    print("Generated: table1_deviation.md")


# ── Main ───────────────────────────────────────────────────────────

def run(mode: str) -> None:
    scenarios = (["increase"] if mode == "increase" else
                 ["decrease"] if mode == "decrease" else
                 ["increase", "decrease"])

    all_data: dict = {}
    for scen in scenarios:
        all_data[scen] = {}
        for prefix in ALL_ALGOS:
            df = try_load(prefix, scen)
            if df is not None:
                all_data[scen][prefix] = df
        if not all_data[scen]:
            print(f"[error] No data for '{scen}'. Run simulations first.")
            continue
        plot_figure(scen, all_data[scen])
        plot_comparison(scen, all_data[scen])

    if mode == "both":
        try:
            tbl = build_table(all_data, scenarios)
            write_table(tbl)
        except Exception as e:
            print(f"[warn] Table skipped: {e}")


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("mode", nargs="?", default="both",
                    choices=["increase", "decrease", "both"])
    run(ap.parse_args().mode)


if __name__ == "__main__":
    main()