# THERMATRIX – Engineering Workbench

**THERMATRIX** is a high-performance native desktop engineering workbench for 3D thermal simulation (FEM, FDM, FVM), 1D finite element analysis, neural network gradient backpropagation verification, CAD/field visualization, and multi-core CPU scaling benchmarking.

Built strictly in **C++17** with **Dear ImGui**, **ImPlot**, and **OpenGL 3.3**, THERMATRIX is fully self-contained and offers true **one-click launch on Windows 11 without installation**.

---

## ⚡ Quick Start: One-Click Launch on Windows 11

No installations, runtimes, or administrative rights required.

1. Double-click **`Launch.bat`** in the repository root.  
   *(Alternatively, launch `bin\THERMATRIX.exe` directly).*
2. The application opens instantly with full OpenGL hardware acceleration.

Sample data and generated field files are in the `data/` and `results/` directories:
- `data/heatsink.stl`: Manifold watertight 3D CAD model of 9-fin industrial heat sink.
- `data/cube.stl`: 3D solid cube control geometry.
- `results/benchmark_results.csv`: Complete 33-configuration solver comparison dataset.
- `results/validation_results.csv`: Analytical slab, MMS, and GCI verification metrics.

---

## 🌟 Key Capabilities

### 1. 3D Thermal Solver Suite (Round 2 Core)

THERMATRIX implements and evaluates three distinct numerical discretizations for steady-state conduction-convection on complex 3D geometries:

$$-\nabla \cdot (k \nabla T) = 0 \quad \text{in } \Omega$$
$$\mathbf{q}'' \cdot \mathbf{n} = q_{\text{in}}'' \quad \text{on } \Gamma_{\text{source}}, \quad -k \nabla T \cdot \mathbf{n} = h(T - T_\infty) \quad \text{on } \Gamma_{\text{conv}}, \quad -k \nabla T \cdot \mathbf{n} = 0 \quad \text{on } \Gamma_{\text{adiabatic}}$$

#### A. Finite Element Method (FEM)
- Weak-form Galerkin formulation using 4-node linear tetrahedral elements ($P_1$ Lagrange basis).
- Element stiffness matrix $K_e = \int_{\Omega_e} B^T k B \, d\Omega$, with exact analytical volume and shape-function gradients.
- Direct Robin boundary integral $\int_{\Gamma_{\text{conv}}} h N_i N_j \, d\Gamma$ and Neumann source flux $\int_{\Gamma_{\text{source}}} q'' N_i \, d\Gamma$.
- Conforming unstructured mesh matching exact geometric boundaries.
- Symmetric Positive Definite (SPD) sparse linear system solved via Jacobi-preconditioned CG.

#### B. Finite Difference Method (FDM)
- Standard 7-point Laplacian stencil on a structured Cartesian regular voxel grid.
- **Equivalence Disclosure:** FDM with volume-averaged harmonic interface conductivity and cell-face flux balances is algebraically equivalent to a cell-centred Finite Volume Method on a regular Cartesian grid.
- Staircase voxelization of boundary faces with modified surface cell convective Robin coupling.
- Strictly symmetric M-matrix with diagonal dominance, solved via Jacobi-PCG.

#### C. Finite Volume Method (FVM)
- Unstructured cell-centred finite volume formulation operating directly on tetrahedral elements.
- Exact conservative face-flux summation: $\sum_f F_f = 0$.
- **Over-Relaxed Non-Orthogonal Correction:**
  $$a_{PN} = \frac{k A_f}{d_{PN} \cos \theta_f}, \quad \mathbf{k}_f = \mathbf{A}_f - \frac{A_f}{\cos \theta_f} \mathbf{e}_{PN}$$
- Implicit orthogonal conductances remain strictly symmetric on the LHS ($M$-matrix); non-orthogonal cross-diffusion fluxes are evaluated via Weighted Least Squares (WLS) cell gradients and deferred to the RHS.
- Outer deferred-correction iteration loop (up to 40 iterations, convergence criterion $\max |\delta T| < 10^{-9}\text{ K}$, relaxation $\alpha = 1.0$) ensures machine-precision convergence even on skewed tetrahedral meshes.

### 2. Linear Solvers & Matrix Symmetry Telemetry
- Compressed Sparse Row (`SparseCsr`) format optimized for cache-friendly SpMV.
- Built-in `check_symmetry()` and `check_positive_diagonal()` methods for continuous SPD validation.
- Jacobi Preconditioned Conjugate Gradient (PCG) solver with strict tolerance $\epsilon = 10^{-10}$.
- Biconjugate Gradient Stabilized (`bicgstab_solve()`) solver implemented and unit-tested as an audited fallback for general or asymmetric systems.

