# THERMATRIX – Engineering Workbench

**THERMATRIX** is a high-performance native desktop engineering workbench for thermal finite element analysis (FEM), neural network gradient backpropagation verification, 3D CAD/field visualization, multi-core CPU scaling benchmarking, and industrial hardware telemetry.

Built strictly in **C++17** with **Dear ImGui**, **ImPlot**, and **OpenGL 3.3**, THERMATRIX is fully self-contained and offers true **one-click launch on Windows 11 without installation**.

---

## ⚡ Quick Start: One-Click Launch on Windows 11

No installations, runtimes, or administrative rights required.

1. Double-click **`Launch.bat`** in the repository root.
   *(Alternatively, launch `bin\THERMATRIX.exe` directly).*
2. The application opens instantly with full OpenGL hardware acceleration.

Sample data files are pre-loaded and accessible in the `data/` directory:
- `data/example.stl`: 3D solid cube CAD geometry.
- `data/example_temperatures.csv`: 3D thermal point-cloud field data.

---

## 🌟 Key Capabilities

### 1. 1D Steady-State Thermal FEM Solver
- Solves $-k \frac{d^2 T}{dx^2} = q$ with Dirichlet boundary conditions ($T(0)=100^\circ\text{C}, T(L)=20^\circ\text{C}$).
- Weak form Galerkin finite-element formulation with linear shape functions.
- $O(N)$ tridiagonal matrix solver (Thomas Algorithm / TDMA).
- Real-time comparison against exact analytical solution with second-order $O(h^2)$ midpoint error convergence plotting.

### 2. Analytical Backpropagation & Finite-Difference Verification
- Forward architecture: $z = w x + b$, $h = \tanh(z)$, $\hat{y} = v h + c$, Loss $J = \frac{1}{2}(\hat{y}-y)^2$.
- Closed-form analytical gradient derivation using chain rule across all four parameters ($\frac{\partial J}{\partial w}, \frac{\partial J}{\partial b}, \frac{\partial J}{\partial v}, \frac{\partial J}{\partial c}$).
- Independent central finite-difference numerical verification ($\epsilon = 10^{-6}$) with strict error assertion ($|\nabla_{\text{analytical}} - \nabla_{\text{FD}}| < 10^{-5}$).
- 100-step gradient descent optimization showing monotonic loss convergence.

### 3. 3D CAD & Temperature Field Visualization
- Robust ASCII STL mesh parser with boundary box and normal calculations.
- Temperature CSV field parser with automatic min/max range detection.
- Interactive 3D OpenGL viewport featuring wireframe mesh rendering, color-mapped temperature point clouds (Blue $\to$ Cyan $\to$ Green $\to$ Yellow $\to$ Red), and camera orbit/pan controls.

### 4. Multi-Core Benchmark Engine
- Evaluates strong scaling across 1 to $p_{\text{max}}$ CPU threads for 5,000 thermal FEM problems.
- Deterministic random seed reproducibility (`std::mt19937_64`).
- Automatic measurement of Speedup ($S_p = T_1 / T_p$) and Efficiency ($E_p = S_p / p$).
- Numerical validation confirming parallel/serial agreement ($|T_{\text{serial}} - T_{\text{parallel}}| \le 10^{-8\ \circ}\text{C}$).
- One-click CSV export to `results/benchmark_results.csv`.

### 5. Hardware Interfacing & Safety Telemetry
- Modbus RTU/TCP register mapping and CAN bus frame abstraction.
- Thread-safe simulation rig modeling physical heater response and convective Newton cooling.
- Live telemetry dials, manual override sliders, and automatic over-temperature safety interlocks.

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
│   ├── glfw3.dll                    # Runtime libraries
│   ├── libgcc_s_seh-1.dll
│   ├── libgomp-1.dll
│   ├── libstdc++-6.dll
│   ├── libwinpthread-1.dll
│   ├── test_*.exe                   # Compiled test binaries
│   └── data/                        # Sample data files
├── data/                            # Sample input datasets
│   ├── example.stl                  # ASCII STL cube mesh
│   └── example_temperatures.csv     # 3D temperature field CSV
├── docs/                            # Engineering documentation
│   ├── report.md                    # Technical report (<=6 pages)
│   ├── block_diagram.md             # Hardware and software architecture diagrams
│   └── ai_use_log.md                # AI prompts, decisions, and verification log
├── results/                         # Benchmark and evaluation exports
│   └── benchmark_results.csv        # Empirical scaling measurements
├── src/                             # Source code
│   ├── main.cpp                     # Application entry point (WinMain)
│   ├── app/                         # UI & presentation (Dear ImGui)
│   │   ├── App.h / App.cpp
│   ├── core/                        # Numerical backend (100% pure C++17)
│   │   ├── FemSolver.h / .cpp       # 1D Thermal FEM & TDMA solver
│   │   ├── BackpropSolver.h / .cpp  # Gradient engine & FD verification
│   │   ├── StlParser.h / .cpp       # ASCII STL parser & validator
│   │   ├── CsvParser.h / .cpp       # Temperature CSV parser & validator
│   │   ├── BenchmarkEngine.h / .cpp # Multi-core benchmark engine
│   │   └── HardwareMock.h / .cpp    # Sensor/actuator simulation rig
├── tests/                           # Unit test suite
│   ├── test_fem_solver.cpp          # 88 FEM assertions
│   ├── test_backprop.cpp            # 123 Backprop assertions
│   ├── test_stl_parser.cpp          # 15 STL assertions
│   ├── test_csv_parser.cpp          # 19 CSV assertions
│   └── test_benchmark.cpp           # 17 Benchmark assertions
└── external/                        # Third-party submodules
    ├── imgui/                       # Dear ImGui (MIT)
    └── implot/                      # ImPlot (MIT)
```

---

## 🔧 Building from Source

### Prerequisites
- CMake 3.16 or newer
- C++17 compatible compiler:
  - **MinGW-w64 GCC 10+** (recommended on Windows)
  - **Clang 12+**
  - **MSVC 2019 / 2022**

### Build Commands (MinGW / Ninja)
```bash
# Configure
cmake -B build -G "Ninja"

# Build all binaries and tests
cmake --build build

# Run complete test suite (262 tests)
cd build && ctest --output-on-failure
```

---

## 🧪 Verification & Test Results

The automated test harness verifies mathematical correctness, numerical precision, and error handling across 262 assertions:

```
Test project D:/THERMATRIX/build
    Start 1: test_stl_parser
1/5 Test #1: test_stl_parser ..................   Passed    0.11 sec
    Start 2: test_csv_parser
2/5 Test #2: test_csv_parser ..................   Passed    0.20 sec
    Start 3: test_fem_solver
3/5 Test #3: test_fem_solver ..................   Passed    0.07 sec
    Start 4: test_backprop
4/5 Test #4: test_backprop ....................   Passed    0.06 sec
    Start 5: test_benchmark
5/5 Test #5: test_benchmark ...................   Passed    0.07 sec

100% tests passed out of 5 (262 individual assertions, 0 failures)
```

---

## 📜 Licenses

All dependencies are free and permissively licensed:
- **Dear ImGui**: MIT License
- **ImPlot**: MIT License
- **GLFW**: zlib / libpng License
- **OpenGL**: System runtime
See [`DEPENDENCIES.md`](file:///d:/THERMATRIX/DEPENDENCIES.md) for full license texts.
