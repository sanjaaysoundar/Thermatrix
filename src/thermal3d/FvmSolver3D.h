#pragma once
// FvmSolver3D.h  –  THERMATRIX 3D Thermal Finite Volume Method Solver
// Cell-Centred Finite Volume formulation on the SAME unstructured tetrahedral mesh as FEM.
// namespace thermatrix::t3d

#include <vector>
#include "ThermalProblem.h"
#include "Mesh3D.h"
#include "SolverResult.h"
#include "LinearSolvers.h"
#include <functional>

namespace thermatrix {
namespace t3d {

struct MeshQualityStats {
    double min_non_ortho_deg { 0.0 };
    double max_non_ortho_deg { 0.0 };
    double mean_non_ortho_deg { 0.0 };
    int    n_internal_faces  { 0 };
    int    n_boundary_faces  { 0 };

    // r4-7 telemetry
    double symmetry_norm     { 0.0 };   ///< ||A - A^T||_F / ||A||_F
    double min_diagonal      { 0.0 };   ///< min_i A_ii
    int    ic0_fallbacks     { 0 };     ///< number of modified-diagonal fallbacks in IC(0)
    bool   ic0_completed     { true };  ///< true if IC(0) succeeded without complete failure
    std::vector<double> outer_residuals; ///< residual norm after each outer deferred correction loop
};

class FvmSolver3D {
public:
    explicit FvmSolver3D(const ThermalProblem& prob = ThermalProblem{});

    // Solve for given convection case and mesh resolution (generates FemMesh)
    SolverResult solve(ConvCase conv_case = ConvCase::A,
                       MeshResolution res = MeshResolution::Medium,
                       const SolverParams& lparams = SolverParams{});

    // Solve on an existing tetrahedral mesh (shared with FEM)
    SolverResult solve(const FemMesh& mesh,
                       ConvCase conv_case = ConvCase::A,
                       const SolverParams& lparams = SolverParams{});

    // Solve manufactured solution problem on mesh with boundary matching T_exact
    SolverResult solve_mms(const FemMesh& mesh,
                           const std::function<double(double, double, double)>& T_exact,
                           const SolverParams& lparams = SolverParams{});

    // Non-orthogonal correction parameters
    // n_outer: max outer loops; alpha: correction strength (1 = full); limiter: 0 = off
    void set_correction_params(int n_outer, double alpha, double limiter = 0.0, double outer_tol = 1e-9) noexcept {
        n_outer_ = n_outer;
        alpha_no_ = alpha;
        limiter_ratio_ = limiter;
        outer_tol_ = outer_tol;
    }
    const std::vector<double>& outer_history() const noexcept { return outer_history_; }

    // Mesh non-orthogonality telemetry
    MeshQualityStats last_quality_stats() const noexcept { return quality_stats_; }

    // Access last assembled system matrix (implicit orthogonal part)
    const SparseCsr& last_matrix() const noexcept { return last_matrix_; }

private:
    ThermalProblem prob_;
    MeshQualityStats quality_stats_;
    SparseCsr last_matrix_;
    int n_outer_ { 40 };
    double alpha_no_ { 1.0 };
    double limiter_ratio_ { 0.0 };
    double outer_tol_ { 1e-9 };
    std::vector<double> outer_history_;
};

} // namespace t3d
} // namespace thermatrix
