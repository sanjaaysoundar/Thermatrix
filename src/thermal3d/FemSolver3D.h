#pragma once
// FemSolver3D.h  –  THERMATRIX 3D Thermal Finite Element Method Solver
// namespace thermatrix::t3d

#include "ThermalProblem.h"
#include "Mesh3D.h"
#include "SolverResult.h"
#include "LinearSolvers.h"
#include <functional>

namespace thermatrix {
namespace t3d {

class FemSolver3D {
public:
    explicit FemSolver3D(const ThermalProblem& prob = ThermalProblem{});

    // Solve for given convection case and mesh resolution
    SolverResult solve(ConvCase conv_case = ConvCase::A,
                       MeshResolution res = MeshResolution::Medium,
                       const SolverParams& lparams = SolverParams{});

    // Solve on an already existing mesh
    SolverResult solve(const FemMesh& mesh,
                       ConvCase conv_case = ConvCase::A,
                       const SolverParams& lparams = SolverParams{});

    // Solve manufactured solution problem on mesh with Dirichlet boundary matching T_exact
    SolverResult solve_mms(const FemMesh& mesh,
                           const std::function<double(double, double, double)>& T_exact,
                           const SolverParams& lparams = SolverParams{});

private:
    ThermalProblem prob_;
};

} // namespace t3d
} // namespace thermatrix
