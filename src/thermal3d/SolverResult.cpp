// SolverResult.cpp  –  THERMATRIX Phase 1
// Implementation of platform-specific peak memory query and CSV output.

#include "SolverResult.h"

#include <sstream>
#include <iomanip>
#include <fstream>
#include <string>

// Platform headers
#ifdef _WIN32
#  define WIN32_LEAN_AND_MEAN
#  define NOMINMAX
#  include <windows.h>
#  include <psapi.h>
#else
#  include <cstdio>
#endif

namespace thermatrix {
namespace t3d {

// ---------------------------------------------------------------------------
// peak_memory_bytes
// ---------------------------------------------------------------------------
std::size_t peak_memory_bytes() {
#ifdef _WIN32
    PROCESS_MEMORY_COUNTERS pmc{};
    pmc.cb = sizeof(pmc);
    if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc)))
        return static_cast<std::size_t>(pmc.PeakWorkingSetSize);
    return 0;
#else
    // Linux: read VmHWM (peak RSS) from /proc/self/status
    std::ifstream f("/proc/self/status");
    std::string line;
    while (std::getline(f, line)) {
        if (line.rfind("VmHWM:", 0) == 0) {
            // Format: "VmHWM:   12345 kB"
            long kB = 0;
            if (std::sscanf(line.c_str(), "VmHWM: %ld kB", &kB) == 1)
                return static_cast<std::size_t>(kB) * 1024;
        }
    }
    return 0;
#endif
}

// ---------------------------------------------------------------------------
// SolverResult::csv_header
// ---------------------------------------------------------------------------
std::string SolverResult::csv_header() {
    return "solver,case,n_dof,n_nnz,n_elements,linear_solver,"
           "iterations,final_rel_res,"
           "t_mesh_ms,t_assemble_ms,t_solve_ms,t_total_ms,"
           "T_min_C,T_max_C,T_avg_C,T_src_C,"
           "Q_in_W,Q_conv_W,Q_balance_rel,"
           "peak_mem_bytes";
}

// ---------------------------------------------------------------------------
// SolverResult::to_csv_row
// ---------------------------------------------------------------------------
std::string SolverResult::to_csv_row() const {
    std::ostringstream ss;
    ss << std::setprecision(6) << std::scientific;

    auto fixd = [&](double v, int p = 4) -> std::string {
        std::ostringstream o;
        o << std::fixed << std::setprecision(p) << v;
        return o.str();
    };
    auto sci = [&](double v) -> std::string {
        std::ostringstream o;
        o << std::setprecision(4) << std::scientific << v;
        return o.str();
    };

    ss << solver_name       << ","
       << case_label        << ","
       << n_dof             << ","
       << n_nnz             << ","
       << n_elements        << ","
       << linear_solver     << ","
       << iterations        << ","
       << sci(final_rel_res) << ","
       << fixd(t_mesh_ms)   << ","
       << fixd(t_assemble_ms) << ","
       << fixd(t_solve_ms)  << ","
       << fixd(t_total_ms)  << ","
       << fixd(T_min, 4)    << ","
       << fixd(T_max, 4)    << ","
       << fixd(T_avg, 4)    << ","
       << fixd(T_src, 4)    << ","
       << fixd(Q_in, 6)     << ","
       << fixd(Q_conv, 6)   << ","
       << sci(Q_balance_rel) << ","
       << peak_mem_bytes;

    return ss.str();
}

// ---------------------------------------------------------------------------
// BenchmarkRunStats::csv_header
// ---------------------------------------------------------------------------
std::string BenchmarkRunStats::csv_header() {
    return "solver,level,case,n_dof,n_nnz,n_elements,linear_solver,"
           "iterations,rel_tol,final_rel_res,"
           "t_mesh_min_ms,t_mesh_med_ms,t_mesh_max_ms,"
           "t_assemble_min_ms,t_assemble_med_ms,t_assemble_max_ms,"
           "t_solve_min_ms,t_solve_med_ms,t_solve_max_ms,"
           "t_post_min_ms,t_post_med_ms,t_post_max_ms,"
           "t_total_min_ms,t_total_med_ms,t_total_max_ms,"
           "t_mesh_ms,t_assemble_ms,t_solve_ms,t_total_ms,"
           "T_min_C,T_max_C,T_avg_C,T_src_C,"
           "Q_in_W,Q_conv_W,Q_balance_rel,"
           "peak_mem_bytes,peak_mem_mb";
}

// ---------------------------------------------------------------------------
// BenchmarkRunStats::to_csv_row
// ---------------------------------------------------------------------------
std::string BenchmarkRunStats::to_csv_row() const {
    std::ostringstream ss;
    auto fixd = [&](double v, int p = 4) -> std::string {
        std::ostringstream o;
        o << std::fixed << std::setprecision(p) << v;
        return o.str();
    };
    auto sci = [&](double v) -> std::string {
        std::ostringstream o;
        o << std::setprecision(4) << std::scientific << v;
        return o.str();
    };

    ss << solver        << ","
       << level         << ","
       << case_label    << ","
       << n_dof         << ","
       << n_nnz         << ","
       << n_elements    << ","
       << linear_solver << ","
       << iterations    << ","
       << sci(rel_tol)  << ","
       << sci(final_rel_res) << ","
       << fixd(t_mesh.min_ms) << ","
       << fixd(t_mesh.med_ms) << ","
       << fixd(t_mesh.max_ms) << ","
       << fixd(t_assemble.min_ms) << ","
       << fixd(t_assemble.med_ms) << ","
       << fixd(t_assemble.max_ms) << ","
       << fixd(t_solve.min_ms) << ","
       << fixd(t_solve.med_ms) << ","
       << fixd(t_solve.max_ms) << ","
       << fixd(t_post.min_ms) << ","
       << fixd(t_post.med_ms) << ","
       << fixd(t_post.max_ms) << ","
       << fixd(t_total.min_ms) << ","
       << fixd(t_total.med_ms) << ","
       << fixd(t_total.max_ms) << ","
       << fixd(t_mesh.med_ms) << ","      // backward-compatible alias t_mesh_ms
       << fixd(t_assemble.med_ms) << ","  // backward-compatible alias t_assemble_ms
       << fixd(t_solve.med_ms) << ","     // backward-compatible alias t_solve_ms
       << fixd(t_total.med_ms) << ","     // backward-compatible alias t_total_ms
       << fixd(T_min, 4) << ","
       << fixd(T_max, 4) << ","
       << fixd(T_avg, 4) << ","
       << fixd(T_src, 4) << ","
       << fixd(Q_in, 6) << ","
       << fixd(Q_conv, 6) << ","
       << sci(Q_balance_rel) << ","
       << peak_mem_bytes << ","
       << fixd(peak_mem_mb, 2);

    return ss.str();
}

} // namespace t3d
} // namespace thermatrix
