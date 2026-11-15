#pragma once
// SolverResult.h  –  THERMATRIX Phase 1
// Common result structure returned by all 3D thermal solvers (FEM, FDM, FVM).
// namespace thermatrix::t3d

#include <cstddef>
#include <string>
#include <vector>

namespace thermatrix {
namespace t3d {

// ---------------------------------------------------------------------------
// Peak memory helper (platform-specific)
// Returns the peak RSS / working-set usage of the current process in bytes.
// Returns 0 if the measurement is unavailable.
// ---------------------------------------------------------------------------
std::size_t peak_memory_bytes();

// ---------------------------------------------------------------------------
// SolverResult  –  unified output for FEM / FDM / FVM
// ---------------------------------------------------------------------------
struct SolverResult {
    // --- Identity -----------------------------------------------------------
    std::string solver_name;    ///< "FEM", "FDM", "FVM"
    std::string case_label;     ///< "A" or "B" (convection case)
    bool        success { false };
    std::string error_message;

    // --- Problem size -------------------------------------------------------
    int    n_dof    { 0 };      ///< Number of degrees of freedom / active nodes
    int    n_nnz    { 0 };      ///< Non-zeros in assembled system matrix
    int    n_elements { 0 };    ///< Number of mesh elements / cells / voxels

    // --- Linear solver statistics -------------------------------------------
    std::string linear_solver;  ///< e.g. "PCG-IC0"
    int    iterations    { 0 };
    double final_rel_res { 0.0 };

    // --- Timing (wall-clock milliseconds) -----------------------------------
    double t_mesh_ms     { 0.0 };   ///< Mesh / grid construction time
    double t_assemble_ms { 0.0 };   ///< Matrix assembly time
    double t_solve_ms    { 0.0 };   ///< Linear solve time
    double t_post_ms     { 0.0 };   ///< Post-processing time
    double t_total_ms    { 0.0 };   ///< Total time (mesh + assemble + solve + post)

    // --- Temperature statistics (°C) ----------------------------------------
    double T_min  { 0.0 };
    double T_max  { 0.0 };
    double T_avg  { 0.0 };     ///< Volume-weighted average T (°C)
    double T_avg_raw { 0.0 }; ///< Unweighted arithmetic average T (°C)
    double T_src  { 0.0 };     ///< Average T on the source patch (heat-flux boundary)

    // --- Energy balance -----------------------------------------------------
    double Q_in     { 0.0 };   ///< Prescribed heat input [W]
    double Q_conv   { 0.0 };   ///< Sum of convective heat out [W]
    double Q_balance_rel { 0.0 }; ///< |Q_in - Q_conv| / Q_in  [dimensionless]

    // --- Memory -------------------------------------------------------------
    std::size_t peak_mem_bytes { 0 };   ///< Peak process memory at end of solve

    // --- Full temperature array (DOF-ordered) --------------------------------
    // For FDM/FVM this is the flat cell-temperature vector (linearised i,j,k).
    // For FEM this is node temperatures in DOF order.
    std::vector<double> T;

    // --- CSV row ------------------------------------------------------------
    // Returns a single CSV row (no newline) for the benchmark report.
    // Header: solver,case,n_dof,n_nnz,n_elements,linear_solver,
    //         iterations,final_rel_res,t_mesh_ms,t_assemble_ms,t_solve_ms,
    //         t_total_ms,T_min,T_max,T_avg,T_src,Q_in,Q_conv,Q_balance_rel,
    //         peak_mem_bytes
    std::string to_csv_row() const;

    // Returns the CSV header string
    static std::string csv_header();
};

// ---------------------------------------------------------------------------
// BenchmarkRunStats  –  multi-run timing statistics (min/median/max)
// ---------------------------------------------------------------------------
struct StageStats {
    double min_ms { 0.0 };
    double med_ms { 0.0 };
    double max_ms { 0.0 };
};

struct BenchmarkRunStats {
    std::string solver;
    std::string level;          ///< "Coarse", "Medium", "Fine"
    std::string case_label;     ///< "A" or "B"
    int         n_dof      { 0 };
    int         n_nnz      { 0 };
    int         n_elements { 0 };
    std::string linear_solver;
    int         iterations { 0 };
    double      rel_tol    { 1e-10 };
    double      final_rel_res { 0.0 };

    StageStats  t_mesh;
    StageStats  t_assemble;
    StageStats  t_solve;
    StageStats  t_post;
    StageStats  t_total;

    double      T_min      { 0.0 };
    double      T_max      { 0.0 };
    double      T_avg      { 0.0 };
    double      T_avg_raw  { 0.0 };
    double      T_src      { 0.0 };

    double      Q_in       { 0.0 };
    double      Q_conv     { 0.0 };
    double      Q_balance_rel { 0.0 };

    std::size_t peak_mem_bytes { 0 };
    double      peak_mem_mb    { 0.0 };

    static std::string csv_header();
    std::string to_csv_row() const;
};

} // namespace t3d
} // namespace thermatrix
