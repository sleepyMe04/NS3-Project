# Student 2105114 Project Report

## 1. Assignment Mapping

From the submission checklist, `2105114 % 8 = 2`.  
Therefore the required networks for this submission are:

- Wireless 802.11 (mobile)
- Wireless 802.15.4 (mobile)

Because both assigned cases are mobile, the checklist parameter `coverage area` is not part of the required sweep. The mobile-node parameter sweeps used here are:

- Number of nodes: `20, 40, 60, 80, 100`
- Number of flows: `10, 20, 30, 40, 50`
- Packets per second: `100, 200, 300, 400, 500`
- Node speed: `5, 10, 15, 20, 25 m/s`

## 2. Networks Under Simulation

### 2.1 Wireless 802.11 (mobile)

The Wi-Fi experiment is implemented in [rto-checklist-wireless.cc](/home/jarin/ns-allinone-3.45/ns-3.45/scratch/rto-checklist-wireless.cc) as the `wifi-mobile` network.

Topology summary:

- One sink/AP-side node is fixed at the center of the field.
- The remaining nodes are mobile stations.
- Stations use a `RandomWaypointMobilityModel`.
- TCP application traffic is generated from mobile nodes toward the sink node.
- Energy is measured through the Wi-Fi energy model.

### 2.2 Wireless 802.15.4 (mobile)

The LR-WPAN experiment is implemented in the same file as the `lrwpan-mobile` network.

Topology summary:

- One coordinator node is fixed at the center.
- Remaining nodes are mobile and use `RandomWaypointMobilityModel`.
- LR-WPAN devices are wrapped with 6LoWPAN and carry TCP traffic toward the coordinator.
- Energy is measured through an LR-WPAN PHY state tracker.

## 3. Base Paper and RTO Work

Earlier in this project, the RTO work was developed around three algorithmic modes:

- `standard`: Jacobson/RFC-style RTO behavior
- `improved`: Xiao-Zhang style adaptive estimator
- `elrto`: improved estimator plus EL-RTO spike suppression

This mapping is reflected in:

- [plot_results.py](/home/jarin/ns-allinone-3.45/ns-3.45/plot_results.py)
- [rto-smooth-simulation.cc](/home/jarin/ns-allinone-3.45/ns-3.45/scratch/rto-smooth-simulation.cc)
- [rto-elrto-spike-simulation.cc](/home/jarin/ns-allinone-3.45/ns-3.45/scratch/rto-elrto-spike-simulation.cc)

The generated paper-related artifacts included in this submission folder are:

- [table1_deviation.md](/home/jarin/ns-allinone-3.45/ns-3.45/2105114_final_submission/base_paper_elrto_artifacts/table1_deviation.md)
- [elrto_spike_summary.csv](/home/jarin/ns-allinone-3.45/ns-3.45/2105114_final_submission/base_paper_elrto_artifacts/elrto_spike_summary.csv)
- [elrto_spike_rtt.png](/home/jarin/ns-allinone-3.45/ns-3.45/2105114_final_submission/base_paper_elrto_artifacts/elrto_spike_rtt.png)
- [elrto_spike_rttvar.png](/home/jarin/ns-allinone-3.45/ns-3.45/2105114_final_submission/base_paper_elrto_artifacts/elrto_spike_rttvar.png)
- [elrto_spike_rto.png](/home/jarin/ns-allinone-3.45/ns-3.45/2105114_final_submission/base_paper_elrto_artifacts/elrto_spike_rto.png)
- [elrto_spike_gamma.png](/home/jarin/ns-allinone-3.45/ns-3.45/2105114_final_submission/base_paper_elrto_artifacts/elrto_spike_gamma.png)

### 3.1 Original base-paper replication folder

The folder [base_paper_results](/home/jarin/ns-allinone-3.45/ns-3.45/2105114_final_submission/base_paper_results) was also copied into the final submission so that the original paper-replication outputs are preserved separately from the checklist experiments.

It contains:

- [figure2_increase.png](/home/jarin/ns-allinone-3.45/ns-3.45/2105114_final_submission/base_paper_results/figure2_increase.png)
- [figure3_decrease.png](/home/jarin/ns-allinone-3.45/ns-3.45/2105114_final_submission/base_paper_results/figure3_decrease.png)
- [table1_deviation.csv](/home/jarin/ns-allinone-3.45/ns-3.45/2105114_final_submission/base_paper_results/table1_deviation.csv)
- [table1_deviation.md](/home/jarin/ns-allinone-3.45/ns-3.45/2105114_final_submission/base_paper_results/table1_deviation.md)
- the underlying standard/improved RTT, RTO, and throughput CSV traces for increase and decrease scenarios

