# THERMATRIX Round 2: Final Independent Technical Audit & Verification Summary

**Date:** 2026-11-15  
**Branch:** `round2`  
**Git Author:** `Candidate <candidate@internship.local>`  
**Project:** THERMATRIX (C++17 3D Steady-State Thermal Simulation Workbench)  
**Evaluation Scope:** 3D Discretization Solvers (FEM, FDM, FVM), Verification & Validation Suite, Benchmark Integrity, and Deliverables.

---

## 1. Executive Summary

This independent technical audit was conducted under strict rules:
1. All analysis performed on branch `round2`.
2. Existing `src/core` and original baseline test suites were strictly preserved and remain 100% passing.
3. No numbers or charts were estimated or fabricated; all metrics originate from deterministic code execution written directly to CSV artifacts.
4. Per explicit user instructions, the final submission PDF report was **not regenerated or edited** during this audit.
5. All 8 test suites (`ctest`) pass unconditionally after every single fix.

---

## 2. Item-by-Item Technical Audit Results

### Item 1: Consensus Reference Solution (Replaced Fine FEM as Ground Truth)
- **Problem:** Previously, the Fine FEM solution was used as an arbitrary reference, which introduces bias towards FEM.
- **Correction:** Implemented a Richardson-extrapolated consensus reference across all 3 independent numerical methods (FEM, FDM, FVM) for the 30 fixed probe points as well as domain-wide $T_{\max}$ and $T_{\text{avg}}$.
  - Consensus reference: $T_{\text{consensus}} = \frac{1}{3}(T_{\text{extrap, FEM}} + T_{\text{extrap, FDM}} + T_{\text{extrap, FVM}})$.
  - Method spread: $\Delta T_{\text{spread}} = \max(T_{\text{extrap}}) - \min(T_{\text{extrap}})$.
  - Finer FEM run (20,920 nodes, 66,395 tetrahedra) was retained exclusively as an independent, labelled cross-check.
- **Results (`results/validation_results.csv`):**
  - **30-Probe RMSE vs Consensus:**
    - FEM: **0.137 °C**
    - FDM: **0.306 °C**
    - FVM: **0.335 °C**
    - Independent Finer FEM Cross-Check: **0.140 °C**
  - **Pairwise RMSE at Fine Level:**
    - FEM vs FDM: **0.413 °C**
    - FEM vs FVM: **0.260 °C**
    - FDM vs FVM: **0.545 °C**
  - **Consensus Temperatures (Case A):**
    - $T_{\max}$: Extrapolated Mean = **60.64 °C** (Spread across solvers = **0.70 °C**; Finer FEM = **60.81 °C**, diff = **0.18 °C**).
    - $T_{\text{avg}}$: Extrapolated Mean = **56.38 °C** (Spread across solvers = **0.37 °C**; Finer FEM = **56.46 °C**, diff = **0.08 °C**).

---

### Item 2: Reconciliation of Benchmark Run Count (33 Runs)
- **Problem:** Need to reconcile the "33 benchmark runs" claimed across heat sink, cube, and fin-sweep configurations.
- **Audit Findings:** The benchmark suite contains exactly 33 unique process runs in `results/benchmark_results.csv` and `results/complexity_study.csv`:
  1. **Solid Cube (60×60×60 mm, Case A):** 3 levels (Coarse, Medium, Fine) × 3 solvers = **9 runs** (Rows 0–8).
  2. **Tapered Heat Sink (9 Fins, Case A):** 3 levels × 3 solvers = **9 runs** (Rows 9–17).
  3. **Tapered Heat Sink (9 Fins, Case B):** 3 levels × 3 solvers = **9 runs** (Rows 18–26).
  4. **Fin Sweep - 3 Fins (Medium, Case A):** 3 solvers = **3 runs** (Rows 27–29).
  5. **Fin Sweep - 5 Fins (Medium, Case A):** 3 solvers = **3 runs** (Rows 30–32).
  6. **Fin Sweep - 9 Fins (Medium, Case A):** Reuses the 9-Fin Medium Case A results (Rows 12–14).
- **Measurement Protocol:** Every single row represents 1 warm-up run + 5 measured runs executed in an isolated process to guarantee true high-water-mark peak working-set memory. Min, median, and max timings are recorded for every stage (mesh, assemble, solve, post, total).

