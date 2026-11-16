# THERMATRIX Repository Baseline Audit (Phase 1)

**Date:** 2026-11-16  
**Auditor:** Sanjaay S  
**Branch:** `round2`  
**Base Commit:** `653d1698fef9f04611033e0b61f3c7b1574a6b00`  
**Compiler:** GCC / MinGW-w64 (C++17)  
**Build Type:** Release  

---

## 1. Baseline Git State & Remote Tracking
- Remote URL: `https://github.com/sanjaaysoundar/Thermatrix.git`
- Checked out branch: `round2` tracking `origin/round2`
- Commit history (recent 5 commits):
  - `653d169` r5-final: complete submission report with reconciled energy balance, probe consensus audit, and timing layout
  - `145a51a` r5-10: final evidence and delivery audit log and manifest synchronization
  - `ad76a4a` r5-9: repair PDF typography, embedded notation, and page layout formatting
  - `0ebd5f7` r5-8: trace origin of disputed values 61.0295, 60.3265, 60.5567 and document artifact status
  - `ee247a8` r5-7: reconcile energy balance to 7.56e-11 max and report min/median/max timing statistics

---

## 2. Source & Test File Inventory

### `src/` (Top-level & App)
- `src/main.cpp`: Entry point for GUI application.
- `src/launcher.cpp`: Launcher utility.
- `src/app/App.h`, `App.cpp`: OpenGL / Dear ImGui 3D interactive application.

### `src/core/` (Round 1 Legacy Solvers & Utilities)
- `FemSolver.h`, `FemSolver.cpp`: 1D FEM rod solver.
- `BackpropSolver.h`, `BackpropSolver.cpp`: Learning module for parameter identification.
- `StlParser.h`, `StlParser.cpp`: Binary and ASCII STL geometry parser.
- `CsvParser.h`, `CsvParser.cpp`: CSV data loader.
- `BenchmarkEngine.h`, `BenchmarkEngine.cpp`: Round 1 execution timer.
- `HardwareMock.h`, `HardwareMock.cpp`: Hardware mock for telemetry.

### `src/thermal3d/` (Round 2 3D Thermal Suite)
- `ThermalProblem.h`: Thermal physics definitions (Aluminium 6061, Robin BC, heat flux).
- `Mesh3D.h`, `Mesh3D.cpp`: Tetrahedral mesh and structured Cartesian grid data structures.
- `SparseCsr.h`, `SparseCsr.cpp`: Sparse CSR matrix, triplet builder, and matrix-vector operations.
- `LinearSolvers.h`, `LinearSolvers.cpp`: PCG with IC(0) preconditioner, Jacobi, SOR, SSOR, BiCGSTAB.
- `SolverResult.h`, `SolverResult.cpp`: Thermal solver telemetry (DOF, iters, residual, temperatures, memory).
- `HeatSinkGeometry.h`, `HeatSinkGeometry.cpp`: Analytical geometry definitions for 9-fin heat sink and cube.
- `FemSolver3D.h`, `FemSolver3D.cpp`: P1 Galerkin tetrahedral FEM solver.
- `FdmSolver3D.h`, `FdmSolver3D.cpp`: 7-point cell-centred voxel FDM solver (finite-volume equivalent on uniform grid).
- `FvmSolver3D.h`, `FvmSolver3D.cpp`: Cell-centred tetrahedral FVM solver.
- `Benchmark3D.h`, `Benchmark3D.cpp`: Benchmarking harness for 3D thermal solvers.
- `Verification3D.h`, `Verification3D.cpp`: V&V suite (1D slab, MMS, 30 probes, GCI).

### `tests/`
- `tests/test_stl_parser.cpp`: Unit tests for STL parsing.
- `tests/test_csv_parser.cpp`: Unit tests for CSV parser.
- `tests/test_fem_solver.cpp`: Unit tests for 1D FEM rod.
- `tests/test_backprop.cpp`: Unit tests for backpropagation parameter recovery.
- `tests/test_benchmark.cpp`: Unit tests for Round 1 benchmarking.
- `tests/test_t3d_linalg.cpp`: Unit tests for 3D sparse CSR and linear solvers.
- `tests/test_t3d_solvers.cpp`: Unit tests for FEM, FDM, and FVM 3D solvers.
- `tests/test_t3d_verification.cpp`: Comprehensive 3D verification and validation suite.
- `tests/run_t3d_benchmark.cpp`: Isolated-process benchmark runner.
- `tests/verify_r4_slab.cpp`: Real 3D solver 1D slab verification across regular/perturbed tets and axes (from patch).
- `tests/r4_analysis.cpp`: Detailed post-processing analysis driver (from patch).

