#pragma once
// FvmSolver3D.h  –  THERMATRIX 3D Thermal Finite Volume Method Solver
// namespace thermatrix::t3d

#include "ThermalProblem.h"
#include "Mesh3D.h"
#include "SolverResult.h"
#include "LinearSolvers.h"

namespace thermatrix {
namespace t3d {

class FvmSolver3D {
public:
    explicit FvmSolver3D(const ThermalProblem& prob = ThermalProblem{});

    // Solve for given convection case and mesh resolution
    SolverResult solve(ConvCase conv_case = ConvCase::A,
                       MeshResolution res = MeshResolution::Medium,
                       const SolverParams& lparams = SolverParams{});

    // Solve on an already existing structured grid
    SolverResult solve(const StructuredGrid3D& grid,
                       ConvCase conv_case = ConvCase::A,
                       const SolverParams& lparams = SolverParams{});

private:
    ThermalProblem prob_;
};

} // namespace t3d
} // namespace thermatrix
