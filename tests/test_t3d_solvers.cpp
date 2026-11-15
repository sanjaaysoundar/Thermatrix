// test_t3d_solvers.cpp  –  Unit and Numerical Tests for FEM, FDM, and FVM 3D Thermal Solvers
// THERMATRIX C++17

#include "../src/thermal3d/ThermalProblem.h"
#include "../src/thermal3d/HeatSinkGeometry.h"
#include "../src/thermal3d/Mesh3D.h"
#include "../src/thermal3d/FemSolver3D.h"
#include "../src/thermal3d/FdmSolver3D.h"
#include "../src/thermal3d/FvmSolver3D.h"
#include <iostream>
#include <cmath>
#include <cassert>

using namespace thermatrix::t3d;

static int g_passed = 0, g_failed = 0;

#define ASSERT_TRUE(cond, msg) \
    do { \
        if (cond) { ++g_passed; std::cout << "[PASS] " << msg << "\n"; } \
        else      { ++g_failed; std::cout << "[FAIL] " << msg << "\n"; } \
    } while (0)

#define ASSERT_NEAR(a, b, tol, msg) \
    ASSERT_TRUE(std::abs((double)(a) - (double)(b)) <= (tol), \
                msg << " (got=" << (a) << ", exp=" << (b) << ", tol=" << tol << ")")

void test_geometry() {
    HeatSinkGeometry geom;
    double vol = geom.total_volume();
    // Base: 0.06*0.06*0.004 = 1.44e-5. Fins: 9 * (0.06*0.5*(0.002+0.0012)*0.025) = 2.16e-5. Total = 3.6e-5 m³
    ASSERT_NEAR(vol, 3.6e-5, 1e-8, "Heat sink analytical volume = 3.6e-5 m³");

    double a_conv = geom.total_exposed_area();
    ASSERT_TRUE(a_conv > 0.025 && a_conv < 0.040, "Heat sink convective area in [0.025, 0.040] m²");

    // Point in base
    ASSERT_TRUE(geom.is_in_solid(0.030, 0.030, 0.002), "Point inside base slab is solid");
    // Point in fin 4 (center fin at y=30mm, x=30mm, z=15mm)
    ASSERT_TRUE(geom.is_in_solid(0.030, 0.030, 0.015), "Point inside center fin is solid");
    // Point in air gap between fin 3 and fin 4 (y=27mm)
    ASSERT_TRUE(!geom.is_in_solid(0.030, 0.027, 0.015), "Point in air gap is NOT solid");
    // Point outside domain
    ASSERT_TRUE(!geom.is_in_solid(0.10, 0.030, 0.01), "Point outside bounding box is NOT solid");

    // Source patch test (patch is 20±5 mm, 35±5 mm, at z=0)
    ASSERT_TRUE(geom.is_in_source_patch(0.020, 0.035, 0.0), "Patch center is in source patch");
    ASSERT_TRUE(!geom.is_in_source_patch(0.050, 0.050, 0.0), "Point outside patch is not in source patch");
    ASSERT_TRUE(!geom.is_in_source_patch(0.020, 0.035, 0.01), "Point at z>0 is not in underside patch");
}

void test_fem_mesh() {
    HeatSinkGeometry geom;
    FemMesh mesh = generate_fem_mesh(geom, MeshResolution::Coarse);
    ASSERT_TRUE(mesh.n_nodes() > 100, "FEM mesh has > 100 nodes");
    ASSERT_TRUE(mesh.n_elements() > 500, "FEM mesh has > 500 tetrahedral elements");
    ASSERT_TRUE(mesh.n_boundary_faces() > 100, "FEM mesh has > 100 boundary faces");

    double vol = mesh.total_volume();
    ASSERT_NEAR(vol, 3.6e-5, 1e-6, "FEM mesh discretized volume matches exact volume (3.6e-5 m³)");

    double src_area = mesh.total_source_area();
    ASSERT_NEAR(src_area, 1.0e-4, 2e-5, "FEM mesh source patch area approx 100 mm²");
}

