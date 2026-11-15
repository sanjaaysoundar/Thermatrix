#pragma once
// Verification3D.h  –  THERMATRIX 3D Thermal Numerical Verification Suite
// Implements 1D slab analytical benchmark, 3D manufactured-solution convergence study,
// 30 fixed probe interpolation, and Richardson Extrapolation / GCI (FIX 4).
// namespace thermatrix::t3d

#include "SolverResult.h"
#include "ThermalProblem.h"
#include "Mesh3D.h"
#include "FemSolver3D.h"
#include "FdmSolver3D.h"
#include "FvmSolver3D.h"
#include <string>
#include <vector>

namespace thermatrix {
namespace t3d {

struct ProbePoint3D {
    std::string name;
    Point3D     coord;
    double      T_fem { 0.0 };
    double      T_fdm { 0.0 };
    double      T_fvm { 0.0 };
    double      T_ref { 0.0 }; // Extrapolated / fine reference
};

struct GciResult {
    std::string solver;
    std::string quantity; // "T_max" or "T_avg"
    double val_coarse { 0.0 };
    double val_medium { 0.0 };
    double val_fine   { 0.0 };
    double observed_p { 0.0 };
    double val_extrap { 0.0 };
    double gci_21_pct { 0.0 };
    double gci_32_pct { 0.0 };
    double asymptotic_ratio { 0.0 };
};

struct VerificationSuiteResult {
    // 1. 1D Slab Test
    double slab_L2_fem { 0.0 };
    double slab_L2_fdm { 0.0 };
    double slab_L2_fvm { 0.0 };

    // 2. Manufactured Solution Order
    double mms_order_fem { 0.0 };
    double mms_order_fdm { 0.0 };
    double mms_order_fvm { 0.0 };

    // 3. 30 Fixed Probe Points
    std::vector<ProbePoint3D> probes;
    double rmse_fem { 0.0 };
    double rmse_fdm { 0.0 };
    double rmse_fvm { 0.0 };

    // 4. Richardson Extrapolation & GCI
    std::vector<GciResult> gci_results;

    // Export validation report CSV
    bool export_csv(const std::string& filepath) const;
};

class Verification3D {
public:
    explicit Verification3D(const ThermalProblem& prob = ThermalProblem{});

    // Run complete verification suite and export CSV
    VerificationSuiteResult run_full_verification(const std::string& out_csv = "");

    // 1D Slab Test versus exact analytical solution
    bool run_1d_slab_test(double& err_fem, double& err_fdm, double& err_fvm);

    // 3D Box Manufactured Solution test returning observed order
    bool run_mms_test(double& order_fem, double& order_fdm, double& order_fvm);

    // Sample temperature at an arbitrary point
    static double sample_fem(const FemMesh& mesh, const std::vector<double>& T_nodes, const Point3D& pt);
    static double sample_fdm(const StructuredGrid3D& grid, const std::vector<double>& T_cells, const Point3D& pt);
    static double sample_fvm(const FemMesh& mesh, const std::vector<double>& T_tets, const Point3D& pt);

    // 30 Standard heat-sink probe locations
    static std::vector<Point3D> get_standard_probes();

    // Richardson extrapolation and GCI calculation helper
    static GciResult compute_gci(const std::string& solver, const std::string& quantity,
                                 double f_coarse, double f_medium, double f_fine,
                                 double r = 1.5);

private:
    ThermalProblem prob_;
};

} // namespace t3d
} // namespace thermatrix