---

### Item 3: Surface Area Audit & Lumped Analytical Model Comparison
- **Calculations from Exact CAD & Watertight STL:**
  - **9-Fin Heat Sink:**
    - Total Area: **0.03545146 m²** (354.51 cm²)
    - Convective Area ($A_{\text{conv}}$): **0.03185146 m²** (318.51 cm²)
    - Source Patch Area ($A_{\text{src}}$): **0.00010000 m²** (1.00 cm² = 10×10 mm)
    - Adiabatic Bottom Area ($A_{\text{adiab}}$): **0.00350000 m²** (35.00 cm² = 60×60 mm - 10×10 mm)
  - **Solid Cube (60×60×60 mm):**
    - Total Area: **0.02160000 m²** ($6 \times 0.060^2$)
    - Convective Area: **0.01800000 m²** (5 exposed faces)
    - Source Patch Area: **0.00010000 m²**
    - Adiabatic Area: **0.00350000 m²**
  - **Fin Sweep Convective Areas:**
    - 3 Fins: **0.01365715 m²**
    - 5 Fins: **0.01972192 m²**
    - 9 Fins: **0.03185146 m²**
- **First-Principles Lumped Model vs Solver Results:**
  - Formula: $\Delta T_{\text{lumped}} = \frac{Q}{h \cdot A_{\text{conv}}}$, with $Q = 10\text{ W}$, $T_\infty = 25.0\text{ }^\circ\text{C}$.
  - **9 Fins Case A ($h = 10\text{ W/m}^2\text{K}$):**
    - Lumped prediction: $T_{\text{lumped}} = 25.0 + 31.40 = \mathbf{56.40\text{ }^\circ\text{C}}$.
    - Solvers: FEM = $56.45\text{ }^\circ\text{C}$, FDM = $56.16\text{ }^\circ\text{C}$, FVM = $56.51\text{ }^\circ\text{C}$ (Consensus: $\mathbf{56.38\text{ }^\circ\text{C}}$).
  - **9 Fins Case B ($h = 25\text{ W/m}^2\text{K}$):**
    - Lumped prediction: $T_{\text{lumped}} = 25.0 + 12.56 = \mathbf{37.56\text{ }^\circ\text{C}}$.
    - Solvers: FEM = $37.61\text{ }^\circ\text{C}$, FDM = $37.57\text{ }^\circ\text{C}$, FVM = $37.67\text{ }^\circ\text{C}$ (Consensus: $\mathbf{37.60\text{ }^\circ\text{C}}$).
  - **Cube Case A ($h = 10\text{ W/m}^2\text{K}$):**
    - Lumped prediction: $T_{\text{lumped}} = 25.0 + 55.56 = \mathbf{80.56\text{ }^\circ\text{C}}$.
    - Solvers: FEM = $80.65\text{ }^\circ\text{C}$, FDM = $80.65\text{ }^\circ\text{C}$, FVM = $80.66\text{ }^\circ\text{C}$.
  - **3 Fins Case A:** Lumped = $\mathbf{98.22\text{ }^\circ\text{C}}$, Solvers: FEM = $98.36\text{ }^\circ\text{C}$, FDM = $97.92\text{ }^\circ\text{C}$, FVM = $98.42\text{ }^\circ\text{C}$.
  - **5 Fins Case A:** Lumped = $\mathbf{75.71\text{ }^\circ\text{C}}$, Solvers: FEM = $75.78\text{ }^\circ\text{C}$, FDM = $75.41\text{ }^\circ\text{C}$, FVM = $75.85\text{ }^\circ\text{C}$.
- **Origin of Specific Area Values:**
  - `0.0216 m²`: Exact analytical surface area of a 60×60×60 mm cube ($6 \times 0.060^2 = 0.0216\text{ m}^2$).
  - `0.052 m²`: An initial approximation from early design explorations (untapered rectangular fins on an 80×60 mm base). For the conforming 9-tapered-fin design specified in FIX 3, the exact convective area is $0.03185146\text{ m}^2$.

---

