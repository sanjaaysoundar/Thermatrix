#pragma once
// LinearSolvers.h  –  THERMATRIX Phase 1
// Iterative solvers for symmetric positive-definite (SPD) systems A x = b.
// namespace thermatrix::t3d
//
// Solvers implemented:
//   • PCG  – Preconditioned Conjugate Gradient
//       Preconditioners: Jacobi (diagonal scaling), IC(0) (incomplete Cholesky)
//   • SOR  – Successive Over-Relaxation (Gauss-Seidel special case ω=1)
//   • SSOR – Symmetric SOR (SOR forward + backward sweep)
//
// All solvers work directly with SparseCsr.
// Stopping criterion: ||r||₂ / ||b||₂ < rel_tol  (or max_iter reached).

#include "SparseCsr.h"
#include <chrono>
#include <functional>

namespace thermatrix {
namespace t3d {

// ---------------------------------------------------------------------------
// Solver parameters
// ---------------------------------------------------------------------------
struct SolverParams {
    int         max_iter   { 10000 };
    double      rel_tol    { 1e-10 };   ///< ||r||/||b|| stopping tolerance
    double      sor_omega  { 1.5 };     ///< Relaxation factor for SOR/SSOR (1.0 = GS)
    std::string method     { "PCG-IC0" }; ///< "PCG-IC0", "PCG-Jacobi", "SSOR", "SOR"
};

// ---------------------------------------------------------------------------
// Solver result
// ---------------------------------------------------------------------------
struct LinearSolveResult {
    bool   converged     { false };
    int    iterations    { 0 };
    double final_rel_res { 0.0 };   ///< ||r||/||b|| at termination
    double solve_ms      { 0.0 };   ///< Wall-clock time in milliseconds
    std::string method;             ///< "PCG-Jacobi", "PCG-IC0", "SOR", "SSOR"
};

// ---------------------------------------------------------------------------
// Preconditioner interface (function: apply M⁻¹ z → out)
// ---------------------------------------------------------------------------
using Preconditioner = std::function<void(const std::vector<double>& /*z*/,
                                           std::vector<double>& /*out*/)>;

// ---------------------------------------------------------------------------
// Build Jacobi preconditioner (diagonal scaling: M⁻¹ z_i = z_i / A_ii)
// ---------------------------------------------------------------------------
Preconditioner make_jacobi_precond(const SparseCsr& A);

// ---------------------------------------------------------------------------
// Build IC(0) preconditioner
//   Performs incomplete Cholesky factorisation with the sparsity pattern of A.
//   The matrix A must be symmetric and stored with BOTH upper and lower
//   triangles (or at least the lower triangle for the factorisation).
//   Returns the apply functor: M⁻¹ z = L⁻ᵀ L⁻¹ z.
// ---------------------------------------------------------------------------
Preconditioner make_ic0_precond(const SparseCsr& A);

// ---------------------------------------------------------------------------
// Preconditioned Conjugate Gradient
//   Solves A x = b starting from x (initial guess, modified in place).
//   precond: M⁻¹ apply functor (Jacobi or IC0).
// ---------------------------------------------------------------------------
LinearSolveResult pcg_solve(const SparseCsr& A,
                             const std::vector<double>& b,
                             std::vector<double>& x,
                             const Preconditioner& precond,
                             const SolverParams& params = {},
                             const std::string& method_name = "PCG");

// ---------------------------------------------------------------------------
// SOR  (forward sweep only)
//   Solves A x = b iteratively.
//   omega = 1.0 → Gauss-Seidel.
// ---------------------------------------------------------------------------
LinearSolveResult sor_solve(const SparseCsr& A,
                             const std::vector<double>& b,
                             std::vector<double>& x,
                             const SolverParams& params = {});

// ---------------------------------------------------------------------------
// SSOR  (symmetric: forward + backward sweep per iteration)
// ---------------------------------------------------------------------------
LinearSolveResult ssor_solve(const SparseCsr& A,
                              const std::vector<double>& b,
                              std::vector<double>& x,
                              const SolverParams& params = {});

} // namespace t3d
} // namespace thermatrix
