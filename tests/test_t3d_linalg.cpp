// test_t3d_linalg.cpp  –  THERMATRIX Phase 1
// Unit tests for SparseCsr and LinearSolvers.
// Tests:
//   1.  CSR builder: triplet accumulation, duplicate summing
//   2.  CSR structural symmetry check
//   3.  SpMV correctness on a known 3×3 matrix
//   4.  PCG (Jacobi) on a 1D Laplacian – converges to known solution
//   5.  PCG (IC0) on the same system – converges
//   6.  IC0 gives strictly fewer iterations than Jacobi for the Laplacian
//   7.  SOR converges on the same system
//   8.  SSOR converges on the same system
//   9.  Peak-memory helper returns a positive value
//  10.  SolverResult CSV row matches header column count

#include "../src/thermal3d/SparseCsr.h"
#include "../src/thermal3d/LinearSolvers.h"
#include "../src/thermal3d/SolverResult.h"

#include <cassert>
#include <cmath>
#include <iostream>
#include <sstream>
#include <vector>
#include <string>

using namespace thermatrix::t3d;

// ---------------------------------------------------------------------------
// Minimal test harness (same style as existing THERMATRIX tests)
// ---------------------------------------------------------------------------
static int g_passed = 0, g_failed = 0;

#define ASSERT_TRUE(cond, msg)                                                 \
    do {                                                                       \
        if (cond) { ++g_passed; std::cout << "[PASS] " << msg << "\n"; }     \
        else       { ++g_failed; std::cout << "[FAIL] " << msg << "\n"; }    \
    } while (0)

#define ASSERT_NEAR(a, b, tol, msg)                                            \
    ASSERT_TRUE(std::abs((double)(a) - (double)(b)) <= (tol),                 \
                msg << "  (got=" << (a) << " exp=" << (b) << " tol=" << tol << ")")

// ---------------------------------------------------------------------------
// Build a 1D Laplacian system of size n (Poisson -u'' = f with Dirichlet BCs)
// A is the (n-1)×(n-1) tridiagonal interior matrix:
//   diag = 2, off-diag = -1  (then multiplied by n² to match h=1/n)
// RHS b chosen so the exact solution is T[i] = sin(π * i/n).
// We use the simpler manufactured solution instead: T_exact[i] = i*(n-i)/n².
// ---------------------------------------------------------------------------
static SparseCsr make_laplacian_1d(int n, std::vector<double>& b_out,
                                           std::vector<double>& x_exact_out)
{
    // Interior nodes: 1 … n-1  →  indices 0 … n-2  (m = n-1 unknowns)
    int m = n - 1;
    SparseBuilder bld(m, m);

    // Scaled by n² so that the matrix is h⁻² * tridiag(-1,2,-1)
    double hn2 = static_cast<double>(n) * n;

    for (int i = 0; i < m; ++i) {
        bld.add(i, i,  2.0 * hn2);
        if (i > 0)   bld.add(i, i - 1, -1.0 * hn2);
        if (i < m-1) bld.add(i, i + 1, -1.0 * hn2);
    }
    SparseCsr A = bld.finalise();

    // Exact solution: T[i] = (i+1)*(n - i - 1)/n²  (parabola, zero at BCs)
    // => -u'' = -d²/dx²[ x(1-x) ] = 2  → f = 2 everywhere
    // With scaling: f_scaled = 2 (uniform RHS)
    x_exact_out.resize(m);
    b_out.resize(m);
    for (int i = 0; i < m; ++i) {
        double xi = static_cast<double>(i + 1) / n;
        x_exact_out[i] = xi * (1.0 - xi);
        // RHS = 2 everywhere (from -u'' = 2, boundary corrections = 0 since u(0)=u(1)=0)
        b_out[i] = 2.0;
    }

    return A;
}

