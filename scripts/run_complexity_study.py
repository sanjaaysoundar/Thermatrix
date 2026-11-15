#!/usr/bin/env python3
# scripts/run_complexity_study.py
# Implements FIX 5: Geometric Complexity Study across Cube vs Heat Sink and Fin Sweep (3, 5, 9 Fins).
# Runs each solver, level, and geometry in an isolated process to ensure true peak memory and timings.

import os
import sys
import subprocess
import pandas as pd

BASE_DIR = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
BIN_DIR = os.path.join(BASE_DIR, "build", "bin")
EXE = os.path.join(BIN_DIR, "run_t3d_benchmark.exe")
if not os.path.exists(EXE):
    EXE = os.path.join(BASE_DIR, "bin", "run_t3d_benchmark.exe")

RESULTS_DIR = os.path.join(BASE_DIR, "results")
FINAL_RESULTS_DIR = os.path.join(BASE_DIR, "THERMATRIX_ASSIGNMENT_2", "Results")
os.makedirs(RESULTS_DIR, exist_ok=True)
os.makedirs(FINAL_RESULTS_DIR, exist_ok=True)

OUT_CSV = os.path.join(RESULTS_DIR, "complexity_study.csv")
if os.path.exists(OUT_CSV):
    os.remove(OUT_CSV)

def run_isolated(solver, level, case_lbl, geom, fins, out_file):
    cmd = [
        EXE,
        "--solver", solver,
        "--level", str(level),
        "--case", case_lbl,
        "--repeats", "5",
        "--warmup", "1",
        "--linear_solver", "pcg-ic0",
        "--geometry", geom,
        "--fins", str(fins),
        "--out", out_file
    ]
    res = subprocess.run(cmd, cwd=BASE_DIR, capture_output=True, text=True)
    if res.returncode != 0:
        print(f"Error running: {' '.join(cmd)}\n{res.stderr}")
        return False
    # Print the stdout progress line
    lines = [l for l in res.stdout.strip().split("\n") if "[BENCHMARK]" in l]
    if lines:
        print(lines[-1])
    return True

def main():
    print("==========================================================================")
    print(" THERMATRIX Geometric Complexity Study (FIX 5)")
    print(" Comparing FEM, FDM, and FVM across:")
    print("   1. 60x60x60 mm Solid Cube (Coarse, Medium, Fine)")
    print("   2. 60x60x29 mm Tapered Heat Sink (Coarse, Medium, Fine)")
    print("   3. Fin Count Sweep (3 Fins, 5 Fins, 9 Fins)")
    print("==========================================================================\n")

    solvers = ["fem", "fdm", "fvm"]

    # 1. Solid Cube Study (Coarse, Medium, Fine for Case A)
    print("[PHASE 1] Solid Cube (60x60x60 mm) Resolution Scaling:")
    for lvl in [1, 2, 3]:
        for s in solvers:
            run_isolated(s, lvl, "A", "cube", 0, OUT_CSV)

    # 2. Heat Sink Study (Coarse, Medium, Fine for Case A and Case B)
    print("\n[PHASE 2] Heat Sink (9 Tapered Fins) Resolution Scaling:")
    for c in ["A", "B"]:
        for lvl in [1, 2, 3]:
            for s in solvers:
                run_isolated(s, lvl, c, "heatsink", 9, OUT_CSV)

    # 3. Fin Count Sweep (3, 5, 9 Fins at Medium Resolution, Case A)
    print("\n[PHASE 3] Fin Count Complexity Sweep (3, 5, 9 Fins):")
    for f in [3, 5]:
        for s in solvers:
            run_isolated(s, 2, "A", "heatsink", f, OUT_CSV)

    # Copy to delivery folder
    final_csv = os.path.join(FINAL_RESULTS_DIR, "complexity_study.csv")
    import shutil
    shutil.copyfile(OUT_CSV, final_csv)
    # Also copy to benchmark_results.csv if needed
    shutil.copyfile(OUT_CSV, os.path.join(RESULTS_DIR, "benchmark_results.csv"))
    shutil.copyfile(OUT_CSV, os.path.join(FINAL_RESULTS_DIR, "benchmark_results.csv"))

    print("\n==========================================================================")
    print(f"[SUCCESS] Complexity study completed. Results written to:\n  {OUT_CSV}\n  {final_csv}")
    print("==========================================================================")

    # Generate summary analysis table
    df = pd.read_csv(OUT_CSV)
    print("\n--- Summary: Cube vs 9-Fin Heat Sink Comparison (Fine Level, Case A) ---")
    sub = df[(df['level'].isin(['Cube_Fine', '9Fins_Fine'])) & (df['case'] == 'A')]
    cols = ['solver', 'level', 'n_dof', 't_total_ms', 'peak_mem_mb', 'T_min_C', 'T_max_C', 'T_avg_C']
    print(sub[cols].to_string(index=False))

    print("\n--- Summary: Fin Count Sweep (Medium Level, Case A) ---")
    sub_fins = df[(df['level'].isin(['3Fins_Medium', '5Fins_Medium', '9Fins_Medium'])) & (df['case'] == 'A')]
    print(sub_fins[cols].to_string(index=False))

if __name__ == "__main__":
    main()