### 3. Geometry Verification & Watertightness
- CAD geometry engine supporting parameterizable extruded bases and tapered fins.
- Automated watertightness checker (`check_stl_watertightness()`) using half-edge mesh topology audits:
  - Exact manifold edge sharing (every edge shared by exactly 2 triangles).
  - 0 boundary edges, 0 non-manifold edges.
  - Closed volume computed via Divergence Theorem matches analytical volume ($3.600 \times 10^{-5}\text{ m}^3$) within $0.001\%$.

### 4. 1D Steady-State Thermal FEM Solver (Round 1)
- Solves $-k \frac{d^2 T}{dx^2} = q$ with Dirichlet boundary conditions ($T(0)=100^\circ\text{C}, T(L)=20^\circ\text{C}$).
- Weak form Galerkin finite-element formulation with linear shape functions.
- $O(N)$ tridiagonal matrix solver (Thomas Algorithm / TDMA).
- Real-time comparison against exact analytical solution with second-order $O(h^2)$ midpoint error convergence plotting.

### 5. Analytical Backpropagation & Verification (Round 1)
- Forward architecture: $z = w x + b$, $h = \tanh(z)$, $\hat{y} = v h + c$, Loss $J = \frac{1}{2}(\hat{y}-y)^2$.
- Closed-form analytical gradient derivation using chain rule across all four parameters.
- Independent central finite-difference numerical verification ($\epsilon = 10^{-6}$) with strict error assertion ($|\nabla_{\text{analytical}} - \nabla_{\text{FD}}| < 10^{-5}$).
- 100-step gradient descent optimization showing monotonic loss convergence.

### 6. Interactive 3D GUI & Visualization
- Interactive 3D OpenGL viewport with camera orbit/pan/zoom.
- Geometry Selector: 9-Fin Heat Sink, Solid Cube, 3-Fin Sweep, 5-Fin Sweep.
- Solver Selector: FEM, FDM, FVM.
- Case Selector: Case A ($h=10$), Case B ($h=25$).
- Resolution Selector: Coarse, Medium, Fine.
- Cross-section cut-plane visualization toggle ($x = 30\text{ mm}$ centerline temperature slice).

---

## 🏗️ Repository Layout

