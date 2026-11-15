#pragma once
// Benchmark3D.h  –  THERMATRIX 3D Thermal Solvers Benchmarking Suite
// namespace thermatrix::t3d

#include "SolverResult.h"
#include "ThermalProblem.h"
#include <string>
#include <vector>

namespace thermatrix {
namespace t3d {

struct BenchmarkSuiteResult {
    std::vector<SolverResult> results;
    std::string summary_table;

    // Export suite results to CSV
    bool export_csv(const std::string& filepath) const;
};

class Benchmark3D {
public:
    explicit Benchmark3D(const ThermalProblem& prob = ThermalProblem{});

    // Run complete benchmark across FEM, FDM, FVM, 3 resolutions, 2 convection cases
    BenchmarkSuiteResult run_full_benchmark(int num_runs_per_case = 3);

    // Run analytical verification on 1D/3D test problems
    bool run_validation(const std::string& out_csv_path);

private:
    ThermalProblem prob_;
};

} // namespace t3d
} // namespace thermatrix
