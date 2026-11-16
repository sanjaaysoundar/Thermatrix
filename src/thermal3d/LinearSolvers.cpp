// LinearSolvers.cpp  –  THERMATRIX Phase 1
// Implementation of PCG (Jacobi + IC0 preconditioners), SOR, SSOR.

#include "LinearSolvers.h"

#include <cassert>
#include <cmath>
#include <stdexcept>
#include <chrono>
#include <numeric>
#include <vector>

namespace thermatrix {
namespace t3d {

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------
namespace {

using Clock = std::chrono::steady_clock;

double dot(const std::vector<double>& a, const std::vector<double>& b) {
    assert(a.size() == b.size());
    double s = 0.0;
    for (std::size_t i = 0; i < a.size(); ++i) s += a[i] * b[i];
    return s;
}

double norm2(const std::vector<double>& v) {
    return std::sqrt(dot(v, v));
}

// r = b - A*x
void compute_residual(const SparseCsr& A,
                      const std::vector<double>& b,
                      const std::vector<double>& x,
                      std::vector<double>& r)
{
    r.resize(A.nrows);
    A.spmv(x, r);                       // r = A*x
    for (int i = 0; i < A.nrows; ++i)
        r[i] = b[i] - r[i];            // r = b - A*x
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// Jacobi preconditioner
// ---------------------------------------------------------------------------
Preconditioner make_jacobi_precond(const SparseCsr& A) {
    std::vector<double> inv_diag = A.diagonal();
    for (auto& d : inv_diag) {
        if (std::abs(d) < 1e-300)
            throw std::runtime_error("Jacobi: zero diagonal encountered");
        d = 1.0 / d;
    }
    return [inv_diag](const std::vector<double>& z,
                       std::vector<double>& out) {
        out.resize(z.size());
        for (std::size_t i = 0; i < z.size(); ++i)
            out[i] = inv_diag[i] * z[i];
    };
}

// ---------------------------------------------------------------------------
// IC(0) preconditioner
//
// Strategy:
//   1. Extract lower-triangular CSR (L_row_ptr, L_col, L_val).
//   2. Build an upper-triangular CSR (Lᵀ) simultaneously – needed for
//      the backward solve.
//   3. Perform the IC(0) factorisation in-place on L_val.
//      Lᵀ_val shares the same numerical values via a cross-index array.
//   4. Apply functor:
//        Forward:   solve L  y = z  (lower-tri forward substitution)
//        Backward:  solve Lᵀ x = y  (upper-tri backward substitution)
// ---------------------------------------------------------------------------
Preconditioner make_ic0_precond(const SparseCsr& A, int* n_fallbacks) {
    if (n_fallbacks) *n_fallbacks = 0;
    if (A.nrows != A.ncols)
        throw std::runtime_error("IC0: matrix must be square");

    int n = A.nrows;

    // ---- 1. Build lower-triangular CSR pattern ----
    // Count lower entries per row
    std::vector<int> L_row_ptr(n + 1, 0);
    for (int i = 0; i < n; ++i)
        for (int p = A.row_ptr[i]; p < A.row_ptr[i + 1]; ++p)
            if (A.col_idx[p] <= i) L_row_ptr[i + 1]++;
    for (int i = 0; i < n; ++i) L_row_ptr[i + 1] += L_row_ptr[i];

    int nnz_L = L_row_ptr[n];
    std::vector<int>    L_col(nnz_L);
    std::vector<double> L_val(nnz_L);

    {
        std::vector<int> fp(L_row_ptr.begin(), L_row_ptr.end());
        for (int i = 0; i < n; ++i) {
            for (int p = A.row_ptr[i]; p < A.row_ptr[i + 1]; ++p) {
                int j = A.col_idx[p];
                if (j <= i) {
                    int pos = fp[i]++;
                    L_col[pos] = j;
                    L_val[pos] = A.values[p];
                }
            }
        }
    }
    // Sort each row by column index (they may come unsorted from A)
    for (int i = 0; i < n; ++i) {
        // Simple insertion sort (rows are small)
        for (int p = L_row_ptr[i] + 1; p < L_row_ptr[i + 1]; ++p) {
            int    kc = L_col[p];
            double kv = L_val[p];
            int q = p - 1;
            while (q >= L_row_ptr[i] && L_col[q] > kc) {
                L_col[q + 1] = L_col[q];
                L_val[q + 1] = L_val[q];
                --q;
            }
            L_col[q + 1] = kc;
            L_val[q + 1] = kv;
        }
    }

    // Diagonal position in L
    std::vector<int> L_diag(n, -1);
    for (int i = 0; i < n; ++i) {
        for (int p = L_row_ptr[i]; p < L_row_ptr[i + 1]; ++p) {
            if (L_col[p] == i) { L_diag[i] = p; break; }
        }
        if (L_diag[i] < 0)
            throw std::runtime_error("IC0: missing diagonal at row " + std::to_string(i));
    }

    // ---- 2. Build upper-triangular CSR (Lᵀ) ----
    // Lᵀ[j][i] = L[i][j] for i >= j.
    // We store Lᵀ in CSR with cross-index into L_val so they share values.
    std::vector<int> Lt_row_ptr(n + 1, 0);
    for (int i = 0; i < n; ++i)
        for (int p = L_row_ptr[i]; p < L_row_ptr[i + 1]; ++p)
            Lt_row_ptr[L_col[p] + 1]++;  // Lᵀ row j = L col j
    for (int i = 0; i < n; ++i) Lt_row_ptr[i + 1] += Lt_row_ptr[i];

    int nnz_Lt = Lt_row_ptr[n];
    std::vector<int> Lt_col(nnz_Lt);       // column index in Lᵀ = row index in L
    std::vector<int> Lt_L_idx(nnz_Lt);    // index into L_val for the same value

    {
        std::vector<int> fp(Lt_row_ptr.begin(), Lt_row_ptr.end());
        for (int i = 0; i < n; ++i) {
            for (int p = L_row_ptr[i]; p < L_row_ptr[i + 1]; ++p) {
                int j = L_col[p];
                int pos = fp[j]++;
                Lt_col[pos]    = i;    // Lᵀ[j][i]
                Lt_L_idx[pos]  = p;    // L_val[p] is the shared value
            }
        }
    }
    // Sort each Lᵀ row by column (ascending)
    for (int j = 0; j < n; ++j) {
        for (int p = Lt_row_ptr[j] + 1; p < Lt_row_ptr[j + 1]; ++p) {
            int    kc  = Lt_col[p];
            int    ki  = Lt_L_idx[p];
            int q = p - 1;
            while (q >= Lt_row_ptr[j] && Lt_col[q] > kc) {
                Lt_col[q + 1]   = Lt_col[q];
                Lt_L_idx[q + 1] = Lt_L_idx[q];
                --q;
            }
            Lt_col[q + 1]   = kc;
            Lt_L_idx[q + 1] = ki;
        }
    }

    // ---- 3. IC(0) factorisation ----
    // For each row i = 0..n-1:
    //   For each j < i in sparsity(row i):
    //     L[i][j] = (A[i][j] - sum_{m < j, m in sparsity(i) and sparsity(j)} L[i][m] * L[j][m]) / L[j][j]
    //   L[i][i] = sqrt(A[i][i] - sum_{m < i, m in sparsity(i)} L[i][m]^2)
    std::vector<int> col_to_pos(n, -1);

    for (int i = 0; i < n; ++i) {
        // Register columns of row i
        for (int p = L_row_ptr[i]; p < L_row_ptr[i + 1]; ++p)
            col_to_pos[L_col[p]] = p;

        double sum_sq = 0.0;
        // For each off-diagonal column j < i in row i
        for (int p = L_row_ptr[i]; p < L_diag[i]; ++p) {
            int j = L_col[p];
            double s = L_val[p]; // initially A[i][j]

            // Iterate over columns m < j in row j
            for (int q = L_row_ptr[j]; q < L_diag[j]; ++q) {
                int m = L_col[q];
                int pos_im = col_to_pos[m];
                if (pos_im >= 0) {
                    s -= L_val[pos_im] * L_val[q];
                }
            }

            double Ljj = L_val[L_diag[j]];
            if (std::abs(Ljj) < 1e-300)
                throw std::runtime_error("IC0: near-zero pivot at row " + std::to_string(j));

            L_val[p] = s / Ljj;
            sum_sq += L_val[p] * L_val[p];
        }

        // Diagonal entry
        int p_diag = L_diag[i];
        double d = L_val[p_diag] - sum_sq;
        if (d <= 0.0) {
            // Add a small diagonal perturbation if necessary (modified IC)
            if (n_fallbacks) (*n_fallbacks)++;
            d = 1e-10 * L_val[p_diag];
        }
        L_val[p_diag] = std::sqrt(d);

        // Unregister columns of row i
        for (int p = L_row_ptr[i]; p < L_row_ptr[i + 1]; ++p)
            col_to_pos[L_col[p]] = -1;
    }

    // ---- 4. Return apply functor ----
    // Captures: L (row_ptr, col, val, diag) and Lᵀ (row_ptr, col, L_idx).
    // L_val is shared (Lᵀ looks up values via Lt_L_idx into L_val).
    return [n, L_row_ptr, L_col, L_val, L_diag,
               Lt_row_ptr, Lt_col, Lt_L_idx]
           (const std::vector<double>& z, std::vector<double>& out) {
        // --- Forward: solve L y = z ---
        std::vector<double> y(n);
        for (int i = 0; i < n; ++i) {
            double s = z[i];
            for (int p = L_row_ptr[i]; p < L_diag[i]; ++p)
                s -= L_val[p] * y[L_col[p]];
            y[i] = s / L_val[L_diag[i]];
        }

        // --- Backward: solve Lᵀ out = y ---
        // Lᵀ is upper-triangular (diagonal + above).
        // For row j in Lᵀ: Lᵀ[j][j]*out[j] + Σ_{k>j} Lᵀ[j][k]*out[k] = y[j]
        // → out[j] = (y[j] - Σ_{k>j} Lᵀ[j][k]*out[k]) / Lᵀ[j][j]
        // Lᵀ[j][j] = L[j][j] = L_val[L_diag[j]]
        out.resize(n);
        for (int j = n - 1; j >= 0; --j) {
            double s = y[j];
            // Off-diagonal upper part of row j in Lᵀ: entries k > j
            for (int p = Lt_row_ptr[j]; p < Lt_row_ptr[j + 1]; ++p) {
                int k = Lt_col[p];
                if (k > j)
                    s -= L_val[Lt_L_idx[p]] * out[k];
            }
            out[j] = s / L_val[L_diag[j]];
        }
    };
}

// ---------------------------------------------------------------------------
// PCG
// ---------------------------------------------------------------------------
LinearSolveResult pcg_solve(const SparseCsr& A,
                             const std::vector<double>& b,
                             std::vector<double>& x,
                             const Preconditioner& precond,
                             const SolverParams& params,
                             const std::string& method_name)
{
    int n = A.nrows;
    assert((int)b.size() == n);
    if ((int)x.size() != n) x.assign(n, 0.0);

    auto t0 = Clock::now();

    double norm_b = norm2(b);
    if (norm_b < 1e-300) {
        x.assign(n, 0.0);
        LinearSolveResult res;
        res.converged = true; res.iterations = 0; res.final_rel_res = 0.0;
        res.method = method_name;
        return res;
    }

    std::vector<double> r, z, p, Ap;
    compute_residual(A, b, x, r);

    precond(r, z);                          // z = M⁻¹ r
    p = z;
    double rz = dot(r, z);

    LinearSolveResult result;
    result.method = method_name;

    for (int iter = 0; iter < params.max_iter; ++iter) {
        double rel_res = norm2(r) / norm_b;
        if (rel_res <= params.rel_tol) {
            result.converged      = true;
            result.iterations     = iter;
            result.final_rel_res  = rel_res;
            break;
        }

        A.spmv(p, Ap);                      // Ap = A * p
        double pAp = dot(p, Ap);
        if (std::abs(pAp) < 1e-300) {
            result.iterations    = iter;
            result.final_rel_res = rel_res;
            break;                          // breakdown
        }
        double alpha = rz / pAp;

        for (int i = 0; i < n; ++i) x[i] += alpha * p[i];
        for (int i = 0; i < n; ++i) r[i] -= alpha * Ap[i];

        precond(r, z);                      // z = M⁻¹ r
        double rz_new = dot(r, z);
        double beta   = rz_new / rz;
        rz = rz_new;

        for (int i = 0; i < n; ++i) p[i] = z[i] + beta * p[i];

        if (iter == params.max_iter - 1) {
            result.iterations    = iter + 1;
            result.final_rel_res = norm2(r) / norm_b;
        }
    }

    auto t1 = Clock::now();
    result.solve_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    return result;
}

// ---------------------------------------------------------------------------
// SOR
// ---------------------------------------------------------------------------
LinearSolveResult sor_solve(const SparseCsr& A,
                             const std::vector<double>& b,
                             std::vector<double>& x,
                             const SolverParams& params)
{
    int n = A.nrows;
    assert((int)b.size() == n);
    if ((int)x.size() != n) x.assign(n, 0.0);

    auto t0 = Clock::now();

    std::vector<double> diag = A.diagonal();
    for (auto& d : diag)
        if (std::abs(d) < 1e-300)
            throw std::runtime_error("SOR: zero diagonal");

    double norm_b = norm2(b);
    double omega  = params.sor_omega;

    std::vector<double> r;
    LinearSolveResult result;
    result.method = "SOR";

    for (int iter = 0; iter < params.max_iter; ++iter) {
        // Forward sweep
        for (int i = 0; i < n; ++i) {
            double sigma = b[i];
            for (int p = A.row_ptr[i]; p < A.row_ptr[i + 1]; ++p) {
                int j = A.col_idx[p];
                if (j != i) sigma -= A.values[p] * x[j];
            }
            double x_gs = sigma / diag[i];
            x[i] = (1.0 - omega) * x[i] + omega * x_gs;
        }

        // Check residual every 10 iterations
        if (iter % 10 == 0 || iter == params.max_iter - 1) {
            compute_residual(A, b, x, r);
            double rel_res = norm2(r) / (norm_b + 1e-300);
            if (rel_res <= params.rel_tol) {
                result.converged     = true;
                result.iterations    = iter + 1;
                result.final_rel_res = rel_res;
                break;
            }
            if (iter == params.max_iter - 1) {
                result.iterations    = iter + 1;
                result.final_rel_res = rel_res;
            }
        }
    }

    auto t1 = Clock::now();
    result.solve_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    return result;
}

// ---------------------------------------------------------------------------
// SSOR  (symmetric: forward + backward sweep)
// ---------------------------------------------------------------------------
LinearSolveResult ssor_solve(const SparseCsr& A,
                              const std::vector<double>& b,
                              std::vector<double>& x,
                              const SolverParams& params)
{
    int n = A.nrows;
    assert((int)b.size() == n);
    if ((int)x.size() != n) x.assign(n, 0.0);

    auto t0 = Clock::now();

    std::vector<double> diag = A.diagonal();
    for (auto& d : diag)
        if (std::abs(d) < 1e-300)
            throw std::runtime_error("SSOR: zero diagonal");

    double norm_b = norm2(b);
    double omega  = params.sor_omega;

    std::vector<double> r;
    LinearSolveResult result;
    result.method = "SSOR";

    for (int iter = 0; iter < params.max_iter; ++iter) {
        // Forward sweep
        for (int i = 0; i < n; ++i) {
            double sigma = b[i];
            for (int p = A.row_ptr[i]; p < A.row_ptr[i + 1]; ++p) {
                int j = A.col_idx[p];
                if (j != i) sigma -= A.values[p] * x[j];
            }
            x[i] = (1.0 - omega) * x[i] + omega * sigma / diag[i];
        }
        // Backward sweep
        for (int i = n - 1; i >= 0; --i) {
            double sigma = b[i];
            for (int p = A.row_ptr[i]; p < A.row_ptr[i + 1]; ++p) {
                int j = A.col_idx[p];
                if (j != i) sigma -= A.values[p] * x[j];
            }
            x[i] = (1.0 - omega) * x[i] + omega * sigma / diag[i];
        }

        if (iter % 10 == 0 || iter == params.max_iter - 1) {
            compute_residual(A, b, x, r);
            double rel_res = norm2(r) / (norm_b + 1e-300);
            if (rel_res <= params.rel_tol) {
                result.converged     = true;
                result.iterations    = iter + 1;
                result.final_rel_res = rel_res;
                break;
            }
            if (iter == params.max_iter - 1) {
                result.iterations    = iter + 1;
                result.final_rel_res = rel_res;
            }
        }
    }

    auto t1 = Clock::now();
    result.solve_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    return result;
}

// ---------------------------------------------------------------------------
// BiCGSTAB
// ---------------------------------------------------------------------------
LinearSolveResult bicgstab_solve(const SparseCsr& A,
                                 const std::vector<double>& b,
                                 std::vector<double>& x,
                                 const Preconditioner& precond,
                                 const SolverParams& params)
{
    int n = A.nrows;
    assert((int)b.size() == n);
    if ((int)x.size() != n) x.assign(n, 0.0);

    auto t0 = Clock::now();
    LinearSolveResult result;
    result.method = "BiCGSTAB";

    double norm_b = norm2(b);
    if (norm_b < 1e-300) {
        x.assign(n, 0.0);
        result.converged = true;
        result.final_rel_res = 0.0;
        return result;
    }

    std::vector<double> r(n);
    compute_residual(A, b, x, r);
    double res0 = norm2(r);
    if (res0 / norm_b <= params.rel_tol) {
        result.converged = true;
        result.final_rel_res = res0 / norm_b;
        return result;
    }

    std::vector<double> r0_hat = r;
    std::vector<double> p = r;
    std::vector<double> v(n, 0.0);
    std::vector<double> p_hat(n, 0.0);
    std::vector<double> s(n, 0.0);
    std::vector<double> s_hat(n, 0.0);
    std::vector<double> t(n, 0.0);

    double rho = 1.0;
    double alpha = 1.0;
    double omega = 1.0;

    for (int iter = 0; iter < params.max_iter; ++iter) {
        double rho_new = dot(r0_hat, r);
        if (std::abs(rho_new) < 1e-300) {
            result.iterations = iter + 1;
            result.final_rel_res = norm2(r) / norm_b;
            break;
        }

        if (iter == 0) {
            p = r;
        } else {
            double beta = (rho_new / rho) * (alpha / omega);
            for (int i = 0; i < n; ++i) {
                p[i] = r[i] + beta * (p[i] - omega * v[i]);
            }
        }

        if (precond) precond(p, p_hat);
        else p_hat = p;

        A.spmv(p_hat, v);

        double denom = dot(r0_hat, v);
        if (std::abs(denom) < 1e-300) {
            result.iterations = iter + 1;
            result.final_rel_res = norm2(r) / norm_b;
            break;
        }
        alpha = rho_new / denom;

        for (int i = 0; i < n; ++i) s[i] = r[i] - alpha * v[i];

        double norm_s = norm2(s);
        if (norm_s / norm_b <= params.rel_tol) {
            for (int i = 0; i < n; ++i) x[i] += alpha * p_hat[i];
            result.converged = true;
            result.iterations = iter + 1;
            result.final_rel_res = norm_s / norm_b;
            break;
        }

        if (precond) precond(s, s_hat);
        else s_hat = s;

        A.spmv(s_hat, t);

        double tt = dot(t, t);
        if (std::abs(tt) < 1e-300) {
            omega = 0.0;
        } else {
            omega = dot(t, s) / tt;
        }

        for (int i = 0; i < n; ++i) {
            x[i] += alpha * p_hat[i] + omega * s_hat[i];
            r[i] = s[i] - omega * t[i];
        }

        rho = rho_new;

        double rel_res = norm2(r) / norm_b;
        if (rel_res <= params.rel_tol) {
            result.converged = true;
            result.iterations = iter + 1;
            result.final_rel_res = rel_res;
            break;
        }

        if (std::abs(omega) < 1e-300) {
            result.iterations = iter + 1;
            result.final_rel_res = rel_res;
            break;
        }

        if (iter == params.max_iter - 1) {
            result.iterations = iter + 1;
            result.final_rel_res = rel_res;
        }
    }

    auto t1 = Clock::now();
    result.solve_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    return result;
}

} // namespace t3d
} // namespace thermatrix
