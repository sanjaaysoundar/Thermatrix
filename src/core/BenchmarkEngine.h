#pragma once
// BenchmarkEngine.h – Serial and parallel thermal FEM benchmark
// THERMATRIX C++17 backend
//
// Generates a reproducible batch of independent 1D thermal FEM problems
// (varied k and q, fixed seed), solves them both serially and in parallel
// using a bounded worker pool with std::thread or OpenMP, and verifies
// agreement (max absolute temperature difference ≤ 1e-8 °C).
//
// Parallelism strategy:
//   Each solve is independent → embarrassingly parallel.
//   A fixed-size thread pool dequeues case indices from a shared queue.
//   Results are written to pre-allocated output arrays at deterministic indices,
//   preserving input order regardless of completion order.
//
// Worker counts tested: 1, 2, 3, 4 (capped at logical CPU count).
// Warm-up: 1 repetition (excluded from timing).
// Measured: 5 repetitions; median is reported.

#include <vector>
#include <string>
#include <cstddef>
#include <cstdint>

namespace thermatrix {

/// Parameters for one benchmark case.
struct BenchCase {
    int    n_elements;
    double k;       ///< [W/(m·K)]
    double q;       ///< [W/m³]
    // Other rod params remain at defaults (L=1, A=1e-4, T_left=100, T_right=20)
};

/// System information collected at start of benchmark.
struct SysInfo {
    std::string cpu_name;
    int         logical_cores;
    int         physical_cores; ///< -1 if unavailable
    double      ram_gb;         ///< -1 if unavailable
    std::string compiler;
    std::string opt_flags;
    bool        single_logical_cpu; ///< speedup cannot be assessed if true
};

/// Results for one (worker-count, repetition) combination.
struct BenchRepeat {
    int    n_workers;
    int    rep;          ///< 1..5
    double compute_ms;   ///< Wall-clock time for batch solve [ms]
};

/// Summary for one worker count (median over repetitions).
struct BenchSummary {
    int    n_workers;
    double median_ms;
    double speedup;      ///< T1/Tp
    double efficiency;   ///< (T1/Tp)/p
    double max_abs_diff; ///< vs serial results [°C]
    bool   agreement;    ///< max_abs_diff <= 1e-8
};

struct BenchmarkResult {
    bool                     success{false};
    std::string              error_message;
    SysInfo                  sysinfo;
    int                      n_cases;
    int                      n_elements_per_case;
    std::uint32_t            seed;
    double                   launch_to_ready_ms; ///< Setup time (excluded from compute timing)
    std::vector<BenchRepeat> raw_repeats;
    std::vector<BenchSummary> summaries;          ///< one per worker count
};

struct BenchmarkConfig {
    int           n_cases{500};         ///< Batch size
    int           n_elements{40};       ///< Mesh size per case
    std::uint32_t seed{42};
    int           max_workers{4};       ///< Will be capped at logical CPUs
    int           n_warmup{1};
    int           n_repeats{5};
};

/// Collect system information (CPU, RAM, compiler).
SysInfo collectSysInfo();

/// Generate reproducible cases with fixed seed.
std::vector<BenchCase> generateCases(int n_cases, int n_elements, std::uint32_t seed);

/// Run the full benchmark.
BenchmarkResult runBenchmark(const BenchmarkConfig& cfg);

/// Export results to CSV.
bool benchmarkExportCsv(const BenchmarkResult& r, const std::string& filepath);

} // namespace thermatrix
