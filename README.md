# NS-3 TCP RTO Estimator Study



A comparative study of TCP Retransmission Timeout (RTO) estimation algorithms implemented inside the **ns-3** discrete-event network simulator. The project reproduces and extends the results of a 2015 research paper by implementing three RTO modes and benchmarking them across wired bottleneck and wireless network scenarios.

---

## Background

Standard TCP uses the **Jacobson/Karels algorithm** (RFC 2988) to estimate RTO:

```
SRTT    ← (1 - α) * SRTT + α * RTT          [α = 1/8]
RTTVAR  ← (1 - β) * RTTVAR + β * |SRTT - RTT|  [β = 1/4]
RTO     ← SRTT + 4 * RTTVAR
```

The fixed weights α and β are non-adaptive — when RTT changes rapidly (e.g. sudden congestion or recovery), RTO lags severely behind RTT, wasting bandwidth or causing premature timeouts.

This project implements and compares three modes:

| Mode | Description |
|------|-------------|
| `standard` | Unmodified Jacobson/Karels algorithm (RFC 2988 baseline) |
| `improved` | Adaptive α/β based on RTT rate-of-change *k* — Xiao & Zhang, AMEII 2015 |
| `elrto` | `improved` + EL-RTO spike suppression using a sliding window mean — Jude et al., Wireless Networks 2022 |

---

## The Improved Algorithm (Xiao & Zhang 2015)

The key insight: RTT's **rate of change** *k* reflects network state better than RTT magnitude alone.

```
k     = |RTT_n - RTT_{n-1}| / RTT_{n-1}     (capped at 1.0)
α_n   = α₀ × (1 + k)     → increases when RTT changes fast
β_n   = β₀ × (1 - k)     → decreases when RTT changes fast
```

Effect: RTO tracks RTT more tightly during congestion bursts and recovers faster after them.

---

## The EL-RTO Extension (Jude et al. 2022)

Wireless networks produce RTT spikes from collision/contention that don't reflect true congestion. EL-RTO suppresses these before they inflate RTTVAR:

```
S_n       = mean of last M raw RTT samples   (sliding window)
RTT'_n+1  = (RTT > θ × S) ? S : RTT         (spike clamped to window mean)
```

SRTT and RTTVAR are then updated with `RTT'` instead of the raw spike.

---

## Implementation

Both algorithms are implemented by modifying ns-3's core RTT estimator:

- **`src/internet/model/rtt-estimator.h`** — added `UseAdaptive`, `UseElRto`, `ElRtoWindow`, `ElRtoTheta` attributes; sliding window state; unified logging interface
- **`src/internet/model/rtt-estimator.cc`** — implemented `CalculateChangeRate()`, `UpdateAdaptiveWeights()`, `ApplySpikeSuppression()`; modified `Measurement()` to dispatch to the right update path; added per-socket CSV logging

All three modes are selectable at simulation time via ns-3 attributes — no recompilation needed between runs.

---

## Assigned Wireless Networks (Student ID: 2105114)

- **Wireless 802.11 (mobile)** — Wi-Fi infrastructure, fixed-rate, mobile nodes
- **Wireless 802.15.4 / LR-WPAN (mobile)** — 6LoWPAN + IPv6, mobile nodes

---

## Results Overview

**Base paper reproduction** (`rto-smooth-simulation`): Smooth bottleneck with increasing/decreasing load. Generates deviation metric γ = |RTO − RTT| / RTT × 100%.

**EL-RTO spike experiment** (`rto-elrto-spike-simulation`): Artificial RTT spike injected mid-flow. Measures how quickly each mode recovers.

**Wireless checklist**: Both assigned wireless topologies run for 10s. Compares throughput, RTT tracking, and RTO stability across all three modes.

---

## References

1. Xiao Jianliang, Zhang Kun — *"Improved RTO Algorithm for TCP Retransmission Timeout"*, AMEII 2015
2. M. Joseph Auxilius Jude et al. — *"EL-RTO: Enhanced RTO for TCP in Wireless Networks"*, Wireless Networks 2022
3. Jacobson V. — *"Congestion Avoidance and Control"*, ACM SIGCOMM 1988
4. RFC 2988 — *Computing TCP's Retransmission Timer*

---

## How to Reproduce Results

See the detailed **[reproduction guide below](#reproducing-the-ns-3-project-results)** for full build and run instructions.




---

---

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

## 4. Reproduce The Checklist Wireless Results

### 4.1 Wireless 802.15.4 (mobile)

```bash
python3 run_lrwpan_mobile_10s.py
```

### 4.2 Wireless 802.11 (mobile)

```bash
python3 run_wifi_mobile_10s.py
```

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
- `run_lrwpan_mobile_10s.py`
- `run_wifi_mobile_10s.py`

## 6. Notes

- The checklist wireless runs use `simTime = 10s` for practicality.
- The Wi-Fi checklist topology uses a fixed-rate infrastructure setup for stability.
- The LR-WPAN checklist topology uses `6LoWPAN + IPv6`.
- In some wireless plots, `improved` and `elrto` may overlap because end-to-end behavior is dominated by wireless contention rather than timer differences alone.

## 7. Submission Artifacts

The final assembled outputs are stored under `2105114_final_submission/`:

- `base_paper_results/`
- `base_paper_elrto_artifacts/`
- `wifi_mobile/`
- `lrwpan_mobile/`

## 8. Minimal Reproduction Command Set

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
