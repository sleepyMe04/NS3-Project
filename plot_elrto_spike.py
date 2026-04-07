#!/usr/bin/env python3

from pathlib import Path

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import pandas as pd

FILES = {
    "standard": Path("standard_spike_data.csv"),
    "improved": Path("improved_spike_data.csv"),
    "elrto": Path("elrto_spike_data.csv"),
}

COLORS = {
    "standard": "#c0392b",
    "improved": "#f39c12",
    "elrto": "#27ae60",
}


def load_mode(mode: str) -> pd.DataFrame:
    path = FILES[mode]
    if not path.exists():
        raise FileNotFoundError(path)
    df = pd.read_csv(path)
    for col in ["Time", "RTT(ms)", "SRTT(ms)", "RTTVAR(ms)", "RTO(ms)"]:
        df[col] = pd.to_numeric(df[col], errors="coerce")
    df = df.dropna().sort_values("Time").reset_index(drop=True)
    df["Gap(ms)"] = (df["RTO(ms)"] - df["RTT(ms)"]).abs()
    df["Gamma(%)"] = 100.0 * df["Gap(ms)"] / df["RTT(ms)"]
    return df


def plot_metric(datasets, ycol, out, title, ylabel, spike_start=None, spike_end=None):
    plt.figure(figsize=(10, 5))
    for mode, df in datasets.items():
        plt.plot(df["Time"], df[ycol], label=mode, color=COLORS[mode], linewidth=2)
    if spike_start is not None and spike_end is not None:
        plt.axvspan(spike_start, spike_end, color="#95a5a6", alpha=0.15, label="spike window")
    plt.xlabel("Time (s)")
    plt.ylabel(ylabel)
    plt.title(title)
    plt.grid(True, linestyle="--", alpha=0.35)
    plt.legend()
    plt.tight_layout()
    plt.savefig(out, dpi=220, bbox_inches="tight")
    plt.close()


def build_summary_table(datasets, spike_start, spike_end):
    rows = []
    for mode, df in datasets.items():
        spike = df[(df["Time"] >= spike_start) & (df["Time"] <= spike_end)].copy()
        if spike.empty:
            continue
        rows.append(
            {
                "mode": mode,
                "peak_rtt_ms": round(spike["RTT(ms)"].max(), 3),
                "peak_rttvar_ms": round(spike["RTTVAR(ms)"].max(), 3),
                "peak_rto_ms": round(spike["RTO(ms)"].max(), 3),
                "avg_gap_ms": round(spike["Gap(ms)"].mean(), 3),
                "avg_gamma_pct": round(spike["Gamma(%)"].mean(), 3),
            }
        )
    table = pd.DataFrame(rows)
    table.to_csv("elrto_spike_summary.csv", index=False)
    return table


def main():
    spike_start = 10.0
    spike_end = 10.5

    datasets = {mode: load_mode(mode) for mode in FILES}

    plot_metric(datasets,
                "RTT(ms)",
                "elrto_spike_rtt.png",
                "RTT around spike-suppression experiment",
                "RTT (ms)",
                spike_start,
                spike_end)
    plot_metric(datasets,
                "RTTVAR(ms)",
                "elrto_spike_rttvar.png",
                "RTTVAR around spike-suppression experiment",
                "RTTVAR (ms)",
                spike_start,
                spike_end)
    plot_metric(datasets,
                "RTO(ms)",
                "elrto_spike_rto.png",
                "RTO around spike-suppression experiment",
                "RTO (ms)",
                spike_start,
                spike_end)
    plot_metric(datasets,
                "Gamma(%)",
                "elrto_spike_gamma.png",
                "RTO-RTT deviation around spike-suppression experiment",
                "Gamma (%)",
                spike_start,
                spike_end)

    table = build_summary_table(datasets, spike_start, spike_end)
    print(table.to_string(index=False))
    print("\nGenerated: elrto_spike_rtt.png")
    print("Generated: elrto_spike_rttvar.png")
    print("Generated: elrto_spike_rto.png")
    print("Generated: elrto_spike_gamma.png")
    print("Generated: elrto_spike_summary.csv")


if __name__ == "__main__":
    main()
