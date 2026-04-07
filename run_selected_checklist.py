#!/usr/bin/env python3

from __future__ import annotations

import argparse
import csv
import subprocess
from pathlib import Path


CASES = []

for nodes in [20, 40, 60, 80, 100]:
    CASES.append({"nodes": nodes, "flows": 10, "pps": 100, "speed": 5.0})

for flows in [10, 20, 30, 40, 50]:
    CASES.append({"nodes": 100, "flows": flows, "pps": 100, "speed": 5.0})

for pps in [100, 200, 300, 400, 500]:
    CASES.append({"nodes": 100, "flows": 10, "pps": pps, "speed": 5.0})

for speed in [5.0, 10.0, 15.0, 20.0, 25.0]:
    CASES.append({"nodes": 100, "flows": 10, "pps": 100, "speed": speed})


def load_completed(path: Path) -> set[tuple[str, str, int, int, int, float]]:
    if not path.exists():
        return set()

    completed = set()
    with path.open(newline="", encoding="utf-8") as f:
        reader = csv.DictReader(f)
        for row in reader:
            completed.add(
                (
                    row["network"],
                    row["mode"],
                    int(float(row["nodes"])),
                    int(float(row["flows"])),
                    int(float(row["pps"])),
                    float(row["speed"]),
                )
            )
    return completed


def build_command(network: str, mode: str, output_csv: str, sim_time: int, case: dict) -> list[str]:
    return [
        "./ns3",
        "run",
        (
            "rto-checklist-wireless "
            f"--network={network} "
            f"--mode={mode} "
            f"--simTime={sim_time} "
            f"--outputCsv={output_csv} "
            f"--nodes={case['nodes']} "
            f"--flows={case['flows']} "
            f"--pps={case['pps']} "
            f"--speed={case['speed']}"
        ),
    ]


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--output-csv", default="student2105114_selected_10s.csv")
    parser.add_argument("--sim-time", type=int, default=10)
    parser.add_argument("--networks", nargs="+", default=["wifi-mobile", "lrwpan-mobile"])
    parser.add_argument("--modes", nargs="+", default=["standard", "improved", "elrto"])
    parser.add_argument("--limit", type=int, default=0, help="Run only the first N unfinished cases")
    args = parser.parse_args()

    output_path = Path(args.output_csv)
    completed = load_completed(output_path)
    launched = 0

    for network in args.networks:
        for mode in args.modes:
            for case in CASES:
                key = (
                    network,
                    mode,
                    case["nodes"],
                    case["flows"],
                    case["pps"],
                    case["speed"],
                )
                if key in completed:
                    continue

                cmd = build_command(network, mode, args.output_csv, args.sim_time, case)
                print(
                    f"[run] network={network} mode={mode} "
                    f"nodes={case['nodes']} flows={case['flows']} pps={case['pps']} speed={case['speed']}"
                )
                subprocess.run(cmd, check=True)
                launched += 1

                if args.limit and launched >= args.limit:
                    return


if __name__ == "__main__":
    main()