void test_structured_grid() {
    HeatSinkGeometry geom;
    StructuredGrid3D grid = generate_structured_grid(geom, MeshResolution::Coarse);
    ASSERT_TRUE(grid.n_solid_cells() > 500, "Structured grid has > 500 solid cells");
    ASSERT_TRUE(!grid.internal_faces.empty(), "Grid has internal faces");
    ASSERT_TRUE(!grid.boundary_faces.empty(), "Grid has boundary faces");

    double vol = grid.total_volume();
    ASSERT_NEAR(vol, 3.6e-5, 1e-5, "Voxel grid volume approximates exact volume within resolution");
}

void test_fem_solve() {
    ThermalProblem prob;
    FemSolver3D solver(prob);

    // Case A: h = 10 W/m²K
    SolverResult rA = solver.solve(ConvCase::A, MeshResolution::Coarse);
    ASSERT_TRUE(rA.success, "FEM solve Case A succeeds");
    ASSERT_TRUE(rA.iterations > 0, "FEM iterations > 0");
    ASSERT_TRUE(rA.T_max > 30.0 && rA.T_max < 90.0, "FEM T_max is physically realistic");
    ASSERT_TRUE(rA.T_min >= 25.0, "FEM T_min >= T_inf (25 °C)");
    ASSERT_TRUE(rA.Q_balance_rel < 0.03, "FEM Case A energy balance error < 3%");

    // Case B: h = 25 W/m²K
    SolverResult rB = solver.solve(ConvCase::B, MeshResolution::Coarse);
    ASSERT_TRUE(rB.success, "FEM solve Case B succeeds");
    ASSERT_TRUE(rB.T_max < rA.T_max, "Higher convection (h=25) yields lower peak temperature than h=10");
    ASSERT_TRUE(rB.Q_balance_rel < 0.03, "FEM Case B energy balance error < 3%");
}

void test_fdm_solve() {
    ThermalProblem prob;
    FdmSolver3D solver(prob);

    SolverResult rA = solver.solve(ConvCase::A, MeshResolution::Coarse);
    ASSERT_TRUE(rA.success, "FDM solve Case A succeeds");
    ASSERT_TRUE(rA.iterations > 0, "FDM iterations > 0");
    ASSERT_TRUE(rA.T_max > 30.0 && rA.T_max < 90.0, "FDM T_max is physically realistic");
    ASSERT_TRUE(rA.T_min >= 25.0, "FDM T_min >= T_inf (25 °C)");
    ASSERT_TRUE(rA.Q_balance_rel < 0.03, "FDM Case A energy balance error < 3%");

    SolverResult rB = solver.solve(ConvCase::B, MeshResolution::Coarse);
    ASSERT_TRUE(rB.success, "FDM solve Case B succeeds");
    ASSERT_TRUE(rB.T_max < rA.T_max, "FDM: Case B peak temperature < Case A");
    ASSERT_TRUE(rB.Q_balance_rel < 0.03, "FDM Case B energy balance error < 3%");
}

void test_fvm_solve() {
    ThermalProblem prob;
    FvmSolver3D solver(prob);

    SolverResult rA = solver.solve(ConvCase::A, MeshResolution::Coarse);
    ASSERT_TRUE(rA.success, "FVM solve Case A succeeds");
    ASSERT_TRUE(rA.iterations > 0, "FVM iterations > 0");
    ASSERT_TRUE(rA.T_max > 30.0 && rA.T_max < 90.0, "FVM T_max is physically realistic");
    ASSERT_TRUE(rA.T_min >= 25.0, "FVM T_min >= T_inf (25 °C)");
    ASSERT_TRUE(rA.Q_balance_rel < 0.01, "FVM Case A energy balance error < 1% (exact conservation)");

    MeshQualityStats stats = solver.last_quality_stats();
    std::cout << "  FVM Mesh Quality Telemetry:\n"
              << "    Internal faces: " << stats.n_internal_faces << "\n"
              << "    Boundary faces: " << stats.n_boundary_faces << "\n"
              << "    Non-orthogonality angle: min=" << stats.min_non_ortho_deg
              << " deg, max=" << stats.max_non_ortho_deg
              << " deg, mean=" << stats.mean_non_ortho_deg << " deg\n";

    ASSERT_TRUE(stats.max_non_ortho_deg > 0.0, "Tetrahedral mesh exhibits non-orthogonality");
    ASSERT_TRUE(stats.mean_non_ortho_deg < 60.0, "Mean non-orthogonality angle is acceptable (< 60 deg)");

    SolverResult rB = solver.solve(ConvCase::B, MeshResolution::Coarse);
    ASSERT_TRUE(rB.success, "FVM solve Case B succeeds");
    ASSERT_TRUE(rB.T_max < rA.T_max, "FVM: Case B peak temperature < Case A");
    ASSERT_TRUE(rB.Q_balance_rel < 0.01, "FVM Case B energy balance error < 1%");
}

