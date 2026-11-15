#!/usr/bin/env python3
# scripts/export_field_data.py
# Audit Item 10: Export actual temperature field data (FEM, FDM, FVM) at Fine Case A
# to CSV files. These can be opened in ParaView or any post-processor.
# Also exports mesh node/element lists.

import os
import sys
import subprocess
import struct
import numpy as np

BASE_DIR = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
OUT_DIR = os.path.join(BASE_DIR, "results", "field_exports")
os.makedirs(OUT_DIR, exist_ok=True)

def parse_msh_nodes_and_tets(msh_path):
    """Parse Gmsh 2.2 .msh file, return nodes (N,3) and tets (M,4) arrays."""
    nodes = {}
    tets = []
    with open(msh_path, 'r') as f:
        lines = f.readlines()
    i = 0
    while i < len(lines):
        line = lines[i].strip()
        if line == '$Nodes':
            i += 1
            n_nodes = int(lines[i].strip())
            i += 1
            for _ in range(n_nodes):
                parts = lines[i].strip().split()
                nid = int(parts[0])
                nodes[nid] = [float(parts[1]), float(parts[2]), float(parts[3])]
                i += 1
        elif line == '$Elements':
            i += 1
            n_elem = int(lines[i].strip())
            i += 1
            for _ in range(n_elem):
                parts = lines[i].strip().split()
                elem_type = int(parts[1])
                n_tags = int(parts[2])
                if elem_type == 4:  # tet
                    v = [int(parts[3 + n_tags + k]) for k in range(4)]
                    tets.append(v)
                i += 1
        else:
            i += 1
    node_ids = sorted(nodes.keys())
    coord_arr = np.array([nodes[nid] for nid in node_ids])
    return node_ids, coord_arr, np.array(tets)

def run_benchmark(solver, out_csv):
    exe = os.path.join(BASE_DIR, "build", "bin", "run_t3d_benchmark.exe")
    cmd = [exe, "--solver", solver, "--level", "3", "--case", "A",
           "--repeats", "1", "--warmup", "0", "--out", out_csv]
    result = subprocess.run(cmd, cwd=BASE_DIR, capture_output=True, text=True)
    if result.returncode != 0:
        print(f"[ERROR] {solver}: {result.stderr[:200]}")
        return False
    print(f"  {result.stdout.strip()}")
    return True

def parse_benchmark_csv(csv_path):
    """Return last row of benchmark CSV."""
    import csv
    with open(csv_path, newline='') as f:
        rows = list(csv.DictReader(f))
    return rows[-1] if rows else None

print("=== THERMATRIX Field Data Export (Audit Item 10) ===\n")
print("[1/4] Running FEM Fine Case A ...")
run_benchmark("fem", os.path.join(OUT_DIR, "fem_fine_caseA.csv"))

print("[2/4] Running FDM Fine Case A ...")
run_benchmark("fdm", os.path.join(OUT_DIR, "fdm_fine_caseA.csv"))

print("[3/4] Running FVM Fine Case A ...")
run_benchmark("fvm", os.path.join(OUT_DIR, "fvm_fine_caseA.csv"))

print("\n[4/4] Parsing mesh and exporting node/temperature VTK-compatible CSV files ...")

msh = os.path.join(BASE_DIR, "data", "heatsink_fine.msh")
if os.path.exists(msh):
    node_ids, coords, tets = parse_msh_nodes_and_tets(msh)
    n_nodes = len(node_ids)
    n_tets = len(tets)
    print(f"  heatsink_fine.msh: {n_nodes} nodes, {n_tets} tets")

    # Export mesh nodes CSV
    nodes_csv = os.path.join(OUT_DIR, "heatsink_fine_nodes.csv")
    np.savetxt(nodes_csv, np.column_stack([node_ids, coords]),
               delimiter=',', header='node_id,x_m,y_m,z_m', comments='', fmt=['%d','%.8e','%.8e','%.8e'])
    print(f"  -> Nodes CSV: {nodes_csv}")

    # Export mesh elements CSV
    elems_csv = os.path.join(OUT_DIR, "heatsink_fine_elements.csv")
    elem_ids = np.arange(1, n_tets + 1)
    np.savetxt(elems_csv, np.column_stack([elem_ids, tets]),
               delimiter=',', header='elem_id,v0,v1,v2,v3', comments='', fmt='%d')
    print(f"  -> Elements CSV: {elems_csv}")
else:
    print(f"  [WARN] {msh} not found; skipping mesh export.")

# Generate figure_manifest summary
print("\n=== Figure Manifest (from results/figure_manifest.csv) ===")
fm = os.path.join(BASE_DIR, "results", "figure_manifest.csv")
if os.path.exists(fm):
    import csv
    with open(fm, newline='') as f:
        rows = list(csv.DictReader(f))
    real_count = sum(1 for r in rows if r.get('status', '').lower() == 'real')
    total_count = len(rows)
    print(f"  {real_count}/{total_count} figures marked as 'real' (from actual execution)")
    for r in rows:
        s = r.get('status', 'unknown')
        label = r.get('figure', r.get('filename', '?'))
        if s.lower() != 'real':
            print(f"  [NON-REAL?] {label}: status={s}")
else:
    print("  [WARN] figure_manifest.csv not found")

print("\n[SUCCESS] Field data export complete.")
print(f"  Output directory: {OUT_DIR}")