---

## 3. Baseline Build & Test Execution

### Build Command
```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j
```
* **Status:** Clean build (49/49 targets built, 0 errors, minor compiler warnings on redefined NOMINMAX and unused static function).

### Test Suite Execution
```bash
ctest --test-dir build --output-on-failure
```
* **Results:** 7/8 passed (88% pass rate, 23.72s runtime).
  1. `test_stl_parser`: PASSED (0.46s)
  2. `test_csv_parser`: PASSED (0.19s)
  3. `test_fem_solver`: PASSED (0.08s)
  4. `test_backprop`: PASSED (0.06s)
  5. `test_benchmark`: PASSED (0.08s)
  6. `test_t3d_linalg`: PASSED (0.07s)
  7. `test_t3d_solvers`: PASSED (2.76s)
  8. `test_t3d_verification`: FAILED (19.99s) - 22 subtests passed, 2 subtests failed.

### Investigation of Baseline Test Failure
* **Subtest Failure:** Section 2: Manufactured Solution Convergence Order Test:
  ```text
  Manufactured Solution Observed Order p:
    FEM: p = 1.84672
    FDM: p = 1.86263
    FVM: p = 0.685482
  [FAIL] FVM observed convergence order >= 0.75 (first-order O(h) on skewed tets)
  ```
* **Root Cause Analysis:** `FvmSolver3D::solve_mms` in `src/thermal3d/FvmSolver3D.cpp` was not updated when the main `FvmSolver3D::solve` was updated with the over-relaxed non-orthogonal split and weighted least squares. `solve_mms` still used the older uncorrected orthogonal conductance formulation and Green-Gauss gradient evaluation on skewed tetrahedra, yielding an observed asymptotic order of only 0.685. This will be resolved in Phase 2.

---

## 4. Feature Implementation Status Matrix

| Feature / Requirement | Status | Details / Notes |
| :--- | :---: | :--- |
| **Round 1 Features (1D FEM, Backprop, CSV, STL, GUI)** | Implemented | All legacy tests pass cleanly; preserved intact. |
| **Corrected FVM Formulation (`FvmSolver3D`)** | Partial | Core `solve()` has over-relaxed split, but `solve_mms()` uses legacy Green-Gauss. Patch needs formal commit and CMake integration. |
| **Real 3D 1-D Slab Verification (`verify_r4_slab`)** | Partial | Source file present; needs CMake integration, execution, and CSV export. |
| **3D Post-Processing Driver (`r4_analysis`)** | Partial | Source file present; needs CMake integration and execution. |
| **Geometry Fidelity & Watertightness Check** | Partial | `HeatSinkGeometry` exists; needs automated watertightness test (edge sharing, volume check) and fin-sweep support (3, 5, 9 fins). |
| **Matrix Symmetry & Definiteness Check** | Partial | Basic CSR exists; needs formal `check_symmetry(A)` and positive-diagonal assertions in tests, with documented BiCGSTAB fallback. |
| **Benchmark Suite (33 Configurations, Isolated Processes)** | Partial | `run_t3d_benchmark.cpp` exists; needs verification of 33 unique process runs, stage timings (mesh, assembly, solve), and RSS telemetry. |
| **30 Fixed Probes & Interpolation** | Implemented | Barycentric for FEM, trilinear for FDM, LSQ cell+gradient for FVM. |
| **Grid Convergence Index (GCI)** | Implemented | Celik et al. (2008) strict check implemented; non-monotonic quantities correctly report UNAVAILABLE. |
| **Energy Balance Verification** | Implemented | Relative balance checked on all runs; target $\le 5\times 10^{-10}$. |
| **OpenGL GUI Thermal 3D Integration** | Implemented | Exposes solvers, resolution levels, cases, temperature fields, cut planes. Needs fin-count selector and cube selector verification. |
| **Visualizations & Publication Figures** | Implemented | 15 figures generated at 300 DPI in `results/figures/`. |
| **Discrepancy Log & Documentation** | Partial | Needs formal `docs/DISCREPANCIES.md` and updated README. |
