// test_t3d_verification.cpp  –  Verification & Validation Test Suite (FIX 4)
// THERMATRIX C++17

#include "../src/thermal3d/Verification3D.h"
#include <iostream>
#include <iomanip>
#include <cmath>

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

void test_1d_slab() {
    Verification3D v;
    double err_fem = 0.0, err_fdm = 0.0, err_fvm = 0.0;
    bool ok = v.run_1d_slab_test(err_fem, err_fdm, err_fvm);

    std::cout << "  1D Slab Relative L2 Errors:\n"
              << "    FEM: " << (err_fem * 100.0) << "%\n"
              << "    FDM: " << (err_fdm * 100.0) << "%\n"
              << "    FVM: " << (err_fvm * 100.0) << "%\n";

    ASSERT_TRUE(ok, "1D slab test passes for all three solvers");
    ASSERT_TRUE(err_fem < 0.005, "FEM 1D slab error < 0.5%");
    ASSERT_TRUE(err_fdm < 0.005, "FDM 1D slab error < 0.5%");
    ASSERT_TRUE(err_fvm < 0.005, "FVM 1D slab error < 0.5%");
}

void test_mms_convergence() {
    Verification3D v;
    double p_fem = 0.0, p_fdm = 0.0, p_fvm = 0.0;
    bool ok = v.run_mms_test(p_fem, p_fdm, p_fvm);

    std::cout << "  Manufactured Solution Observed Order p:\n"
              << "    FEM: p = " << p_fem << "\n"
              << "    FDM: p = " << p_fdm << "\n"
              << "    FVM: p = " << p_fvm << "\n";

    ASSERT_TRUE(ok, "Manufactured solution convergence test succeeds");
    ASSERT_TRUE(p_fem >= 1.5, "FEM observed convergence order >= 1.5 (asymptotic O(h^2))");
    ASSERT_TRUE(p_fdm >= 1.5, "FDM observed convergence order >= 1.5 (asymptotic O(h^2))");
    ASSERT_TRUE(p_fvm >= 1.2, "FVM observed convergence order >= 1.2");
}

void test_gci_and_probes() {
    Verification3D v;
    auto suite = v.run_full_verification("results/validation_results.csv");
    suite.export_csv("THERMATRIX_ASSIGNMENT_2/Results/validation_results.csv");

    ASSERT_TRUE(suite.probes.size() == 30, "Exactly 30 fixed probe points sampled");
    std::cout << "  Sampled 30 Probe Temperatures (Medium Level):\n";
    for (size_t i = 0; i < 5; ++i) {
        const auto& p = suite.probes[i];
        std::cout << "    " << p.name << " at (" << p.coord.x*1e3 << ", " << p.coord.y*1e3 << ", " << p.coord.z*1e3
                  << ") mm: FEM=" << p.T_fem << " C, FDM=" << p.T_fdm << " C, FVM=" << p.T_fvm
                  << " C, Ref=" << p.T_ref << " C\n";
    }
    std::cout << "    ... (30 probes total)\n";

    std::cout << "  Probe RMSE vs Ref:\n"
              << "    FEM: " << suite.rmse_fem << " C\n"
              << "    FDM: " << suite.rmse_fdm << " C\n"
              << "    FVM: " << suite.rmse_fvm << " C\n";

    ASSERT_TRUE(suite.rmse_fem < 2.0, "FEM probe RMSE < 2.0 °C vs consensus reference");
    ASSERT_TRUE(suite.rmse_fdm < 2.5, "FDM probe RMSE < 2.5 °C vs consensus reference");
    ASSERT_TRUE(suite.rmse_fvm < 2.0, "FVM probe RMSE < 2.0 °C vs consensus reference");

    std::cout << "\n  Pairwise RMSE Between Solver Pairs (Fine Level):\n"
              << "    FEM vs FDM: " << suite.rmse_fem_fdm << " C\n"
              << "    FEM vs FVM: " << suite.rmse_fem_fvm << " C\n"
              << "    FDM vs FVM: " << suite.rmse_fdm_fvm << " C\n";

    std::cout << "\n  Solver RMSE Versus Consensus Reference:\n"
              << "    FEM vs Consensus:       " << suite.rmse_fem_vs_consensus << " C\n"
              << "    FDM vs Consensus:       " << suite.rmse_fdm_vs_consensus << " C\n"
              << "    FVM vs Consensus:       " << suite.rmse_fvm_vs_consensus << " C\n"
              << "    Finer FEM vs Consensus: " << suite.rmse_finer_fem_vs_consensus << " C (labelled cross-check)\n";

    std::cout << "\n  Consensus Extrapolated Temperatures:\n"
              << "    T_max: Extrapolated FEM=" << suite.tmax_extrap_fem << " C, FDM=" << suite.tmax_extrap_fdm
              << " C, FVM=" << suite.tmax_extrap_fvm << " C\n"
              << "           Consensus Mean=" << suite.tmax_consensus << " C, Spread=" << suite.tmax_spread << " C\n"
              << "           Finer FEM CrossCheck=" << suite.tmax_fem_finer << " C (diff=" << std::abs(suite.tmax_fem_finer - suite.tmax_consensus) << " C)\n"
              << "    T_avg: Extrapolated FEM=" << suite.tavg_extrap_fem << " C, FDM=" << suite.tavg_extrap_fdm
              << " C, FVM=" << suite.tavg_extrap_fvm << " C\n"
              << "           Consensus Mean=" << suite.tavg_consensus << " C, Spread=" << suite.tavg_spread << " C\n"
              << "           Finer FEM CrossCheck=" << suite.tavg_fem_finer << " C (diff=" << std::abs(suite.tavg_fem_finer - suite.tavg_consensus) << " C)\n";

    ASSERT_TRUE(suite.tmax_spread < 3.0, "Consensus T_max spread across methods < 3.0 °C");
    ASSERT_TRUE(suite.tavg_spread < 1.5, "Consensus T_avg spread across methods < 1.5 °C");

    ASSERT_TRUE(!suite.gci_results.empty(), "GCI results populated");
    std::cout << "\n  Grid Convergence Index (GCI) Summary:\n";
    for (const auto& g : suite.gci_results) {
        std::cout << "    " << std::left << std::setw(4) << g.solver
                  << " " << std::setw(6) << g.quantity
                  << " | Coarse=" << g.val_coarse
                  << " | Med=" << g.val_medium
                  << " | Fine=" << g.val_fine
                  << " | Extrap=" << g.val_extrap
                  << " | GCI_21=" << g.gci_21_pct << "%"
                  << " | Order p=" << g.observed_p
                  << " | Asymp=" << g.asymptotic_ratio << "\n";

        ASSERT_TRUE(g.gci_21_pct > 0.0 && g.gci_21_pct < 10.0, "GCI_21 is reasonable (< 10%)");
    }
}

int main() {
    std::cout << "=== THERMATRIX 3D Verification & Validation Suite ===\n\n";

    std::cout << "--- 1. 1D Slab Analytical Test ---\n";
    test_1d_slab();

    std::cout << "\n--- 2. Manufactured Solution Convergence Order Test ---\n";
    test_mms_convergence();

    std::cout << "\n--- 3. 30 Fixed Probe Points & GCI ---\n";
    test_gci_and_probes();

    std::cout << "\n======================================================\n";
    std::cout << g_passed << " passed, " << g_failed << " failed.\n";
    return (g_failed > 0) ? 1 : 0;
}