```
THERMATRIX/
├── Launch.bat                       # One-click Windows 11 launcher
├── CMakeLists.txt                   # CMake configuration (C++17)
├── README.md                        # Project documentation
├── DEPENDENCIES.md                  # Complete library license audit
├── bin/                             # Portable pre-compiled application bundle
│   ├── THERMATRIX.exe               # Native executable (no-console GUI)
│   ├── run_t3d_benchmark.exe        # 33-configuration benchmark runner
│   ├── verify_r4_slab.exe           # 3D 1-D slab verification runner
│   ├── r4_analysis.exe              # Probe sampling & field exporter
│   ├── test_*.exe                   # Compiled test binaries
│   └── data/                        # Sample data files
├── data/                            # CAD geometry files
│   ├── heatsink.stl                 # Watertight 9-fin heat sink CAD
│   ├── cube.stl                     # Watertight solid cube CAD
│   └── example_temperatures.csv     # 3D temperature field CSV
├── docs/                            # Engineering documentation & audit logs
│   ├── AUDIT.md                     # Codebase audit & feature matrix
│   ├── DISCREPANCIES.md             # Numerical verification & audit against PDF targets
│   ├── experiment_log.md            # Hardware & benchmark run logs
│   └── phase_logs/                  # Raw execution transcripts
├── results/                         # Benchmark & verification CSVs
│   ├── benchmark_results.csv        # 33 isolated-process benchmark runs
│   ├── complexity_study.csv         # Detailed scaling & memory study
│   ├── validation_results.csv       # Slab, MMS, and GCI metrics
│   ├── slab_verification.csv        # 3D slab multi-axis error verification
│   ├── probe_values.csv             # 30-point fixed probe field samples
│   ├── figure_manifest.csv          # 15 publication figures manifest
│   └── machine_info.txt             # Benchmark hardware telemetry
├── scripts/                         # Python automation & plotting
│   ├── run_complexity_study.py      # Automated benchmark harness
│   ├── make_figures.py              # 15 publication figures generator
│   └── export_convergence.py        # Celik et al. GCI evaluator
├── src/                             # Source code (100% C++17)
│   ├── main.cpp                     # Application entry point
│   ├── app/                         # UI & presentation (Dear ImGui)
│   │   ├── App.h / App.cpp
│   ├── core/                        # Round 1 numerical backend
│   │   ├── FemSolver.h / .cpp       # 1D Thermal FEM & TDMA solver
│   │   ├── BackpropSolver.h / .cpp  # Gradient engine & FD verification
│   │   ├── StlParser.h / .cpp       # ASCII STL parser & validator
│   │   ├── CsvParser.h / .cpp       # Temperature CSV parser & validator
│   │   ├── BenchmarkEngine.h / .cpp # Multi-core benchmark engine
│   │   └── HardwareMock.h / .cpp    # Sensor/actuator simulation rig
│   └── thermal3d/                   # Round 2 3D Thermal Suite
│       ├── HeatSinkGeometry.h / .cpp# CAD generation & watertightness
│       ├── Mesh3D.h / .cpp          # Tet meshing & voxel grids
│       ├── SparseCsr.h / .cpp       # CSR sparse matrix & symmetry audit
│       ├── LinearSolvers.h / .cpp   # Jacobi-PCG & BiCGSTAB
│       ├── FemSolver3D.h / .cpp     # 3D Tetrahedral FEM
│       ├── FdmSolver3D.h / .cpp     # 3D Voxel FDM (FV equivalent)
│       └── FvmSolver3D.h / .cpp     # 3D Over-Relaxed Cell-Centered FVM
└── tests/                           # Automated test suites
    ├── test_fem_solver.cpp          # 1D FEM assertions
    ├── test_backprop.cpp            # Backprop assertions
    ├── test_stl_parser.cpp          # STL parser assertions
    ├── test_csv_parser.cpp          # CSV parser assertions
    ├── test_benchmark.cpp           # Multi-core scaling assertions
    ├── test_t3d_linalg.cpp          # CSR, PCG, BiCGSTAB assertions
    ├── test_t3d_solvers.cpp         # 3D FEM/FDM/FVM physics & watertightness
    ├── test_t3d_verification.cpp    # 3D MMS spatial convergence order
    └── verify_r4_slab.cpp           # 3D 1-D slab exact solution tests
```

---

## 🔧 Building from Source

### Prerequisites
- CMake 3.16 or newer
- C++17 compatible compiler:
  - **MinGW-w64 GCC 10+** (recommended on Windows)
  - **Clang 12+**
  - **MSVC 2019 / 2022**

### Build Commands (MinGW-w64 / Ninja)
```powershell
# Set MinGW compiler in PATH (if required)
$env:PATH = "C:\msys64\mingw64\bin;" + $env:PATH

# Configure Release build
cmake -B build -G "Ninja" -DCMAKE_BUILD_TYPE=Release

# Build all binaries and tests
cmake --build build -j

# Run complete test suite (9 test suites, 100% pass)
ctest --test-dir build --output-on-failure
```

---

## 🧪 Verification & Test Results

The automated test harness verifies mathematical correctness, numerical precision, matrix symmetry, and physical conservation laws across **9 test suites**:

```
Test project D:/THERMATRIX/build
    Start 1: test_stl_parser
1/9 Test #1: test_stl_parser ..................   Passed    0.02 sec
    Start 2: test_csv_parser
2/9 Test #2: test_csv_parser ..................   Passed    0.02 sec
    Start 3: test_fem_solver
3/9 Test #3: test_fem_solver ..................   Passed    0.01 sec
    Start 4: test_backprop
4/9 Test #4: test_backprop ....................   Passed    0.01 sec
    Start 5: test_benchmark
5/9 Test #5: test_benchmark ...................   Passed    0.02 sec
    Start 6: test_t3d_linalg
6/9 Test #6: test_t3d_linalg ..................   Passed    0.08 sec
    Start 7: test_t3d_solvers
7/9 Test #7: test_t3d_solvers .................   Passed    2.79 sec
    Start 8: test_t3d_verification
8/9 Test #8: test_t3d_verification ............   Passed   16.97 sec
    Start 9: verify_r4_slab
9/9 Test #9: verify_r4_slab ...................   Passed    0.17 sec

100% tests passed out of 9 (0 failures)
```

---

## 📜 Licenses

All dependencies are free and permissively licensed:
- **Dear ImGui**: MIT License
- **ImPlot**: MIT License
- **GLFW**: zlib / libpng License
- **OpenGL**: System runtime
See [`DEPENDENCIES.md`](file:///d:/THERMATRIX/DEPENDENCIES.md) for full license texts.