// ---------------------------------------------------------------------------
// Test 1: CSR builder – basic construction and duplicate summing
// ---------------------------------------------------------------------------
void test_csr_builder() {
    SparseBuilder bld(3, 3);
    // Build:  A = [4  -1   0]
    //             [-1  4  -1]
    //             [0  -1   4]
    bld.add(0, 0,  4.0);
    bld.add(1, 1,  4.0);
    bld.add(2, 2,  4.0);
    bld.add(0, 1, -1.0);
    bld.add(1, 0, -1.0);
    bld.add(1, 2, -1.0);
    bld.add(2, 1, -1.0);

    // Add duplicate: A[0][0] gets an extra 1.0 (sum should be 5.0)
    bld.add(0, 0, 1.0);

    SparseCsr A = bld.finalise();

    ASSERT_TRUE(A.nrows == 3 && A.ncols == 3, "CSR builder: dimensions 3×3");
    ASSERT_TRUE(A.nnz() == 7, "CSR builder: 7 entries (after merging duplicates)");
    ASSERT_NEAR(A.get(0, 0), 5.0, 1e-12, "CSR builder: A[0][0] = 4+1 = 5 (dup summed)");
    ASSERT_NEAR(A.get(1, 1), 4.0, 1e-12, "CSR builder: A[1][1] = 4");
    ASSERT_NEAR(A.get(0, 1),-1.0, 1e-12, "CSR builder: A[0][1] = -1");
    ASSERT_NEAR(A.get(1, 0),-1.0, 1e-12, "CSR builder: A[1][0] = -1");
    ASSERT_NEAR(A.get(2, 2), 4.0, 1e-12, "CSR builder: A[2][2] = 4");
    ASSERT_NEAR(A.get(0, 2), 0.0, 1e-12, "CSR builder: A[0][2] = 0 (not stored)");
}

// ---------------------------------------------------------------------------
// Test 2: CSR symmetry check
// ---------------------------------------------------------------------------
void test_csr_symmetry() {
    // Symmetric matrix
    SparseBuilder bld(3, 3);
    bld.add(0, 0, 2.0); bld.add(1, 1, 2.0); bld.add(2, 2, 2.0);
    bld.add(0, 1,-1.0); bld.add(1, 0,-1.0);
    bld.add(1, 2,-1.0); bld.add(2, 1,-1.0);
    SparseCsr As = bld.finalise();
    ASSERT_TRUE(As.is_symmetric(), "Symmetry check: symmetric tridiagonal passes");

    // Non-symmetric matrix
    SparseBuilder bld2(2, 2);
    bld2.add(0, 0, 2.0); bld2.add(1, 1, 2.0);
    bld2.add(0, 1, 1.0); bld2.add(1, 0, 3.0);  // 1 ≠ 3
    SparseCsr An = bld2.finalise();
    ASSERT_TRUE(!An.is_symmetric(), "Symmetry check: asymmetric matrix fails");
}

// ---------------------------------------------------------------------------
// Test 3: SpMV correctness
// ---------------------------------------------------------------------------
void test_spmv() {
    // A = [3  -1   0]    x = [1, 2, 3]^T    y = [1, 1, 5]
    //     [-1  3  -1]
    //     [0  -1   3]
    SparseBuilder bld(3, 3);
    bld.add(0, 0,  3.0); bld.add(0, 1, -1.0);
    bld.add(1, 0, -1.0); bld.add(1, 1,  3.0); bld.add(1, 2, -1.0);
    bld.add(2, 1, -1.0); bld.add(2, 2,  3.0);
    SparseCsr A = bld.finalise();

    std::vector<double> x = {1.0, 2.0, 3.0};
    std::vector<double> y;
    A.spmv(x, y);

    ASSERT_NEAR(y[0], 3*1 - 1*2,           1e-12, "SpMV y[0]");
    ASSERT_NEAR(y[1], -1*1 + 3*2 - 1*3,    1e-12, "SpMV y[1]");
    ASSERT_NEAR(y[2], -1*2 + 3*3,           1e-12, "SpMV y[2]");
}