void test_cross_solver_consistency() {
    ThermalProblem prob;
    FemSolver3D fem(prob);
    FdmSolver3D fdm(prob);
    FvmSolver3D fvm(prob);

    SolverResult r_fem = fem.solve(ConvCase::A, MeshResolution::Coarse);
    SolverResult r_fdm = fdm.solve(ConvCase::A, MeshResolution::Coarse);
    SolverResult r_fvm = fvm.solve(ConvCase::A, MeshResolution::Coarse);

    std::cout << "\nCross-Solver Comparison (Case A, Coarse):\n"
              << "  FEM: DOF=" << r_fem.n_dof << ", NNZ=" << r_fem.n_nnz << ", T_min=" << r_fem.T_min << " C, T_max=" << r_fem.T_max << " C, T_avg=" << r_fem.T_avg << " C\n"
              << "  FDM: DOF=" << r_fdm.n_dof << ", NNZ=" << r_fdm.n_nnz << ", T_min=" << r_fdm.T_min << " C, T_max=" << r_fdm.T_max << " C, T_avg=" << r_fdm.T_avg << " C\n"
              << "  FVM: DOF=" << r_fvm.n_dof << ", NNZ=" << r_fvm.n_nnz << ", T_min=" << r_fvm.T_min << " C, T_max=" << r_fvm.T_max << " C, T_avg=" << r_fvm.T_avg << " C\n\n";

    // 1. Assert FDM and FVM matrices are NOT identical
    ASSERT_TRUE(r_fdm.n_dof != r_fvm.n_dof, "FDM and FVM DOFs are NOT identical (distinct grid vs mesh)");
    ASSERT_TRUE(r_fdm.n_nnz != r_fvm.n_nnz, "FDM and FVM NNZs are NOT identical");
    ASSERT_TRUE(r_fem.n_dof != r_fvm.n_dof, "FEM (nodes) and FVM (tets) DOFs are NOT identical");

    // 2. Assert FEM and FVM operate on the SAME mesh
    HeatSinkGeometry geom;
    FemMesh shared_mesh = generate_fem_mesh(geom, MeshResolution::Coarse);
    ASSERT_TRUE(r_fem.n_dof == shared_mesh.n_nodes(), "FEM DOF matches shared mesh node count");
    ASSERT_TRUE(r_fvm.n_dof == shared_mesh.n_elements(), "FVM DOF matches shared mesh tetrahedral element count");

    // Mean temperatures should be close across all 3 discretizations
    ASSERT_NEAR(r_fdm.T_avg, r_fvm.T_avg, 3.0, "FDM and FVM average temperatures within 3.0 °C");
    ASSERT_NEAR(r_fem.T_avg, r_fvm.T_avg, 3.0, "FEM and FVM average temperatures within 3.0 °C");
}

int main() {
    std::cout << "=== THERMATRIX 3D Thermal Solvers Test Suite ===\n\n";

    std::cout << "--- Geometry Tests ---\n";
    test_geometry();

    std::cout << "\n--- FEM Mesh Generation Tests ---\n";
    test_fem_mesh();

    std::cout << "\n--- Structured Grid Tests ---\n";
    test_structured_grid();

    std::cout << "\n--- FEM Solver Tests ---\n";
    test_fem_solve();

    std::cout << "\n--- FDM Solver Tests ---\n";
    test_fdm_solve();

    std::cout << "\n--- FVM Solver Tests ---\n";
    test_fvm_solve();

    std::cout << "\n--- Cross-Solver Consistency Tests ---\n";
    test_cross_solver_consistency();

    std::cout << "\n==============================================\n";
    std::cout << g_passed << " passed, " << g_failed << " failed.\n";
    return (g_failed > 0) ? 1 : 0;
}