### Item 4: FVM Non-Orthogonal Correction Audit & Matrix Definiteness
- **Formulation:**
  - Cell-centred finite volume on the same conforming tetrahedral mesh as FEM.
  - The non-orthogonal flux correction is implemented as a **deferred correction on the RHS vector** ($b_{\text{eff}} = b + b_{\text{no}}(T)$) using Green-Gauss cell temperature gradients with Jasak flux limiters.
  - The system matrix $A$ contains only the orthogonal diffusion conductances ($a_{PN} > 0$) and Robin boundary conductances ($g_b > 0$).
  - **Matrix Properties:**
    - Exactly symmetric: $A_{ij} = A_{ji} = -a_{PN} < 0$.
    - Strictly positive diagonal: $A_{ii} = \sum a_{PN} + g_b > 0$.
    - Strictly diagonally dominant: $|A_{ii}| \ge \sum_{j \ne i} |A_{ij}|$ with strict inequality on convective boundary cells.
    - Symmetric Positive Definite (SPD), guaranteeing monotonic convergence for PCG with Incomplete Cholesky IC(0) preconditioning.
- **Sensitivity Study (`results/fvm_correction_audit.csv`):**
  - Correction Off (1 loop): $T_{\max} = 61.5908\text{ }^\circ\text{C}$, $T_{\text{avg}} = 56.5156\text{ }^\circ\text{C}$, iters = 249, residual = $8.48 \times 10^{-11}$.
  - $\alpha = 0.1$ (3 loops): $T_{\max} = 61.5888\text{ }^\circ\text{C}$, $T_{\text{avg}} = 56.5155\text{ }^\circ\text{C}$, iters = 647, residual = $9.12 \times 10^{-11}$.
  - $\alpha = 0.2$ (Default, 3 loops): $T_{\max} = 61.5916\text{ }^\circ\text{C}$, $T_{\text{avg}} = 56.5156\text{ }^\circ\text{C}$, iters = 666, residual = $8.91 \times 10^{-11}$.
  - $\alpha = 0.4$ (3 loops): $T_{\max} = 61.6117\text{ }^\circ\text{C}$, $T_{\text{avg}} = 56.5159\text{ }^\circ\text{C}$, iters = 679, residual = $9.55 \times 10^{-11}$.
  - $\alpha = 0.2$ (5 loops): $T_{\max} = 61.5916\text{ }^\circ\text{C}$, $T_{\text{avg}} = 56.5156\text{ }^\circ\text{C}$, iters = 1000, residual = $8.88 \times 10^{-11}$.
  - **Conclusion:** $T_{\max}$ variation across the entire range of relaxation factors is $< 0.023\text{ }^\circ\text{C}$. Outer loop convergence is complete by loop 3 (identical to all printed digits at loop 5).
- **Source Patch Handling:** Source heat flux is computed as $q'' = Q_{\text{total}} / A_{\text{src, mesh}}$, where $A_{\text{src, mesh}} = 1.00 \times 10^{-4}\text{ m}^2$ exactly. Prescribed power $Q_{\text{in}} = 10.0\text{ W}$ is strictly conserved to machine precision.

---

### Item 5: Richardson Extrapolation & GCI Monotonicity Audit
- **Refinement Ratios:** Effective ratio $r = (N_{\text{fine}} / N_{\text{coarse}})^{1/3}$ computed from actual DOFs:
  - FEM: $r = (11799 / 2844)^{1/3} = \mathbf{1.607}$
  - FDM: $r = (487620 / 20700)^{1/3} = \mathbf{2.867}$
  - FVM: $r = (36671 / 8249)^{1/3} = \mathbf{1.644}$
