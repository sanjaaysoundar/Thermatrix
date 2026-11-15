#pragma once
// FvmSolver3D.h  –  THERMATRIX 3D Thermal Finite Volume Method Solver
// Cell-Centred Finite Volume formulation on the SAME unstructured tetrahedral mesh as FEM.
// namespace thermatrix::t3d

#include "ThermalProblem.h"
#include "Mesh3D.h"
#include "SolverResult.h"
#include "LinearSolvers.h"

namespace thermatrix {
namespace t3d {

struct MeshQualityStats {
    double min_non_ortho_deg { 0.0 };
    double max_non_ortho_deg { 0.0 };
    double mean_non_ortho_deg { 0.0 };
    int    n_internal_faces  { 0 };
    int    n_boundary_faces  { 0 };
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

    // Mesh non-orthogonality telemetry
    MeshQualityStats last_quality_stats() const noexcept { return quality_stats_; }

private:
    ThermalProblem prob_;
    MeshQualityStats quality_stats_;
};

} // namespace t3d
} // namespace thermatrix
