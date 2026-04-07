#!/usr/bin/env python3

from __future__ import annotations

import subprocess
from pathlib import Path


OUTPUT_CSV = "student2105114_wifi_mobile_10s.csv"
PLOT_DIR = "student2105114_wifi_mobile_10s_plots"


def main() -> None:
    repo = Path(__file__).resolve().parent

    run_cmd = [
        "python3",
        "run_selected_checklist.py",
        "--output-csv",
        OUTPUT_CSV,
        "--sim-time",
        "10",
        "--networks",
        "wifi-mobile",
    ]
    plot_cmd = [
        "python3",
        "plot_checklist_wireless.py",
        OUTPUT_CSV,
        "--out-dir",
        PLOT_DIR,
    ]

    subprocess.run(run_cmd, cwd=repo, check=True)
    subprocess.run(plot_cmd, cwd=repo, check=True)


if __name__ == "__main__":
    main()