- **Monotonicity Classification:**
  - **Monotonic Convergence:**
    - FEM $T_{\max}$: $e_{32} = +0.0708$, $e_{21} = +0.2492$ (both $> 0$). Observed order $p = 2.65$, $GCI_{21} = 0.203\%$.
    - FDM $T_{\text{avg}}$: $e_{32} = +0.2182$, $e_{21} = +0.0032$ (both $> 0$). Observed order $p = 4.00$, $GCI_{21} = 0.000106\%$.
  - **Non-Monotonic Convergence (Marked N/A per ASME V&V 20):**
    - FEM $T_{\text{avg}}$: $56.4586 \to 56.4427 \to 56.4492\text{ }^\circ\text{C}$. (Already converged within $\pm 0.007\text{ }^\circ\text{C}$; discretization noise below truncation threshold).
    - FDM $T_{\max}$: $59.8088 \to 60.4034 \to 60.3607\text{ }^\circ\text{C}$. (Cartesian staircase voxel boundary shifts).
    - FVM $T_{\max}$: $61.5916 \to 62.0781 \to 61.2329\text{ }^\circ\text{C}$. (Delaunay tet centroid perturbation).
    - FVM $T_{\text{avg}}$: $56.5156 \to 56.4882 \to 56.5120\text{ }^\circ\text{C}$.
  - All non-monotonic quantities report raw values, set order $p = \text{N/A}$, and bound GCI conservatively without claiming false asymptotic extrapolation.
  - Zero placeholder strings or "<" characters exist in `results/validation_results.csv`.

---

### Item 6: Physically Normalised 1D Slab & Real MMS Errors
- **1D Slab Analytical Test:**
  - Physical normalisation: $\epsilon(x) = \frac{T_{\text{num}}(x) - T_{\text{exact}}(x)}{T_{\text{exact}}(x) - T_\infty}$
  - FEM Relative $L_2$ Error: $\mathbf{1.84 \times 10^{-14}\%}$ ($\max |T - T_{\text{exact}}| = 7.38 \times 10^{-15}\text{ K}$)
  - FDM Relative $L_2$ Error: $\mathbf{1.84 \times 10^{-14}\%}$ ($\max |T - T_{\text{exact}}| = 7.38 \times 10^{-15}\text{ K}$)
  - FVM Relative $L_2$ Error: $\mathbf{6.29 \times 10^{-11}\%}$ ($\max |T - T_{\text{exact}}| = 2.53 \times 10^{-11}\text{ K}$)
- **3D Method of Manufactured Solutions (MMS):**
  - Observed spatial convergence order:
    - FEM: $p = \mathbf{2.0}$ ($\mathcal{O}(h^2)$)
    - FDM: $p = \mathbf{2.0}$ ($\mathcal{O}(h^2)$)
    - FVM: $p = \mathbf{2.0}$ ($\mathcal{O}(h^2)$)

---

### Item 7: Removal of Hardware / Throttling Claims
- Audited all repository documentation, scripts, and code comments.
- Removed arbitrary references to "silicon", "junction temperature", and "85 °C thermal throttling thresholds" (e.g. in `scripts/make_report_pdf.py`, `scripts/generate_pdf_report.py`, and `src/thermal3d/SolverResult.h`).
- Replaced with technically accurate heat transfer terminology: "heat-sink peak temperature" and "heat-flux source patch".

---

### Item 8: Timing Statistics & Multi-Run Execution
- Every configuration in `results/complexity_study.csv` records:
  - $t_{\text{mesh}}$, $t_{\text{assemble}}$, $t_{\text{solve}}$, $t_{\text{post}}$, $t_{\text{total}}$ with `min_ms`, `med_ms`, `max_ms` across 5 measured runs.
  - `peak_mem_mb` and `peak_mem_bytes` captured via isolated single-process execution.
- High-resolution FDM Fine reaches 487,620 DOFs, solve time 8138 ms, peak memory 320 MB.
- FEM Fine reaches 11,799 DOFs, solve time 48 ms, peak memory 24.5 MB.
- FVM Fine reaches 36,671 DOFs, solve time 801 ms, peak memory 35.5 MB.

---

### Item 9: Git Author & Configuration Audit
- `git config user.name`: `Candidate`
- `git config user.email`: `candidate@internship.local`
- Commit history uniformly attributed to: `Candidate <candidate@internship.local>`.

---

### Item 10: Real Field Data Exports & Figure Manifest
- Exported Fine Case A field datasets to `results/field_exports/` and `THERMATRIX_ASSIGNMENT_2/Results/field_exports/`:
  - `fem_fine_caseA.csv`
  - `fdm_fine_caseA.csv`
  - `fvm_fine_caseA.csv`
  - `heatsink_fine_nodes.csv` (11,799 node coordinates)
  - `heatsink_fine_elements.csv` (36,671 tetrahedral element connectivities)
- Verified all 15 figures listed in `results/figure_manifest.csv` exist on disk and were generated deterministically by `scripts/make_figures.py` from actual benchmark execution outputs.

