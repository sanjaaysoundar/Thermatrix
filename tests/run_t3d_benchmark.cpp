// run_t3d_benchmark.cpp  –  Standalone CLI Runner for THERMATRIX 3D Thermal Solvers Benchmark
// namespace thermatrix::t3d

#include "../src/thermal3d/Benchmark3D.h"
#include <iostream>

using namespace thermatrix::t3d;

int main() {
    std::cout << "Starting THERMATRIX 3D Full Benchmark Suite...\n";
    Benchmark3D bench;

    // Run complete benchmark across FEM, FDM, FVM (Coarse, Medium, Fine for Case A and Case B)
    auto suite = bench.run_full_benchmark(3);

    // Export benchmark CSV
    std::string csv_bench = "THERMATRIX_ASSIGNMENT_2/Results/benchmark_results.csv";
    if (suite.export_csv(csv_bench)) {
        std::cout << "[SUCCESS] Exported benchmark results to " << csv_bench << "\n";
    } else {
        std::cerr << "[ERROR] Failed to export " << csv_bench << "\n";
    }

    // Export simulation CSV (detailed)
    std::string csv_sim = "THERMATRIX_ASSIGNMENT_2/Results/simulation_results.csv";
    if (suite.export_csv(csv_sim)) {
        std::cout << "[SUCCESS] Exported simulation results to " << csv_sim << "\n";
    }

    // Run analytical validation and export validation CSV
    std::string csv_val = "THERMATRIX_ASSIGNMENT_2/Results/validation_results.csv";
    if (bench.run_validation(csv_val)) {
        std::cout << "[SUCCESS] Exported validation results to " << csv_val << "\n";
    }

    std::cout << "All benchmark runs and exports completed successfully.\n";
    return 0;
}