From [table1_deviation.md](/home/jarin/ns-allinone-3.45/ns-3.45/2105114_final_submission/base_paper_results/table1_deviation.md):

- Average gamma on RTT increase:
  - traditional/standard: `35.1`
  - improved: `19.14`
- Average gamma on RTT decrease:
  - traditional/standard: `19.56`
  - improved: `23.09`
- Throughput values:
  - increase standard: `11.05 Mbps`
  - increase improved: `10.996 Mbps`
  - decrease standard: `11.088 Mbps`
  - decrease improved: `11.125 Mbps`

### 3.2 EL-RTO outcome summary

From [table1_deviation.md](/home/jarin/ns-allinone-3.45/ns-3.45/2105114_final_submission/base_paper_elrto_artifacts/table1_deviation.md):

- Average gamma on RTT increase scenario:
  - standard: `29.32`
  - improved: `32.32`
  - elrto: `32.32`
- Average gamma on RTT decrease scenario:
  - standard: `33.29`
  - improved: `31.39`
  - elrto: `31.39`

From [elrto_spike_summary.csv](/home/jarin/ns-allinone-3.45/ns-3.45/2105114_final_submission/base_paper_elrto_artifacts/elrto_spike_summary.csv):

- Standard peak RTO: `142 ms`
- Improved peak RTO: `127 ms`
- EL-RTO peak RTO: `127 ms`
- Standard average gamma: `19.073%`
- Improved average gamma: `17.410%`
- EL-RTO average gamma: `17.410%`

These results show that in the spike experiment the modified estimators reduced RTO inflation and reduced the average RTO-RTT mismatch.

## 4. EL-RTO and Simulator Modifications

The checklist experiments use the same algorithm selector in [rto-checklist-wireless.cc](/home/jarin/ns-allinone-3.45/ns-3.45/scratch/rto-checklist-wireless.cc). The key modifications are:

- `UseAdaptive` toggles the improved adaptive RTO estimator.
- `UseElRto` enables EL-RTO logic.
- `ElRtoWindow = 4`
- `ElRtoTheta = 2.0`

In practical terms, the project compares:

- Standard Jacobson-style RTO estimation
- Adaptive improved RTO estimation
- EL-RTO, which keeps the adaptive estimator and adds spike handling

Project workflow scripts added for the checklist runs:

- [run_selected_checklist.py](/home/jarin/ns-allinone-3.45/ns-3.45/run_selected_checklist.py)
- [run_lrwpan_mobile_10s.py](/home/jarin/ns-allinone-3.45/ns-3.45/run_lrwpan_mobile_10s.py)
- [run_wifi_mobile_10s.py](/home/jarin/ns-allinone-3.45/ns-3.45/run_wifi_mobile_10s.py)
- [plot_checklist_wireless.py](/home/jarin/ns-allinone-3.45/ns-3.45/plot_checklist_wireless.py)

These scripts were used to:

- resume incomplete experiment matrices safely
- isolate only the required mobile networks
- generate all required plots for throughput, delay, PDR, drop ratio, and energy

## 5. Checklist Experiment Setup

For the final checklist runs, the two required mobile networks were simulated and plotted from:

- [student2105114_mobile_checklist_combined_10s.csv](/home/jarin/ns-allinone-3.45/ns-3.45/2105114_final_submission/student2105114_mobile_checklist_combined_10s.csv)

Per-network result files:

- Wi-Fi mobile:
  - [student2105114_wifi_mobile_10s.csv](/home/jarin/ns-allinone-3.45/ns-3.45/2105114_final_submission/wifi_mobile/student2105114_wifi_mobile_10s.csv)
  - plots folder: [student2105114_wifi_mobile_10s_plots](/home/jarin/ns-allinone-3.45/ns-3.45/2105114_final_submission/wifi_mobile/student2105114_wifi_mobile_10s_plots)
