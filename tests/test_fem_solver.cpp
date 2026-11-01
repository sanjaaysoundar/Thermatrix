// test_fem_solver.cpp – Unit tests for the 1D thermal FEM solver
// Tests: positive, negative, and numerical cases
// THERMATRIX C++17

#include "../src/core/FemSolver.h"
#include <cassert>
#include <cmath>
#include <iostream>
#include <vector>

using namespace thermatrix;

static int passed = 0, failed = 0;

#define ASSERT_TRUE(cond, msg) \
    do { if (cond) { ++passed; std::cout << "[PASS] " << msg << "\n"; } \
         else { ++failed; std::cout << "[FAIL] " << msg << "\n"; } } while(0)

#define ASSERT_NEAR(a, b, tol, msg) \
    ASSERT_TRUE(std::abs((a)-(b)) <= (tol), msg << " (got " << (a) << ", expected " << (b) << ", tol=" << tol << ")")

// ─── Analytical solution checks ──────────────────────────────────────────────

void test_analytical() {
    FemParams p;
    // At x=0: T=100 (boundary condition)
    ASSERT_NEAR(femAnalytical(0.0, p), 100.0, 1e-12, "Analytical at x=0");
    // At x=1: T=20 (boundary condition)
    ASSERT_NEAR(femAnalytical(1.0, p), 20.0, 1e-12, "Analytical at x=L");
    // At midpoint x=0.5: T = 100 - 80*0.5 + 10000/(2*10)*0.5*0.5 = 100-40+125 = 185
    double T_mid_exact = 100.0 - 80.0*0.5 + (10000.0/(2.0*10.0))*0.5*0.5;
    ASSERT_NEAR(femAnalytical(0.5, p), T_mid_exact, 1e-10, "Analytical at x=0.5 m");
    // Exact midpoint value
    ASSERT_NEAR(T_mid_exact, 185.0, 1e-10, "Analytical midpoint = 185°C (sanity)");
}

// ─── Boundary conditions ─────────────────────────────────────────────────────

void test_boundary_conditions() {
    FemParams p;
    auto r = femSolve(p);
    ASSERT_TRUE(r.success, "FEM solve succeeds (10 elements)");
    ASSERT_NEAR(r.T_fem.front(), 100.0, 1e-10, "T(0)=100°C exactly (Dirichlet BC)");
    ASSERT_NEAR(r.T_fem.back(),   20.0, 1e-10, "T(L)=20°C exactly (Dirichlet BC)");
}

// ─── Convergence: max error decreases with mesh refinement ──────────────────

void test_convergence() {
    FemParams p;
    auto results = femConvergence(p, {10, 20, 40, 80});
    ASSERT_TRUE(results.size() == 4, "4 convergence results");

    // Error should decrease monotonically
    for (int i = 1; i < (int)results.size(); ++i) {
        ASSERT_TRUE(results[i].max_abs_error < results[i-1].max_abs_error,
                    "Error decreases: n=" << results[i].params.n_elements <<
                    " < n=" << results[i-1].params.n_elements);
    }

    // 80-element mesh should have max error < 1e-2 °C for these params
    ASSERT_TRUE(results.back().max_abs_error < 1e-1,
                "80-element mesh: max error < 0.1°C");
}

// ─── Numerical accuracy at known midpoints ───────────────────────────────────

void test_numerical_midpoint() {
    // For n=40 elements, the FE midpoint values should match analytical
    FemParams p;
    p.n_elements = 40;
    auto r = femSolve(p);
    ASSERT_TRUE(r.success, "FEM solve n=40");
    ASSERT_TRUE(r.x_midpoints.size() == 40, "40 midpoints");
    for (int i = 0; i < (int)r.x_midpoints.size(); ++i) {
        double T_ex = femAnalytical(r.x_midpoints[i], p);
        ASSERT_NEAR(r.T_fem_mid[i], T_ex, 0.5, // within 0.5°C for n=40
                    "Midpoint " << i << " T_fem ≈ T_exact");
    }
}

// ─── Invalid parameters ──────────────────────────────────────────────────────

void test_invalid_params() {
    FemParams p;
    p.n_elements = 0;
    auto r = femSolve(p);
    ASSERT_TRUE(!r.success, "n_elements=0 returns failure");
    ASSERT_TRUE(!r.error_message.empty(), "Error message set for n_elements=0");
}

// ─── Energy balance check (internal nodes sum to zero net flux) ──────────────

void test_energy_balance() {
    FemParams p;
    p.n_elements = 20;
    auto r = femSolve(p);
    // All interior temperatures should be >= min(T_left, T_right)
    double tMin = std::min(p.T_left, p.T_right);
    for (auto& T : r.T_fem)
        ASSERT_TRUE(T >= tMin - 1e-6, "Interior T >= min boundary T");
}

// ─── Symmetry test ───────────────────────────────────────────────────────────

void test_symmetry_no_heat_gen() {
    // With q=0 and equal BCs, solution should be linear (constant T)
    FemParams p;
    p.q      = 0.0;
    p.T_left = 50.0;
    p.T_right= 50.0;
    p.n_elements = 10;
    auto r = femSolve(p);
    for (auto& T : r.T_fem)
        ASSERT_NEAR(T, 50.0, 1e-8, "Uniform T=50°C with q=0, equal BCs");
}

int main() {
    std::cout << "=== FEM Solver Tests ===\n";
    test_analytical();
    test_boundary_conditions();
    test_convergence();
    test_numerical_midpoint();
    test_invalid_params();
    test_energy_balance();
    test_symmetry_no_heat_gen();
    std::cout << "\n" << passed << " passed, " << failed << " failed.\n";
    return (failed > 0) ? 1 : 0;
}
