#pragma once
// FemSolver.h – 1D steady-state thermal FEM solver
// THERMATRIX C++17 backend
//
// Solves: -d/dx(kA dT/dx) = qA  on [0, L]
//   T(0) = T_left   (prescribed Dirichlet BC)
//   T(L) = T_right  (prescribed Dirichlet BC)
// using uniform two-node linear (Lagrange) elements.
//
// Assignment parameters:
//   L = 1 m,  A = 1e-4 m²,  k = 10 W/(m·K),  q = 10000 W/m³
//   T(0) = 100 °C,  T(L) = 20 °C
//
// Analytical solution:
//   T_exact(x) = 100 - 80*x/L + [q/(2k)] * x*(L-x)
//
// Units: all inputs in SI; temperatures in °C (offset from K does not affect
//        the diffusion equation when k is constant).

#include <vector>
#include <string>
#include <cstddef>

namespace thermatrix {

/// Parameters for the 1D thermal FEM problem.
struct FemParams {
    double L{1.0};           ///< Rod length [m]
    double A{1e-4};          ///< Cross-sectional area [m²]
    double k{10.0};          ///< Thermal conductivity [W/(m·K)]
    double q{10000.0};       ///< Volumetric heat generation [W/m³]
    double T_left{100.0};    ///< Temperature at x=0 [°C]
    double T_right{20.0};    ///< Temperature at x=L [°C]
    int    n_elements{10};   ///< Number of uniform elements
};

/// Result of a single FEM solve.
struct FemResult {
    bool                success{false};
    std::string         error_message;
    FemParams           params;
    std::vector<double> x_nodes;          ///< Node positions [m]
    std::vector<double> T_fem;            ///< FE temperatures at nodes [°C]
    std::vector<double> x_midpoints;      ///< Element midpoint positions [m]
    std::vector<double> T_fem_mid;        ///< FE temperatures at midpoints (interpolated) [°C]
    std::vector<double> T_exact_mid;      ///< Analytical temperatures at midpoints [°C]
    std::vector<double> error_mid;        ///< |T_fem_mid - T_exact_mid| [°C]
    double              max_abs_error{0.0}; ///< Maximum absolute error at midpoints [°C]
};

/// Evaluate the analytical solution at position x.
double femAnalytical(double x, const FemParams& p);

/// Solve the 1D thermal problem for n_elements.
FemResult femSolve(const FemParams& p);

/// Run convergence study: solve for all given element counts and return results.
/// elem_counts typically {10, 20, 40, 80}.
std::vector<FemResult> femConvergence(const FemParams& base,
                                       const std::vector<int>& elem_counts);

/// Export convergence results to CSV.
/// Returns true on success.
bool femExportCsv(const std::vector<FemResult>& results,
                  const std::string& filepath);

} // namespace thermatrix
