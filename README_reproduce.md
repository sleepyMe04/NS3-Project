# Reproducing The NS-3 Project Results

This repository contains the modified ns-3 source, experiment drivers, and plotting scripts needed to regenerate the project outputs.

The project compares three TCP timeout modes:

- `standard`
- `improved`
- `elrto`

The assigned checklist networks for student ID `2105114` are:

- `Wireless 802.11 (mobile)`
- `Wireless 802.15.4 (mobile)`

## 1. Build ns-3

From the repository root:

```bash
./ns3 configure
./ns3 build
```

If the project was already built, you can usually skip this step unless source files changed.

## 2. Reproduce The Base-Paper Results

Run the smooth bottleneck simulation for both scenarios and all three modes:

```bash
./ns3 run "rto-smooth-simulation --mode=standard --scenario=increase"
./ns3 run "rto-smooth-simulation --mode=improved --scenario=increase"
./ns3 run "rto-smooth-simulation --mode=elrto --scenario=increase"

./ns3 run "rto-smooth-simulation --mode=standard --scenario=decrease"
./ns3 run "rto-smooth-simulation --mode=improved --scenario=decrease"
./ns3 run "rto-smooth-simulation --mode=elrto --scenario=decrease"
```

Then generate the figures and gamma table:

```bash
python3 plot_results.py both
```

This will regenerate files such as:

- `figure2_increase.png`
- `figure3_decrease.png`
- `table1_deviation.csv`
- `table1_deviation.md`

## 3. Reproduce The EL-RTO Spike Experiment

Run the spike experiment for all three modes:

```bash
./ns3 run "rto-elrto-spike-simulation --mode=standard"
./ns3 run "rto-elrto-spike-simulation --mode=improved"
./ns3 run "rto-elrto-spike-simulation --mode=elrto"
```

Then generate the spike plots and summary:

```bash
python3 plot_elrto_spike.py
```

This regenerates:

- `standard_spike_data.csv`
- `improved_spike_data.csv`
- `elrto_spike_data.csv`
- `standard_spike_throughput.csv`
- `improved_spike_throughput.csv`
- `elrto_spike_throughput.csv`
- `elrto_spike_rtt.png`
- `elrto_spike_rttvar.png`
- `elrto_spike_rto.png`
- `elrto_spike_gamma.png`
- `elrto_spike_summary.csv`

## 4. Reproduce The Checklist Wireless Results

The checklist wireless runs were split by network for convenience.

### 4.1 Wireless 802.15.4 (mobile)

```bash
python3 run_lrwpan_mobile_10s.py
```

This regenerates:

- `student2105114_lrwpan_mobile_10s.csv`
- `student2105114_lrwpan_mobile_10s_plots/`
- `student_base_summary.csv` inside the plot folder

### 4.2 Wireless 802.11 (mobile)

```bash
python3 run_wifi_mobile_10s.py
```

This regenerates:

- `student2105114_wifi_mobile_10s.csv`
- `student2105114_wifi_mobile_10s_plots/`
- `student_base_summary.csv` inside the plot folder

## 5. Files That Drive The Experiments

Core modified estimator:

- `src/internet/model/rtt-estimator.h`
- `src/internet/model/rtt-estimator.cc`

Main experiment drivers:

- `scratch/rto-smooth-simulation.cc`
- `scratch/rto-elrto-spike-simulation.cc`
- `scratch/rto-checklist-wireless.cc`

Plotting and automation:

- `plot_results.py`
- `plot_elrto_spike.py`
- `plot_checklist_wireless.py`
- `run_selected_checklist.py`
- `run_lrwpan_mobile_10s.py`
- `run_wifi_mobile_10s.py`

## 6. Notes

- The checklist wireless runs use `simTime = 10s` for practicality.
- The Wi-Fi checklist topology uses a fixed-rate infrastructure setup for stability.
- The LR-WPAN checklist topology uses `6LoWPAN + IPv6`.
- In some wireless plots, `improved` and `elrto` may overlap because the end-to-end network behavior is dominated by wireless contention and capacity rather than timer differences alone.

## 7. Submission Artifacts

The final assembled outputs are stored under:

- `2105114_final_submission/`

Important output groups inside that folder include:

- `base_paper_results/`
- `base_paper_elrto_artifacts/`
- `wifi_mobile/`
- `lrwpan_mobile/`

## 8. Minimal Reproduction Command Set

If you want the shortest path to regenerate the main outputs:

```bash
./ns3 build

./ns3 run "rto-smooth-simulation --mode=standard --scenario=increase"
./ns3 run "rto-smooth-simulation --mode=improved --scenario=increase"
./ns3 run "rto-smooth-simulation --mode=standard --scenario=decrease"
./ns3 run "rto-smooth-simulation --mode=improved --scenario=decrease"
python3 plot_results.py both

./ns3 run "rto-elrto-spike-simulation --mode=standard"
./ns3 run "rto-elrto-spike-simulation --mode=improved"
./ns3 run "rto-elrto-spike-simulation --mode=elrto"
python3 plot_elrto_spike.py

python3 run_lrwpan_mobile_10s.py
python3 run_wifi_mobile_10s.py
```


