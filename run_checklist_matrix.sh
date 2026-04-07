#!/usr/bin/env bash
set -euo pipefail

RESULT_CSV="${1:-checklist_wireless_results.csv}"

run_case() {
  local network="$1"
  local mode="$2"
  local extra="$3"
  ./ns3 run "rto-checklist-wireless --network=${network} --mode=${mode} --simTime=10 --outputCsv=${RESULT_CSV} ${extra}"
}

networks=("wifi-mobile" "lrwpan-mobile")
modes=("standard" "improved" "elrto")

for network in "${networks[@]}"; do
  for mode in "${modes[@]}"; do
    for nodes in 20 40 60 80 100; do
      run_case "${network}" "${mode}" "--nodes=${nodes} --flows=10 --pps=100 --speed=5"
    done

    for flows in 10 20 30 40 50; do
      run_case "${network}" "${mode}" "--nodes=100 --flows=${flows} --pps=100 --speed=5"
    done

    for pps in 100 200 300 400 500; do
      run_case "${network}" "${mode}" "--nodes=100 --flows=10 --pps=${pps} --speed=5"
    done

    for speed in 5 10 15 20 25; do
      run_case "${network}" "${mode}" "--nodes=100 --flows=10 --pps=100 --speed=${speed}"
    done
  done
done