- LR-WPAN mobile:
  - [student2105114_lrwpan_mobile_10s.csv](/home/jarin/ns-allinone-3.45/ns-3.45/2105114_final_submission/lrwpan_mobile/student2105114_lrwpan_mobile_10s.csv)
  - plots folder: [student2105114_lrwpan_mobile_10s_plots](/home/jarin/ns-allinone-3.45/ns-3.45/2105114_final_submission/lrwpan_mobile/student2105114_lrwpan_mobile_10s_plots)

Combined summary:

- [student2105114_mobile_summary_combined.csv](/home/jarin/ns-allinone-3.45/ns-3.45/2105114_final_submission/student2105114_mobile_summary_combined.csv)

## 6. Results with Graphs

The graphs required by the checklist were generated for each network and for each sweep:

- nodes vs metric
- flows vs metric
- packets-per-second vs metric
- speed vs metric

For each sweep, the following metrics were plotted:

- throughput
- end-to-end delay
- packet delivery ratio
- packet drop ratio
- energy consumption

### 6.1 Wireless 802.15.4 (mobile) summary

From [student_base_summary.csv](/home/jarin/ns-allinone-3.45/ns-3.45/2105114_final_submission/lrwpan_mobile/student2105114_lrwpan_mobile_10s_plots/student_base_summary.csv):

- Node sweep:
  - EL-RTO achieved the highest average throughput: `0.064215 Mbps`
  - EL-RTO also gave the lowest average delay: `630.646 ms`
- Flow sweep:
  - standard: `0.058195 Mbps`
  - improved: `0.053853 Mbps`
  - elrto: `0.055373 Mbps`
- Speed sweep:
  - EL-RTO gave slightly lower drop ratio than standard and improved in the average summary

Overall, LR-WPAN throughput is low compared to Wi-Fi, but the algorithm differences are visible in delay, PDR, and drop ratio trends.

### 6.2 Wireless 802.11 (mobile) summary

From [student_base_summary.csv](/home/jarin/ns-allinone-3.45/ns-3.45/2105114_final_submission/wifi_mobile/student2105114_wifi_mobile_10s_plots/student_base_summary.csv):

- Node sweep:
  - standard average throughput: `3.848239 Mbps`
  - improved average throughput: `3.797797 Mbps`
  - elrto average throughput: `3.797797 Mbps`
- Node sweep average delay:
  - standard: `73.936 ms`
  - improved: `71.562 ms`
  - elrto: `71.562 ms`
- Flow sweep:
  - improved/elrto slightly improved average delay over standard
  - improved/elrto also slightly reduced average drop ratio versus standard

Wi-Fi achieved much higher throughput than LR-WPAN, as expected, while still showing algorithm-dependent differences in delay and drop behavior.

## 7. Summary Findings

Main observations from the checklist experiments:

- Wi-Fi mobile delivers much higher throughput than LR-WPAN mobile.
- LR-WPAN is more sensitive to offered load and mobility, especially in delay and drop ratio.
- In the checklist runs, `improved` and `elrto` often behave very similarly.
- In Wi-Fi mobile, improved/EL-RTO tend to reduce delay a little compared with standard.
- In LR-WPAN mobile, EL-RTO sometimes provides better throughput/delay averages on the node sweep.
- Energy consumption scales strongly with node count in both mobile scenarios.

## 8. Reflection and Discussion

The checklist simulations show that transport-layer timer refinements can improve timing behavior without necessarily producing dramatic throughput gains in every scenario. This matches the earlier spike-suppression experiment, where the major improvement was reduced RTO overshoot and smaller RTO-RTT gap rather than a large throughput change.

Another important observation is that the wireless environment itself often dominates performance. In Wi-Fi mobile, contention and mobility strongly influence throughput and delay. In LR-WPAN mobile, the lower-capacity link makes delay and packet delivery ratio more sensitive to traffic load. Therefore, even when the RTO algorithm is improved, the benefit appears more clearly in timer stability and delay behavior than in raw throughput.

Finally, the mobile checklist does not require area-scale variation, so the experiments appropriately keep area scale fixed while varying the parameters requested for mobile cases only.

## 9. Final Submission Folder

The final packaged folder is:

- [2105114_final_submission](/home/jarin/ns-allinone-3.45/ns-3.45/2105114_final_submission)

It contains:

- per-network checklist CSVs
- per-network graph folders
- combined checklist CSV and summary
- earlier base-paper and EL-RTO artifacts
- this report
