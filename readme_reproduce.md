# Supervisor Reproduction Guide

This is the shortest command set needed to regenerate the main results from the repository.

## 1. Clone and enter the repository

```bash
git clone https://github.com/sleepyMe04/NS3-Project.git
cd NS3-Project
```

## 2. Build ns-3

```bash
./ns3 configure
./ns3 build
```

## 3. Reproduce the base-paper results

```bash
./ns3 run "rto-smooth-simulation --mode=standard --scenario=increase"
./ns3 run "rto-smooth-simulation --mode=improved --scenario=increase"
./ns3 run "rto-smooth-simulation --mode=standard --scenario=decrease"
./ns3 run "rto-smooth-simulation --mode=improved --scenario=decrease"
python3 plot_results.py both
```

## 4. Reproduce the EL-RTO spike experiment

```bash
./ns3 run "rto-elrto-spike-simulation --mode=standard"
./ns3 run "rto-elrto-spike-simulation --mode=improved"
./ns3 run "rto-elrto-spike-simulation --mode=elrto"
python3 plot_elrto_spike.py
```

## 5. Reproduce the assigned wireless checklist results

```bash
python3 run_lrwpan_mobile_10s.py
python3 run_wifi_mobile_10s.py
```

## 6. Main outputs

These commands regenerate the key outputs:

- `figure2_increase.png`
- `figure3_decrease.png`
- `table1_deviation.csv`
- `elrto_spike_rtt.png`
- `elrto_spike_rttvar.png`
- `elrto_spike_rto.png`
- `elrto_spike_gamma.png`
- `elrto_spike_summary.csv`
- `student2105114_lrwpan_mobile_10s.csv`
- `student2105114_wifi_mobile_10s.csv`
- all checklist plot folders

All final generated graphs, CSVs, summaries, and report artifacts are organized in:

- `2105114_final_submission/`

Important subfolders inside it:

- `2105114_final_submission/base_paper_results/`
- `2105114_final_submission/base_paper_elrto_artifacts/`
- `2105114_final_submission/wifi_mobile/`
- `2105114_final_submission/lrwpan_mobile/`

## 7. Assigned networks

For student ID `2105114`, the assigned checklist networks are:

- `Wireless 802.11 (mobile)`
- `Wireless 802.15.4 (mobile)`

## 8. Main modified and added files

The most important project files are:

### Modified core ns-3 files

- `src/internet/model/rtt-estimator.h`
- `src/internet/model/rtt-estimator.cc`

### Main experiment drivers

- `scratch/rto-smooth-simulation.cc`
- `scratch/rto-elrto-spike-simulation.cc`
- `scratch/rto-checklist-wireless.cc`

### Plotting and automation scripts

- `plot_results.py`
- `plot_elrto_spike.py`
- `plot_checklist_wireless.py`
- `run_selected_checklist.py`
- `run_lrwpan_mobile_10s.py`
- `run_wifi_mobile_10s.py`


