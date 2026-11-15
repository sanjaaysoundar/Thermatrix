# THERMATRIX Round 2 – Experiment Log

**Generated**: 2026-11-15T18:17:49.858717  
**Git branch**: `round2`  
**Git commit**: `145a51a813c922cc5858f5983f1f15abb0756bce`

## Exact Commands Used

### Environment Setup
```bash
cd d:\THERMATRIX
git checkout round2
# Build (CMake + Ninja + MinGW-W64 GCC 16.2.0)
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=C:/msys64/mingw64/bin/c++.exe
cmake --build build --parallel
```

### Mesh Generation (Gmsh offline, Python)
```bash
python scripts/make_heatsink.py
# → data/heatsink.stl (watertight, 100% 2-manifold, 60x60x4mm + 9 tapered fins)
# → data/heatsink_coarse.msh, heatsink_medium.msh, heatsink_fine.msh
# → data/cube_coarse.msh, cube_medium.msh, cube_fine.msh
# → data/heatsink_3fins.msh, heatsink_5fins.msh
```

### Single-Process Benchmark (FIX 2 format)
```bash
# Example: single combination
build\bin\run_t3d_benchmark.exe --solver fem --level 1 --case A --repeats 5 --warmup 1 --out results/complexity_study.csv
# Full sweep (Python orchestrator):
python scripts/run_complexity_study.py
```

### Test Suite
```bash
cd build && ctest --output-on-failure -C Release
# → 8/8 passed: stl_parser, csv_parser, fem_solver, backprop, benchmark,
#               t3d_linalg (28 cases), t3d_solvers (51 cases), t3d_verification (19 cases)
```

### Figure Generation (FIX 6)
```bash
python scripts/make_figures.py
# → 15 figures at 300 dpi in THERMATRIX_ASSIGNMENT_2/Visualizations/
```

## Solver Details

| Solver | Discretisation | Mesh | Linear System | BC Treatment |
|--------|---------------|------|---------------|--------------|
| FEM    | P1 Galerkin weak form | Shared Gmsh tet mesh | PCG-IC0 | Robin integrals on boundary faces |
| FDM    | 7-point central difference | Cartesian voxel grid (STL ray-cast) | PCG-IC0 | Ghost-node Robin, staircase boundary |
| FVM    | Cell-centred, tet control volumes | Shared Gmsh tet mesh | PCG-IC0 | Flux BCs on boundary faces, non-orthogonal correction |

## Physics Parameters

| Parameter | Value |
|-----------|-------|
| Material | Aluminium 6061, k = 167 W/mK |
| Heat input Q | 10 W |
| Source patch | 10×10 mm, offset dx=−10 mm, dy=+5 mm |
| Convection h (Case A) | 10 W/m²K |
| Convection h (Case B) | 25 W/m²K |
| Ambient T∞ | 25°C |
| Underside (outside source) | Adiabatic |

## Verification Results (from validation_results.csv)

| Test | FEM | FDM | FVM |
|------|-----|-----|-----|
| 1D Slab L2 error | <0.08% | <0.15% | <0.11% |
| Manufactured solution order p | ≈1.95 | ≈1.88 | ≈1.72 |
| 30-probe RMSE vs fine FEM | 0.032°C | 0.441°C | 0.709°C |
| GCI (all levels) | <3.5% | <3.5% | <3.5% |

## Commits on `round2`

| Hash | Description |
|------|-------------|
| `5c15afa` | FIX 1: FVM cell-centred on shared tet mesh |
| `0a9898d` | FIX 2: Isolated single-process benchmark CLI |
| `12f342a` | FIX 3: 9-tapered-fin geometry, Gmsh .msh reader |
| `5e26c24` | FIX 4: 1D slab, manufactured solution, GCI, probe RMSE |
| `daac306` | FIX 5: Geometric complexity study (33 runs) |
| _(this commit)_ | FIX 6: Real figures, machine info, experiment log |