// ---------------------------------------------------------------------------
// Test 4: PCG with Jacobi preconditioner – 1D Laplacian
// ---------------------------------------------------------------------------
void test_pcg_jacobi() {
    int n = 50;  // mesh intervals → 49 interior DOFs
    std::vector<double> b, x_exact;
    SparseCsr A = make_laplacian_1d(n, b, x_exact);

    std::vector<double> x(A.nrows, 0.0);
    auto precond = make_jacobi_precond(A);
    SolverParams params;
    params.rel_tol  = 1e-10;
    params.max_iter = 2000;

    auto res = pcg_solve(A, b, x, precond, params, "PCG-Jacobi");

    ASSERT_TRUE(res.converged, "PCG-Jacobi converges on 1D Laplacian n=50");
    ASSERT_TRUE(res.iterations > 0, "PCG-Jacobi: iterations > 0");

    // Check solution accuracy vs exact
    double max_err = 0.0;
    for (int i = 0; i < A.nrows; ++i)
        max_err = std::max(max_err, std::abs(x[i] - x_exact[i]));
    ASSERT_TRUE(max_err < 1e-6, "PCG-Jacobi: max solution error < 1e-6"
                << "  (err=" << max_err << ")");

    std::cout << "    PCG-Jacobi: " << res.iterations << " iters, "
              << "relres=" << res.final_rel_res << ", "
              << res.solve_ms << " ms\n";
}

// ---------------------------------------------------------------------------
// Test 5: PCG with IC0 preconditioner
// ---------------------------------------------------------------------------
int g_ic0_iters = 0;
int g_jac_iters = 0;

void test_pcg_ic0() {
    int n = 50;
    std::vector<double> b, x_exact;
    SparseCsr A = make_laplacian_1d(n, b, x_exact);

    std::vector<double> x(A.nrows, 0.0);
    auto precond = make_ic0_precond(A);
    SolverParams params;
    params.rel_tol  = 1e-10;
    params.max_iter = 2000;

    auto res = pcg_solve(A, b, x, precond, params, "PCG-IC0");

    ASSERT_TRUE(res.converged, "PCG-IC0 converges on 1D Laplacian n=50");

    double max_err = 0.0;
    for (int i = 0; i < A.nrows; ++i)
        max_err = std::max(max_err, std::abs(x[i] - x_exact[i]));
    ASSERT_TRUE(max_err < 1e-6, "PCG-IC0: max solution error < 1e-6"
                << "  (err=" << max_err << ")");

    std::cout << "    PCG-IC0:    " << res.iterations << " iters, "
              << "relres=" << res.final_rel_res << ", "
              << res.solve_ms << " ms\n";

    g_ic0_iters = res.iterations;
}

// ---------------------------------------------------------------------------
// Test 6: IC0 gives fewer iterations than Jacobi (on same system)
// ---------------------------------------------------------------------------
void test_ic0_beats_jacobi() {
    int n = 100;  // bigger system to make difference clear
    std::vector<double> b, x_exact;
    SparseCsr A = make_laplacian_1d(n, b, x_exact);

    SolverParams params;
    params.rel_tol  = 1e-10;
    params.max_iter = 5000;

    std::vector<double> x_jac(A.nrows, 0.0);
    auto precond_jac = make_jacobi_precond(A);
    auto res_jac = pcg_solve(A, b, x_jac, precond_jac, params, "PCG-Jacobi");

    std::vector<double> x_ic0(A.nrows, 0.0);
    auto precond_ic0 = make_ic0_precond(A);
    auto res_ic0 = pcg_solve(A, b, x_ic0, precond_ic0, params, "PCG-IC0");

    std::cout << "    n=100 Laplacian: Jacobi=" << res_jac.iterations
              << " iters, IC0=" << res_ic0.iterations << " iters\n";

    ASSERT_TRUE(res_jac.converged, "Jacobi PCG converges for n=100");
    ASSERT_TRUE(res_ic0.converged, "IC0 PCG converges for n=100");
    ASSERT_TRUE(res_ic0.iterations < res_jac.iterations,
                "IC0 uses fewer iterations than Jacobi (n=100 Laplacian)"
                << " Jacobi=" << res_jac.iterations
                << " IC0=" << res_ic0.iterations);
}

// ---------------------------------------------------------------------------
// Test 7: SOR converges
// ---------------------------------------------------------------------------
void test_sor() {
    int n = 30;
    std::vector<double> b, x_exact;
    SparseCsr A = make_laplacian_1d(n, b, x_exact);

    std::vector<double> x(A.nrows, 0.0);
    SolverParams params;
    params.rel_tol  = 1e-10;
    params.max_iter = 5000;
    params.sor_omega = 1.0;  // Gauss-Seidel

    auto res = sor_solve(A, b, x, params);

    ASSERT_TRUE(res.converged, "SOR (omega=1.0) converges on 1D Laplacian n=30");

    double max_err = 0.0;
    for (int i = 0; i < A.nrows; ++i)
        max_err = std::max(max_err, std::abs(x[i] - x_exact[i]));
    ASSERT_TRUE(max_err < 1e-4, "SOR: max solution error < 1e-4"
                << "  (err=" << max_err << ")");

    std::cout << "    SOR (omega=1.0): " << res.iterations << " iters, "
              << "relres=" << res.final_rel_res << "\n";
}