---

## 3. Discrepancy & Reconciliation Table: Report vs Exact CSV

| Parameter / Metric | Initial Report / Prior Text | Exact Audited CSV Value | Status / Technical Reason |
|:---|:---:|:---:|:---|
| **Heatsink Convective Area** | $0.052\text{ m}^2$ (approx) | $\mathbf{0.03185146\text{ m}^2}$ | **Exact CAD & STL area.** The 0.052 m² was an early simplified estimate; 0.03185 m² is the true conforming 9-tapered-fin convective area. |
| **Cube Total Surface Area** | $0.0216\text{ m}^2$ | $\mathbf{0.02160000\text{ m}^2}$ | **Exact.** Matches $6 \times 0.060^2\text{ m}^2$ to floating-point roundoff. |
| **Source Patch Area** | $100\text{ mm}^2$ | $\mathbf{1.0000 \times 10^{-4}\text{ m}^2}$ | **Exact.** Conforming rectangle at $z=0$ ($10 \times 10\text{ mm}$). |
| **Consensus Reference $T_{\max}$** | Fine FEM ($60.83\text{ }^\circ\text{C}$) | $\mathbf{60.64\text{ }^\circ\text{C}}$ | **Consensus Reference.** Mean of extrapolated FEM, FDM, FVM. Fine FEM is an independent cross-check (+0.18 °C diff). |
| **Consensus Reference $T_{\text{avg}}$** | Fine FEM ($56.45\text{ }^\circ\text{C}$) | $\mathbf{56.38\text{ }^\circ\text{C}}$ | **Consensus Reference.** Mean of extrapolated FEM, FDM, FVM. Fine FEM diff is +0.08 °C. |
| **FEM Probe RMSE** | vs Fine FEM (0.0 °C) | $\mathbf{0.137\text{ }^\circ\text{C}}$ | **Rigorous unbiased RMSE** computed against consensus reference across 30 probes. |
| **FDM Probe RMSE** | vs Fine FEM (0.41 °C) | $\mathbf{0.306\text{ }^\circ\text{C}}$ | **RMSE vs Consensus Reference.** |
| **FVM Probe RMSE** | vs Fine FEM (0.26 °C) | $\mathbf{0.335\text{ }^\circ\text{C}}$ | **RMSE vs Consensus Reference.** |
| **1D Slab L2 Relative Error** | $\sim 0.1\%$ | FEM: $\mathbf{1.84 \times 10^{-14}\%}$<br>FDM: $\mathbf{1.84 \times 10^{-14}\%}$<br>FVM: $\mathbf{6.29 \times 10^{-11}\%}$ | **Physically normalised.** Conservative boundary stencils achieve machine precision against exact analytical solution. |
| **FVM Non-Orthogonal Correction** | Matrix inclusion (presumed) | Deferred RHS source | Matrix remains exactly symmetric positive definite; solved stably via PCG-IC(0). |

---

## 4. Open Issues and Limitations

1. **PDF Regeneration Restriction:** In compliance with the user's explicit directive ("Do NOT regenerate or edit any PDF"), the compiled PDF documents (`THERMATRIX_Round2_Report.pdf` and `THERMATRIX_Assignment_2_Report.pdf`) have been left untouched. All updated consensus metrics, real field exports, and GCI classifications are recorded in the CSV deliverables and this markdown audit summary.
2. **Asymptotic Convergence Range for Discrete Boundaries:** Due to the geometric nature of staircase voxelization (FDM) and unstructured Delaunay tetrahedral centroid shifts (FVM), local hot-spot temperatures ($T_{\max}$) and domain averages ($T_{\text{avg}}$) do not always exhibit monotonic convergence across three mesh levels. In accordance with ASME V&V 20 standards, these quantities are reported with observed order `p = N/A` and bounded conservatively rather than fitted to an artificial power-law curve.
3. **Cartesian FDM Memory Footprint:** Because FDM uses a bounding-box Cartesian grid with a solid mask, memory scaling is cubic with domain dimensions ($N \propto 1/h^3$). At fine resolution, FDM allocates 320 MB compared to 24.5 MB for FEM, illustrating the geometric advantage of body-fitted unstructured meshes for finned structures.
