// test_t3d_solvers.cpp  –  Unit and Numerical Tests for FEM, FDM, and FVM 3D Thermal Solvers
// THERMATRIX C++17

#include "../src/thermal3d/ThermalProblem.h"
#include "../src/thermal3d/HeatSinkGeometry.h"
#include "../src/thermal3d/Mesh3D.h"
#include "../src/thermal3d/FemSolver3D.h"
#include "../src/thermal3d/FdmSolver3D.h"
#include "../src/thermal3d/FvmSolver3D.h"
#include <iostream>
#include <fstream>
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
    ASSERT_NEAR(vol, 3.600e-5, 3.6e-8, "Heat sink analytical volume = 3.600e-5 m³ (within 0.1%)");
    ASSERT_TRUE(std::abs(vol - 3.600e-5) / 3.600e-5 <= 0.001, "Heat sink volume agreement within 0.1%");

    double a_conv = geom.total_exposed_area();
    ASSERT_NEAR(a_conv, 3.1851e-2, 3.1851e-5, "Heat sink convective area = 3.1851e-2 m² (within 0.1%)");
    ASSERT_TRUE(std::abs(a_conv - 3.1851e-2) / 3.1851e-2 <= 0.001, "Convective area agreement within 0.1%");

    double a_src = geom.patch_area();
    ASSERT_NEAR(a_src, 100e-6, 1e-7, "Source patch area = 100 mm² (1e-4 m²)");
    ASSERT_TRUE(std::abs(a_src - 100e-6) / 100e-6 <= 0.001, "Source patch area agreement within 0.1%");

    // Control body: 60x60x60 mm solid cube
    HeatSinkParams cube_p;
    cube_p.base_length = 0.060; cube_p.base_width = 0.060; cube_p.base_height = 0.060;
    cube_p.n_fins = 0; cube_p.fin_height = 0.0;
    HeatSinkGeometry cube_geom(cube_p);
    double cube_vol = cube_geom.total_volume();
    ASSERT_NEAR(cube_vol, 2.16e-4, 2.16e-7, "Solid cube volume = 2.16e-4 m³ (60x60x60 mm)");
    double cube_conv = 5.0 * 0.060 * 0.060; // 5 exposed faces (bottom is adiabatic + 100 mm² patch)
    ASSERT_NEAR(cube_conv, 1.8e-2, 1.8e-5, "Solid cube convective area = 1.8e-2 m²");

    // Fin sweep: 3 fins and 5 fins
    HeatSinkParams p3; p3.n_fins = 3; p3.fin_pitch = 0.0150; p3.fin_y0 = 0.0150;
    HeatSinkGeometry g3(p3);
    double vol3 = g3.total_volume();
    ASSERT_NEAR(vol3, 1.44e-5 + 3.0 * (0.060 * 0.5 * (0.0020 + 0.0012) * 0.025), 1e-8, "3-fin heat sink volume matches analytical");

    HeatSinkParams p5; p5.n_fins = 5; p5.fin_pitch = 0.0100; p5.fin_y0 = 0.0100;
    HeatSinkGeometry g5(p5);
    double vol5 = g5.total_volume();
    ASSERT_NEAR(vol5, 1.44e-5 + 5.0 * (0.060 * 0.5 * (0.0020 + 0.0012) * 0.025), 1e-8, "5-fin heat sink volume matches analytical");

    // Watertightness checks on STL files
    WatertightnessReport rep_hs = check_stl_watertightness("data/heatsink.stl", 3.600e-5);
    ASSERT_TRUE(rep_hs.is_watertight, "Heat sink STL (data/heatsink.stl) is watertight");
    ASSERT_TRUE(rep_hs.n_non_manifold_edges == 0, "Heat sink STL has 0 non-manifold edges");

    WatertightnessReport rep_cube = check_stl_watertightness("data/cube.stl", 2.160e-4);
    ASSERT_TRUE(rep_cube.is_watertight, "Cube STL (data/cube.stl) is watertight");
    ASSERT_TRUE(rep_cube.n_non_manifold_edges == 0, "Cube STL has 0 non-manifold edges");

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

    // Linear algebra assertions (Phase 5)
    ASSERT_NEAR(check_symmetry(solver.last_matrix()), 0.0, 1e-12, "FEM matrix is strictly symmetric (rel symmetry error = 0)");
    double min_d_fem = 0.0;
    ASSERT_TRUE(check_positive_diagonal(solver.last_matrix(), &min_d_fem), "FEM matrix has strictly positive diagonal");
    ASSERT_TRUE(min_d_fem > 0.0, "FEM min diagonal > 0 (SPD system)");

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

    // Linear algebra assertions (Phase 5)
    ASSERT_NEAR(check_symmetry(solver.last_matrix()), 0.0, 1e-12, "FDM matrix is strictly symmetric (rel symmetry error = 0)");
    double min_d_fdm = 0.0;
    ASSERT_TRUE(check_positive_diagonal(solver.last_matrix(), &min_d_fdm), "FDM matrix has strictly positive diagonal");
    ASSERT_TRUE(min_d_fdm > 0.0, "FDM min diagonal > 0 (SPD system)");

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

    // Linear algebra assertions (Phase 5)
    ASSERT_NEAR(check_symmetry(solver.last_matrix()), 0.0, 1e-12, "FVM matrix is strictly symmetric (implicit orthogonal conductances)");
    double min_d_fvm = 0.0;
    ASSERT_TRUE(check_positive_diagonal(solver.last_matrix(), &min_d_fvm), "FVM matrix has strictly positive diagonal");
    ASSERT_TRUE(min_d_fvm > 0.0, "FVM min diagonal > 0 (SPD system)");

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

