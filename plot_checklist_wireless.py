#!/usr/bin/env python3

from pathlib import Path

import argparse
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import pandas as pd

COLORS = {
    "standard": "#c0392b",
    "improved": "#f39c12",
    "elrto": "#27ae60",
}

STYLES = {
    "standard": {"linestyle": "-", "marker": "o"},
    "improved": {"linestyle": "-.", "marker": "s"},
    "elrto": {"linestyle": "--", "marker": "^"},
}

LABELS = {
    "standard": "Standard",
    "improved": "Improved",
    "elrto": "EL-RTO",
    "wifi-mobile": "Wireless 802.11 (mobile)",
    "lrwpan-mobile": "Wireless 802.15.4 (mobile)",
}

SWEEPS = {
    "nodes": {
        "column": "nodes",
        "values": [20, 40, 60, 80, 100],
        "filters": {"flows": 10, "pps": 100, "speed": 5, "areaScale": 2.0},
        "xlabel": "Number of nodes",
    },
    "flows": {
        "column": "flows",
        "values": [10, 20, 30, 40, 50],
        "filters": {"nodes": 100, "pps": 100, "speed": 5, "areaScale": 2.0},
        "xlabel": "Number of flows",
    },
    "pps": {
        "column": "pps",
        "values": [100, 200, 300, 400, 500],
        "filters": {"nodes": 100, "flows": 10, "speed": 5, "areaScale": 2.0},
        "xlabel": "Packets per second",
    },
    "speed": {
        "column": "speed",
        "values": [5, 10, 15, 20, 25],
        "filters": {"nodes": 100, "flows": 10, "pps": 100, "areaScale": 2.0},
        "xlabel": "Node speed (m/s)",
    },
}

METRICS = {
    "throughputMbps": {
        "ylabel": "Throughput (Mbps)",
        "title": "Network Throughput",
    },
    "averageDelayMs": {
        "ylabel": "End-to-end delay (ms)",
        "title": "End-to-End Delay",
    },
    "pdrPct": {
        "ylabel": "Packet delivery ratio (%)",
        "title": "Packet Delivery Ratio",
    },
    "dropRatioPct": {
        "ylabel": "Packet drop ratio (%)",
        "title": "Packet Drop Ratio",
    },
    "energyJ": {
        "ylabel": "Energy consumption (J)",
        "title": "Energy Consumption",
    },
}


def load_results(path: Path) -> pd.DataFrame:
    df = pd.read_csv(path)
    numeric_columns = [
        "nodes",
        "flows",
        "pps",
        "speed",
        "areaScale",
        "packetSize",
        "throughputMbps",
        "averageDelayMs",
        "pdr",
        "dropRatio",
        "energyJ",
        "txPackets",
        "rxPackets",
        "txBytes",
        "rxBytes",
    ]
    for col in numeric_columns:
        df[col] = pd.to_numeric(df[col], errors="coerce")
    df = df.dropna().copy()
    df["pdrPct"] = 100.0 * df["pdr"]
    df["dropRatioPct"] = 100.0 * df["dropRatio"]
    return df


def filter_for_sweep(df: pd.DataFrame, network: str, mode: str, sweep_name: str) -> pd.DataFrame:
    spec = SWEEPS[sweep_name]
    subset = df[(df["network"] == network) & (df["mode"] == mode)].copy()
    for key, value in spec["filters"].items():
        subset = subset[subset[key] == value]
    subset = subset[subset[spec["column"]].isin(spec["values"])]
    subset = subset.sort_values(spec["column"]).drop_duplicates(
        subset=["network", "mode", "nodes", "flows", "pps", "speed", "areaScale", "packetSize"],
        keep="last",
    )
    return subset


def series_equal(left: pd.DataFrame, right: pd.DataFrame, xcol: str, ycol: str) -> bool:
    if left.empty or right.empty:
        return False
    a = left[[xcol, ycol]].reset_index(drop=True).round(9)
    b = right[[xcol, ycol]].reset_index(drop=True).round(9)
    return a.equals(b)


def plot_metric(df: pd.DataFrame, network: str, sweep_name: str, metric: str, out_dir: Path) -> None:
    spec = SWEEPS[sweep_name]
    metric_spec = METRICS[metric]

    plt.figure(figsize=(8.8, 5))
    plotted = {}
    for mode in ["standard", "improved", "elrto"]:
        subset = filter_for_sweep(df, network, mode, sweep_name)
        if subset.empty:
            continue
        plotted[mode] = subset
        plt.plot(
            subset[spec["column"]],
            subset[metric],
            marker=STYLES[mode]["marker"],
            linewidth=2,
            linestyle=STYLES[mode]["linestyle"],
            color=COLORS[mode],
            label=LABELS[mode],
        )

    plt.xlabel(spec["xlabel"])
    plt.ylabel(metric_spec["ylabel"])
    plt.title(f"{LABELS[network]}: {metric_spec['title']} vs {spec['xlabel']}")
    plt.grid(True, linestyle="--", alpha=0.35)
    plt.legend()
    if "improved" in plotted and "elrto" in plotted:
        if series_equal(plotted["improved"], plotted["elrto"], spec["column"], metric):
            plt.text(
                0.99,
                0.02,
                "Improved and EL-RTO overlap exactly",
                transform=plt.gca().transAxes,
                ha="right",
                va="bottom",
                fontsize=8,
                bbox=dict(boxstyle="round,pad=0.25", facecolor="white", alpha=0.8, edgecolor="#95a5a6"),
            )
    plt.tight_layout()

    out = out_dir / f"{network}_{sweep_name}_{metric}.png"
    plt.savefig(out, dpi=220, bbox_inches="tight")
    plt.close()
    print(f"Generated: {out}")


def build_summary(df: pd.DataFrame, out_dir: Path) -> Path:
    rows = []
    networks = [network for network in ["wifi-mobile", "lrwpan-mobile"] if network in set(df["network"])]
    for network in networks:
        for sweep_name, spec in SWEEPS.items():
            for mode in ["standard", "improved", "elrto"]:
                subset = filter_for_sweep(df, network, mode, sweep_name)
                if subset.empty:
                    continue
                rows.append(
                    {
                        "network": network,
                        "sweep": sweep_name,
                        "mode": mode,
                        "avg_throughput_mbps": round(subset["throughputMbps"].mean(), 6),
                        "avg_delay_ms": round(subset["averageDelayMs"].mean(), 6),
                        "avg_pdr_pct": round(subset["pdrPct"].mean(), 6),
                        "avg_drop_ratio_pct": round(subset["dropRatioPct"].mean(), 6),
                        "avg_energy_j": round(subset["energyJ"].mean(), 6),
                    }
                )
    out = out_dir / "student_base_summary.csv"
    pd.DataFrame(rows).to_csv(out, index=False)
    print(f"Generated: {out}")
    return out


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("results_csv", nargs="?", default="student2105114_base_results.csv")
    parser.add_argument("--out-dir", default="student2105114_base_plots")
    args = parser.parse_args()

    results_csv = Path(args.results_csv)
    if not results_csv.exists():
        raise FileNotFoundError(results_csv)

    out_dir = Path(args.out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)

    df = load_results(results_csv)
    networks = [network for network in ["wifi-mobile", "lrwpan-mobile"] if network in set(df["network"])]
    for network in networks:
        for sweep_name in SWEEPS:
            for metric in METRICS:
                plot_metric(df, network, sweep_name, metric, out_dir)

    build_summary(df, out_dir)


if __name__ == "__main__":
    main()