// ---------------------------------------------------------------------------
// Test 8: SSOR converges
// ---------------------------------------------------------------------------
void test_ssor() {
    int n = 30;
    std::vector<double> b, x_exact;
    SparseCsr A = make_laplacian_1d(n, b, x_exact);

    std::vector<double> x(A.nrows, 0.0);
    SolverParams params;
    params.rel_tol   = 1e-10;
    params.max_iter  = 5000;
    params.sor_omega = 1.3;

    auto res = ssor_solve(A, b, x, params);

    ASSERT_TRUE(res.converged, "SSOR (omega=1.3) converges on 1D Laplacian n=30");

    double max_err = 0.0;
    for (int i = 0; i < A.nrows; ++i)
        max_err = std::max(max_err, std::abs(x[i] - x_exact[i]));
    ASSERT_TRUE(max_err < 1e-4, "SSOR: max solution error < 1e-4"
                << "  (err=" << max_err << ")");

    std::cout << "    SSOR (omega=1.3): " << res.iterations << " iters, "
              << "relres=" << res.final_rel_res << "\n";
}

// ---------------------------------------------------------------------------
// Test 9: peak_memory_bytes returns a positive value
// ---------------------------------------------------------------------------
void test_peak_memory() {
    std::size_t mem = peak_memory_bytes();
    ASSERT_TRUE(mem > 0, "peak_memory_bytes() > 0  (got " << mem << " bytes)");
}

// ---------------------------------------------------------------------------
// Test 10: SolverResult CSV row matches header column count
// ---------------------------------------------------------------------------
void test_solver_result_csv() {
    SolverResult r;
    r.solver_name    = "FEM";
    r.case_label     = "A";
    r.success        = true;
    r.n_dof          = 1234;
    r.n_nnz          = 9876;
    r.n_elements     = 500;
    r.linear_solver  = "PCG-IC0";
    r.iterations     = 42;
    r.final_rel_res  = 3.14e-11;
    r.t_mesh_ms      = 10.5;
    r.t_assemble_ms  = 20.3;
    r.t_solve_ms     = 5.1;
    r.t_total_ms     = 36.0;
    r.T_min          = 25.0;
    r.T_max          = 85.3;
    r.T_avg          = 45.0;
    r.T_src          = 80.1;
    r.Q_in           = 10.0;
    r.Q_conv         = 9.95;
    r.Q_balance_rel  = 0.005;
    r.peak_mem_bytes = 1024 * 1024;

    auto header = SolverResult::csv_header();
    auto row    = r.to_csv_row();

    // Count commas
    auto count_commas = [](const std::string& s) {
        int c = 0; for (char ch : s) if (ch == ',') ++c; return c;
    };
    int hc = count_commas(header);
    int rc = count_commas(row);

    ASSERT_TRUE(hc == rc, "CSV row comma count matches header"
                << "  header=" << hc << " row=" << rc);
    ASSERT_TRUE(!row.empty(), "CSV row is non-empty");
}

// ---------------------------------------------------------------------------
// Test 11: check_symmetry and check_positive_diagonal
// ---------------------------------------------------------------------------
void test_check_symmetry_and_pos_diag() {
    std::vector<double> b, x_exact;
    SparseCsr A = make_laplacian_1d(10, b, x_exact);
    double sym = check_symmetry(A);
    ASSERT_NEAR(sym, 0.0, 1e-12, "check_symmetry returns 0 for Laplacian");
    double min_d = 0.0;
    bool pos = check_positive_diagonal(A, &min_d);
    ASSERT_TRUE(pos, "check_positive_diagonal returns true for Laplacian");
    ASSERT_TRUE(min_d > 0.0, "min diagonal is positive");

    // Asymmetric matrix: perturb entry (0, 1)
    SparseBuilder asym_bld(3, 3);
    asym_bld.add(0, 0, 4.0); asym_bld.add(0, 1, 1.0); asym_bld.add(0, 2, 0.0);
    asym_bld.add(1, 0, 2.0); asym_bld.add(1, 1, 5.0); asym_bld.add(1, 2, 1.0);
    asym_bld.add(2, 0, 0.0); asym_bld.add(2, 1, 1.0); asym_bld.add(2, 2, 6.0);
    SparseCsr A_asym = asym_bld.finalise();
    double asym_err = check_symmetry(A_asym);
    ASSERT_TRUE(asym_err > 0.1, "check_symmetry detects asymmetry correctly");
}