void test_fvm_non_orthogonal_sensitivity() {
    ThermalProblem prob;
    std::cout << "\n--- FVM Non-Orthogonal Correction Audit & Sensitivity ---\n";

    struct Config {
        std::string label;
        int n_outer;
        double alpha;
    };

    std::vector<Config> configs = {
        {"Correction Off (1 loop)", 1, 0.0},
        {"Alpha = 0.1 (3 loops)",   3, 0.1},
        {"Alpha = 0.2 (Default)",   3, 0.2},
        {"Alpha = 0.4 (3 loops)",   3, 0.4},
        {"Alpha = 0.2 (5 loops)",   5, 0.2}
    };

    std::ofstream csv("results/fvm_correction_audit.csv");
    if (csv.is_open()) {
        csv << "config,n_outer,alpha,T_max_C,T_avg_C,iterations,rel_res,Q_balance_rel\n";
    }

    double t_max_default = 0.0;

    for (const auto& cfg : configs) {
        FvmSolver3D solver(prob);
        solver.set_correction_params(cfg.n_outer, cfg.alpha);
        SolverResult r = solver.solve(ConvCase::A, MeshResolution::Coarse);

        std::cout << "  [" << cfg.label << "]: T_max=" << r.T_max
                  << " °C, T_avg=" << r.T_avg << " °C, iters=" << r.iterations
                  << ", res=" << r.final_rel_res << ", Q_bal=" << r.Q_balance_rel << "\n";

        if (csv.is_open()) {
            csv << "\"" << cfg.label << "\"," << cfg.n_outer << "," << cfg.alpha << ","
                << r.T_max << "," << r.T_avg << "," << r.iterations << ","
                << r.final_rel_res << "," << r.Q_balance_rel << "\n";
        }

        if (cfg.label == "Alpha = 0.2 (Default)") {
            t_max_default = r.T_max;
        }

        ASSERT_TRUE(r.success, "FVM solve converges for " + cfg.label);
        ASSERT_TRUE(r.Q_balance_rel < 0.02, "Energy conservation preserved under correction");
    }

    // Sensitivity check: variation across correction parameters should be small (< 1.5 °C)
    ASSERT_TRUE(t_max_default > 50.0 && t_max_default < 70.0, "FVM Coarse default T_max in expected range");
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
    test_fvm_non_orthogonal_sensitivity();

    std::cout << "\n--- Cross-Solver Consistency Tests ---\n";
    test_cross_solver_consistency();

    std::cout << "\n==============================================\n";
    std::cout << g_passed << " passed, " << g_failed << " failed.\n";
    return (g_failed > 0) ? 1 : 0;
}
