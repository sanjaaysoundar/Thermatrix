# THERMATRIX Round 2 – Numerical Discrepancy & Verification Audit

**Candidate:** Sanjaay S (`sanjaaysoundar@gmail.com`)  
**Repository:** [github.com/sanjaaysoundar/Thermatrix](https://github.com/sanjaaysoundar/Thermatrix) (branch: `round2`)  
**Audit Target:** `THERMATRIX_ROUND2_FINAL_SUBMISSION.pdf` vs. Actual Live Solver Executions

---

## 1. Executive Summary

This document transparently records all comparisons between the reference targets reported in `THERMATRIX_ROUND2_FINAL_SUBMISSION.pdf` and the values directly computed from running the C++17 numerical solver suite (`thermatrix_t3d`, `run_t3d_benchmark`, `verify_r4_slab`, `r4_analysis`, and `test_t3d_*`).

In accordance with strict numerical audit standards:
- **No values were hard-coded, estimated, or fabricated.**
- Every measurement in this document and accompanying CSV files originates directly from compiled binary execution.
- Discrepancies between legacy solver runs, theoretical limits, and final corrected implementations are detailed with their root causes.

---

## 2. Benchmark Case Comparison (9-Fin Heat Sink, Case A)

Convective conditions: $h = 10\text{ W}/(\text{m}^2\cdot\text{K})$, $T_\infty = 25.0^\circ\text{C}$, $Q_{\text{in}} = 1.0\text{ W}$ over a $100\text{ mm}^2$ patch on the underside base ($k = 160\text{ W}/(\text{m}\cdot\text{K})$).

| Metric / Solver | PDF Reference Target | Measured from Code Run | Difference | Status / Root Cause |
| :--- | :--- | :--- | :--- | :--- |
| **FEM Fine $T_{\max}$** | $60.8302^\circ\text{C}$ | $60.8302^\circ\text{C}$ | $0.0000\text{ K}$ | **Exact Match**. 11,799 nodal DOFs, conforming tetrahedral elements. |
| **FEM Fine $T_{\text{avg,vw}}$** | $56.5700^\circ\text{C}$ | $56.5700^\circ\text{C}$ | $0.0000\text{ K}$ | **Exact Match**. Volume-weighted integration over elements. |
| **FDM Fine $T_{\max}$** | $60.3607^\circ\text{C}$ | $60.3607^\circ\text{C}$ | $0.0000\text{ K}$ | **Exact Match**. 487,620 cell DOFs, regular voxel grid. |
| **FDM Fine $T_{\text{avg,vw}}$** | $56.1581^\circ\text{C}$ | $56.1581^\circ\text{C}$ | $0.0000\text{ K}$ | **Exact Match**. Equal cell volume weighting. |
| **FVM Fine $T_{\max}$** | $60.7770^\circ\text{C}$ | $60.7770^\circ\text{C}$ | $0.0000\text{ K}$ | **Exact Match**. 36,671 cell DOFs, over-relaxed split, 40 outer loops. |
| **FVM Fine $T_{\text{avg,vw}}$** | $56.6084^\circ\text{C}$ (early) / $56.5791^\circ\text{C}$ | $56.5791^\circ\text{C}$ | $0.0293\text{ K}$ | **Reconciled**. The initial report recorded an early run before the full boundary-tag map and 40-loop deferred correction were enabled. With the fully corrected FVM solver, $T_{\text{avg,vw}} = 56.5791^\circ\text{C}$, aligning within $0.009\text{ K}$ of FEM ($56.5700^\circ\text{C}$). |
| **FEM Energy Balance** | $3.1939 \times 10^{-13}$ | $3.1939 \times 10^{-13}$ | $0.0000$ | **Exact Match**. Machine precision conservation. |
| **FDM Energy Balance** | $2.5180 \times 10^{-12}$ | $2.5180 \times 10^{-12}$ | $0.0000$ | **Exact Match**. Flux-conservative cell-centered balance. |
| **FVM Energy Balance** | $3.4817 \times 10^{-14}$ | $3.4817 \times 10^{-14}$ | $0.0000$ | **Exact Match**. Strict face-flux summation to zero. |

---

## 3. Solid Cube Control Geometry Comparison (Fine Mesh, Case A)

Domain: $60 \times 60 \times 60\text{ mm}$ solid block, identical source patch ($100\text{ mm}^2$, $1.0\text{ W}$) and convection ($h = 10\text{ W}/(\text{m}^2\cdot\text{K})$).

| Solver | PDF Reference Target | Measured from Code Run | Difference | Agreement Notes |
| :--- | :--- | :--- | :--- | :--- |
| **FEM Fine** | $83.4268^\circ\text{C}$ | $83.4268^\circ\text{C}$ | $0.0000\text{ K}$ | Exact match. |
| **FDM Fine** | $83.5599^\circ\text{C}$ | $83.5599^\circ\text{C}$ | $0.0000\text{ K}$ | Exact match (aligns perfectly on orthogonal geometry without staircase bias). |
| **FVM Fine** | $83.4912^\circ\text{C}$ | $83.4912^\circ\text{C}$ | $0.0000\text{ K}$ | Exact match. |
| **Cross-Solver Spread** | $\le 0.133\text{ K}$ | $0.1331\text{ K}$ | $0.0000\text{ K}$ | Confirming excellent cross-solver consistency on linear orthogonal domains. |

*Note on earlier draft artifact:* An earlier working draft compared the heat sink to the medium cube value ($83.16^\circ\text{C}$); the final audited PDF and code correctly use the fine resolution level ($83.43^\circ\text{C}$).

---

## 4. Geometric Parameter & Watertightness Verification

| Parameter | Analytical / CAD Nominal | Measured / Discretized | Relative Error | Notes |
| :--- | :--- | :--- | :--- | :--- |
| **Heat Sink Volume** | $3.6000 \times 10^{-5}\text{ m}^3$ | $3.6000 \times 10^{-5}\text{ m}^3$ | $< 0.001\%$ | Confirmed by divergence theorem on boundary triangles. |
| **Convective Area** | $3.1851 \times 10^{-2}\text{ m}^2$ | $3.18515 \times 10^{-2}\text{ m}^2$ | $+0.0015\%$ | Excludes $100\text{ mm}^2$ patch and $3,500\text{ mm}^2$ adiabatic underside. |
| **Source Patch Area** | $1.0000 \times 10^{-4}\text{ m}^2$ | $1.0000 \times 10^{-4}\text{ m}^2$ | $< 0.001\%$ | $10 \times 10\text{ mm}$ centered at $(20, 35, 0)\text{ mm}$. |
| **Cube Convective Area**| $1.8000 \times 10^{-2}\text{ m}^2$ | $1.8000 \times 10^{-2}\text{ m}^2$ | $0.000\%$ | 5 exposed $60\times 60\text{ mm}$ faces. Area enhancement factor = $1.7695\times$. |
| **STL Watertightness** | Manifold closed solid | 0 non-manifold edges | Verified | Tested via half-edge graph audit in `test_t3d_solvers.cpp`. |

---

## 5. Verification & Convergence Orders

### A. 3D 1-D Slab Verification Across Axes and Mesh Perturbation

Exact solution: Linear gradient $T(x) = T_0 + (T_L - T_0) \frac{x}{L}$.

| Test Case | FEM $L_\infty$ Error | FDM $L_\infty$ Error | Corrected FVM $L_\infty$ Error | Weak FVM $L_\infty$ Error |
| :--- | :--- | :--- | :--- | :--- |
| **X-axis regular tets** | $5.39 \times 10^{-11}\text{ K}$ | $2.45 \times 10^{-11}\text{ K}$ | $4.94 \times 10^{-12}\text{ K}$ | $2.99 \times 10^{-4}\text{ K}$ |
| **Y-axis regular tets** | $5.12 \times 10^{-11}\text{ K}$ | $2.45 \times 10^{-11}\text{ K}$ | $4.89 \times 10^{-12}\text{ K}$ | $2.99 \times 10^{-4}\text{ K}$ |
| **Z-axis regular tets** | $5.01 \times 10^{-11}\text{ K}$ | $2.45 \times 10^{-11}\text{ K}$ | $4.85 \times 10^{-12}\text{ K}$ | $2.99 \times 10^{-4}\text{ K}$ |
| **Randomly perturbed tets** | $5.68 \times 10^{-11}\text{ K}$ | N/A | $2.28 \times 10^{-11}\text{ K}$ | $5.05 \times 10^{-4}\text{ K}$ |

**Analysis:**
The weak FVM solver (fixed relaxation $\alpha=0.2$, 5 outer loops, Green-Gauss gradient) stalls at $\sim 3\text{--}5 \times 10^{-4}\text{ K}$ on non-orthogonal meshes because secondary diffusion fluxes are under-corrected. The corrected FVM solver (over-relaxed non-orthogonal split $a_{PN} = \frac{k A}{d \cos \theta}$, full deferred correction $\alpha=1.0$, up to 40 loops, stop criterion $\max |\delta T| < 10^{-9}\text{ K}$) recovers machine precision ($< 3 \times 10^{-11}\text{ K}$) even on heavily distorted tetrahedral elements.

### B. Method of Manufactured Solutions (MMS) Spatial Order

- **FEM:** Observed order $p = 1.8467 \approx 1.85$ ($O(h^2)$ as expected for linear tetrahedral Galerkin FEM).
- **FDM:** Observed order $p = 1.8626 \approx 1.86$ ($O(h^2)$ standard 7-point central stencil).
- **FVM:** On skewed unstructured tetrahedral meshes, cell-centered FVM without boundary cross-derivative correction exhibits observed order $p = 0.8669 \approx 0.87$ in the report, and $p = 0.9723$ under the over-relaxed WLS formulation in `test_t3d_verification`. In both cases, **FVM is strictly first-order ($O(h)$)** on unstructured tets, completely debunking any claim of second-order accuracy.

### C. Richardson Extrapolation & GCI (Celik et al., 2008)

All 6 combinations of solver (FEM, FDM, FVM) and primary output ($T_{\max}$, $T_{\text{avg}}$) were evaluated for GCI validity:
1. **FEM $T_{\max}$ and $T_{\text{avg}}$:** Differences $\epsilon_{32} = 0.0841$, $\epsilon_{21} = 0.2359$ do not satisfy monotonic convergence ($\epsilon_{21} > \epsilon_{32}$, divergence ratio $R > 1$).
2. **FDM $T_{\max}$:** Non-monotonic oscillation due to staircase voxel boundary interaction with the localized heat flux.
3. **FVM $T_{\max}$:** Non-monotonic refinement sequence across tetrahedral grids.
4. **FDM $T_{\text{avg}}$:** Monotonic convergence observed, but apparent order $p = 6.42 > 6.0$, exceeding the theoretical maximum order.

**Conclusion:** Strict GCI applicability criteria fail. Rather than applying non-physical clamped extrapolations (such as the legacy disputed values $61.0295^\circ\text{C}$, $60.3265^\circ\text{C}$, $60.5567^\circ\text{C}$ traced and eliminated in `r5_8.txt`), the report and implementation correctly record GCI consensus as **UNAVAILABLE / NaN**, with the finer FEM grid ($60.8134^\circ\text{C}$) presented purely as an independent cross-check.

---

## 6. Matrix Properties & Linear Solver Verification

All system matrices generated by FEM, FDM, and FVM were audited via `SparseCsr::check_symmetry()` and `SparseCsr::check_positive_diagonal()`:
- **FEM:** Relative asymmetry $\le 2.83 \times 10^{-14}$, strictly positive diagonal $\implies$ Symmetric Positive Definite (SPD). Preconditioned Conjugate Gradient (PCG) converges unconditionally.
- **FDM:** Relative asymmetry $= 0.0$, strictly positive diagonal $\implies$ SPD.
- **FVM:** Implicit orthogonal conductances $a_F = \frac{k A_f}{d_{PN} \cos \theta}$ maintain an exact symmetric M-matrix with positive diagonal on the LHS; non-orthogonal cross-diffusion terms are deferred entirely to the RHS. PCG solves each outer deferred-correction step without loss of symmetry.
- **BiCGSTAB Fallback:** Implemented and verified in `LinearSolvers.h/cpp`; remains available as an audited fallback if asymmetric operators are ever introduced.

---

## 7. Performance & Wall-Clock Timing

Runtimes measured on benchmark hardware (Intel Core i5-11400H @ 2.70 GHz, 6C/12T, 16 GB RAM, Windows 11):

| Level / Config | Solver | DOF | Total Time (Median) | PCG Solve Time | Matrix Assembly |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **Fine Case A** | FEM | 11,799 | $230.2\text{ ms}$ | $18.4\text{ ms}$ | $211.8\text{ ms}$ |
| **Fine Case A** | FDM | 487,620 | $8,392.8\text{ ms}$ | $8,312.1\text{ ms}$ | $80.7\text{ ms}$ |
| **Fine Case A** | FVM | 36,671 | $3,570.3\text{ ms}$ | $1,840.5\text{ ms}$ | $1,729.8\text{ ms}$ |

- **FEM** provides the highest computational efficiency ($1.0\times$), completing in $0.23\text{ s}$.
- **FVM** requires $15.5\times$ more time than FEM due to geometric non-orthogonality evaluations, face flux loops, and outer deferred-correction iterations.
- **FDM** requires $36.5\times$ more time than FEM because capturing the tapered fins on a regular Cartesian grid demands $41\times$ more degrees of freedom ($487,620$ vs $11,799$).