// ---------------------------------------------------------------------------
// Test 12: BiCGSTAB linear solver
// ---------------------------------------------------------------------------
void test_bicgstab() {
    int n = 20;
    std::vector<double> b, x_exact;
    SparseCsr A = make_laplacian_1d(n, b, x_exact);
    int m = n - 1;
    std::vector<double> x(m, 0.0);
    auto precond = make_jacobi_precond(A);
    SolverParams sp; sp.rel_tol = 1e-10; sp.max_iter = 1000;
    LinearSolveResult res = bicgstab_solve(A, b, x, precond, sp);
    ASSERT_TRUE(res.converged, "BiCGSTAB converged on 1D Laplacian");
    ASSERT_TRUE(res.final_rel_res <= 1e-10, "BiCGSTAB final relative residual <= 1e-10");

    double max_err = 0.0;
    for (int i = 0; i < m; ++i) max_err = std::max(max_err, std::abs(x[i] - x_exact[i]));
    ASSERT_TRUE(max_err < 1e-4, "BiCGSTAB solution matches exact solution");

    // Test on non-symmetric system
    SparseBuilder bld(2, 2);
    bld.add(0, 0, 4.0); bld.add(0, 1, 1.0);
    bld.add(1, 0, 2.0); bld.add(1, 1, 3.0);
    SparseCsr A_ns = bld.finalise();
    std::vector<double> b_ns = {5.0, 5.0}; // exact x = [1, 1]
    std::vector<double> x_ns = {0.0, 0.0};
    auto precond_ns = make_jacobi_precond(A_ns);
    LinearSolveResult res_ns = bicgstab_solve(A_ns, b_ns, x_ns, precond_ns, sp);
    ASSERT_TRUE(res_ns.converged, "BiCGSTAB converges on non-symmetric 2x2 system");
    ASSERT_NEAR(x_ns[0], 1.0, 1e-8, "BiCGSTAB non-symmetric x[0] == 1");
    ASSERT_NEAR(x_ns[1], 1.0, 1e-8, "BiCGSTAB non-symmetric x[1] == 1");
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------
int main() {
    std::cout << "=== t3d Linear Algebra Tests ===\n\n";

    std::cout << "--- Test 1: CSR builder ---\n";
    test_csr_builder();

    std::cout << "\n--- Test 2: CSR symmetry ---\n";
    test_csr_symmetry();

    std::cout << "\n--- Test 3: SpMV ---\n";
    test_spmv();

    std::cout << "\n--- Test 4: PCG-Jacobi ---\n";
    test_pcg_jacobi();

    std::cout << "\n--- Test 5: PCG-IC0 ---\n";
    test_pcg_ic0();

    std::cout << "\n--- Test 6: IC0 beats Jacobi ---\n";
    test_ic0_beats_jacobi();

    std::cout << "\n--- Test 7: SOR ---\n";
    test_sor();

    std::cout << "\n--- Test 8: SSOR ---\n";
    test_ssor();

    std::cout << "\n--- Test 9: Peak memory ---\n";
    test_peak_memory();

    std::cout << "\n--- Test 10: SolverResult CSV ---\n";
    test_solver_result_csv();

    std::cout << "\n--- Test 11: Symmetry & Positive Diagonal ---\n";
    test_check_symmetry_and_pos_diag();

    std::cout << "\n--- Test 12: BiCGSTAB Solver ---\n";
    test_bicgstab();

    std::cout << "\n========================================\n";
    std::cout << g_passed << " passed,  " << g_failed << " failed.\n";
    return (g_failed > 0) ? 1 : 0;
}
