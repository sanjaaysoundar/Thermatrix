#pragma once
// FdmSolver3D.h  –  THERMATRIX 3D Thermal Finite Difference Method Solver
// namespace thermatrix::t3d
//
// METHODOLOGICAL DISCLOSURE:
// On a uniform Cartesian grid, this 7-point voxel formulation is algebraically identical
// to a cell-centred finite-volume scheme with orthogonal planar faces. Its distinguishing
// numerical characteristics are the structured grid layout, Cartesian coordinate alignment,
// and the staircase (voxelized) boundary approximation of non-axis-aligned geometric features.

#include "ThermalProblem.h"
#include "Mesh3D.h"
#include "SolverResult.h"
#include "LinearSolvers.h"
#include <functional>

namespace thermatrix {
namespace t3d {

class FdmSolver3D {
public:
    explicit FdmSolver3D(const ThermalProblem& prob = ThermalProblem{});

    // Solve for given convection case and mesh resolution
    SolverResult solve(ConvCase conv_case = ConvCase::A,
                       MeshResolution res = MeshResolution::Medium,
                       const SolverParams& lparams = SolverParams{});

    // Solve on an already existing structured grid
    SolverResult solve(const StructuredGrid3D& grid,
                       ConvCase conv_case = ConvCase::A,
                       const SolverParams& lparams = SolverParams{});

    // Solve manufactured solution problem on grid with Dirichlet boundary matching T_exact
    SolverResult solve_mms(const StructuredGrid3D& grid,
                           const std::function<double(double, double, double)>& T_exact,
                           const SolverParams& lparams = SolverParams{});

    // Access last assembled system matrix
    const SparseCsr& last_matrix() const noexcept { return last_matrix_; }

private:
    ThermalProblem prob_;
    SparseCsr last_matrix_;
};

} // namespace t3d
} // namespace thermatrix
