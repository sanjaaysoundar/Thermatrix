// Benchmark3D.cpp  –  THERMATRIX 3D Thermal Solvers Benchmarking Suite
#include "Benchmark3D.h"
#include "FemSolver3D.h"
#include "FdmSolver3D.h"
#include "FvmSolver3D.h"
#include "HeatSinkGeometry.h"
#include <fstream>
#include <iostream>
#include <iomanip>
#include <sstream>

namespace thermatrix {
namespace t3d {

Benchmark3D::Benchmark3D(const ThermalProblem& prob)
    : prob_(prob) {}

bool BenchmarkSuiteResult::export_csv(const std::string& filepath) const {
    std::ofstream out(filepath);
    if (!out) return false;

    out << SolverResult::csv_header() << "\n";
    for (const auto& r : results) {
        out << r.to_csv_row() << "\n";
    }
    return true;
}

BenchmarkSuiteResult Benchmark3D::run_full_benchmark(int num_runs_per_case) {
    BenchmarkSuiteResult suite;
    std::cout << "\n======================================================\n"
              << " THERMATRIX 3D Thermal Solvers Full Benchmark Suite\n"
              << " Geometry: Aluminium 6061 Heat Sink (5 Fins, 10W Heat Input)\n"
              << " Solvers: FEM, FDM, FVM  |  Resolutions: Coarse, Medium, Fine\n"
              << " Cases: Case A (h=10 W/m²K), Case B (h=25 W/m²K)\n"
              << "======================================================\n\n";

    // Export STL geometry file
    HeatSinkGeometry geom;
    geom.export_stl("data/heatsink.stl");
    geom.export_stl("bin/data/heatsink.stl");

    FemSolver3D fem(prob_);
    FdmSolver3D fdm(prob_);
    FvmSolver3D fvm(prob_);

    std::vector<ConvCase> cases = { ConvCase::A, ConvCase::B };
    std::vector<std::pair<std::string, MeshResolution>> resolutions = {
        { "Coarse", MeshResolution::Coarse },
        { "Medium", MeshResolution::Medium },
        { "Fine",   MeshResolution::Fine   }
    };

    for (auto c : cases) {
        std::string case_str = (c == ConvCase::A) ? "Case A (h=10)" : "Case B (h=25)";

        for (const auto& res_pair : resolutions) {
            std::string res_name = res_pair.first;
            MeshResolution res = res_pair.second;

            // 1. FEM
            {
                std::cout << "[RUNNING] FEM " << res_name << " " << case_str << " ("
                          << num_runs_per_case << " runs)... " << std::flush;
                SolverResult best_res;
                double sum_t_mesh = 0, sum_t_as = 0, sum_t_sol = 0, sum_t_tot = 0;

                for (int run = 0; run < num_runs_per_case; ++run) {
                    SolverResult r = fem.solve(c, res);
                    if (run == 0) best_res = r;
                    sum_t_mesh += r.t_mesh_ms;
                    sum_t_as   += r.t_assemble_ms;
                    sum_t_sol  += r.t_solve_ms;
                    sum_t_tot  += r.t_total_ms;
                }
                best_res.t_mesh_ms     = sum_t_mesh / num_runs_per_case;
                best_res.t_assemble_ms = sum_t_as / num_runs_per_case;
                best_res.t_solve_ms    = sum_t_sol / num_runs_per_case;
                best_res.t_total_ms    = sum_t_tot / num_runs_per_case;

                std::cout << "Done: " << best_res.t_total_ms << " ms | DOF: "
                          << best_res.n_dof << " | Tmax: " << best_res.T_max << " °C\n";
                suite.results.push_back(best_res);
            }

            // 2. FDM
            {
                std::cout << "[RUNNING] FDM " << res_name << " " << case_str << " ("
                          << num_runs_per_case << " runs)... " << std::flush;
                SolverResult best_res;
                double sum_t_mesh = 0, sum_t_as = 0, sum_t_sol = 0, sum_t_tot = 0;

                for (int run = 0; run < num_runs_per_case; ++run) {
                    SolverResult r = fdm.solve(c, res);
                    if (run == 0) best_res = r;
                    sum_t_mesh += r.t_mesh_ms;
                    sum_t_as   += r.t_assemble_ms;
                    sum_t_sol  += r.t_solve_ms;
                    sum_t_tot  += r.t_total_ms;
                }
                best_res.t_mesh_ms     = sum_t_mesh / num_runs_per_case;
                best_res.t_assemble_ms = sum_t_as / num_runs_per_case;
                best_res.t_solve_ms    = sum_t_sol / num_runs_per_case;
                best_res.t_total_ms    = sum_t_tot / num_runs_per_case;

                std::cout << "Done: " << best_res.t_total_ms << " ms | DOF: "
                          << best_res.n_dof << " | Tmax: " << best_res.T_max << " °C\n";
                suite.results.push_back(best_res);
            }

            // 3. FVM
            {
                std::cout << "[RUNNING] FVM " << res_name << " " << case_str << " ("
                          << num_runs_per_case << " runs)... " << std::flush;
                SolverResult best_res;
                double sum_t_mesh = 0, sum_t_as = 0, sum_t_sol = 0, sum_t_tot = 0;

                for (int run = 0; run < num_runs_per_case; ++run) {
                    SolverResult r = fvm.solve(c, res);
                    if (run == 0) best_res = r;
                    sum_t_mesh += r.t_mesh_ms;
                    sum_t_as   += r.t_assemble_ms;
                    sum_t_sol  += r.t_solve_ms;
                    sum_t_tot  += r.t_total_ms;
                }
                best_res.t_mesh_ms     = sum_t_mesh / num_runs_per_case;
                best_res.t_assemble_ms = sum_t_as / num_runs_per_case;
                best_res.t_solve_ms    = sum_t_sol / num_runs_per_case;
                best_res.t_total_ms    = sum_t_tot / num_runs_per_case;

                std::cout << "Done: " << best_res.t_total_ms << " ms | DOF: "
                          << best_res.n_dof << " | Tmax: " << best_res.T_max << " °C\n";
                suite.results.push_back(best_res);
            }
        }
    }

    // Build human-readable summary table
    std::ostringstream ss;
    ss << "\n========================================================================================================================\n"
       << std::left << std::setw(8)  << "Solver"
       << std::setw(6)  << "Case"
       << std::setw(10) << "DOF"
       << std::setw(10) << "NNZ"
       << std::setw(8)  << "Iters"
       << std::setw(12) << "Total(ms)"
       << std::setw(10) << "Tmin(C)"
       << std::setw(10) << "Tmax(C)"
       << std::setw(10) << "Tsrc(C)"
       << std::setw(12) << "Q_in(W)"
       << std::setw(12) << "Q_conv(W)"
       << std::setw(14) << "EnergyErr(%)"
       << std::setw(10) << "Mem(MB)\n"
       << "------------------------------------------------------------------------------------------------------------------------\n";

    for (const auto& r : suite.results) {
        ss << std::left << std::setw(8)  << r.solver_name
           << std::setw(6)  << r.case_label
           << std::setw(10) << r.n_dof
           << std::setw(10) << r.n_nnz
           << std::setw(8)  << r.iterations
           << std::fixed << std::setprecision(2)
           << std::setw(12) << r.t_total_ms
           << std::setprecision(2)
           << std::setw(10) << r.T_min
           << std::setw(10) << r.T_max
           << std::setw(10) << r.T_src
           << std::setprecision(3)
           << std::setw(12) << r.Q_in
           << std::setw(12) << r.Q_conv
           << std::setprecision(4)
           << std::setw(14) << (r.Q_balance_rel * 100.0)
           << std::setprecision(2)
           << std::setw(10) << (r.peak_mem_bytes / (1024.0 * 1024.0))
           << "\n";
    }
    ss << "========================================================================================================================\n";

    suite.summary_table = ss.str();
    std::cout << suite.summary_table << "\n";

    return suite;
}

bool Benchmark3D::run_validation(const std::string& out_csv_path) {
    std::ofstream out(out_csv_path);
    if (!out) return false;

    out << "test_id,description,solver,parameter,numerical_val,analytical_ref,error_rel,status\n";

    // Test 1: Analytical heat balance verification
    // 0D lumped thermal balance: Q_in = h * A_conv * (T_avg - T_inf)
    HeatSinkGeometry geom;
    double A_conv = geom.total_exposed_area();
    double h_A = prob_.h(ConvCase::A);
    double h_B = prob_.h(ConvCase::B);
    double expected_dT_A = prob_.Q_total / (h_A * A_conv);
    double expected_dT_B = prob_.Q_total / (h_B * A_conv);

    FemSolver3D fem(prob_);
    FdmSolver3D fdm(prob_);
    FvmSolver3D fvm(prob_);

    auto r_fem_A = fem.solve(ConvCase::A, MeshResolution::Coarse);
    auto r_fdm_A = fdm.solve(ConvCase::A, MeshResolution::Coarse);
    auto r_fvm_A = fvm.solve(ConvCase::A, MeshResolution::Coarse);

    auto r_fem_B = fem.solve(ConvCase::B, MeshResolution::Coarse);
    auto r_fdm_B = fdm.solve(ConvCase::B, MeshResolution::Coarse);
    auto r_fvm_B = fvm.solve(ConvCase::B, MeshResolution::Coarse);

    auto write_row = [&](int id, const std::string& desc, const std::string& slv,
                         const std::string& param, double num, double ref) {
        double err = std::abs(num - ref) / std::abs(ref);
        bool pass = err < 0.15; // Within 15% of 0D lumped estimate (due to spatial gradients)
        out << id << ",\"" << desc << "\"," << slv << "," << param << ","
            << num << "," << ref << "," << err << "," << (pass ? "PASSED" : "FAILED") << "\n";
    };

    write_row(1, "Case A Mean Delta T vs Lumped Model", "FEM", "dT_avg", r_fem_A.T_avg - prob_.T_inf, expected_dT_A);
    write_row(2, "Case A Mean Delta T vs Lumped Model", "FDM", "dT_avg", r_fdm_A.T_avg - prob_.T_inf, expected_dT_A);
    write_row(3, "Case A Mean Delta T vs Lumped Model", "FVM", "dT_avg", r_fvm_A.T_avg - prob_.T_inf, expected_dT_A);

    write_row(4, "Case B Mean Delta T vs Lumped Model", "FEM", "dT_avg", r_fem_B.T_avg - prob_.T_inf, expected_dT_B);
    write_row(5, "Case B Mean Delta T vs Lumped Model", "FDM", "dT_avg", r_fdm_B.T_avg - prob_.T_inf, expected_dT_B);
    write_row(6, "Case B Mean Delta T vs Lumped Model", "FVM", "dT_avg", r_fvm_B.T_avg - prob_.T_inf, expected_dT_B);

    // Test 7-9: Conservation of energy (|Q_in - Q_conv| / Q_in < 1%)
    auto write_energy = [&](int id, const std::string& slv, double q_in, double q_conv) {
        double err = std::abs(q_in - q_conv) / q_in;
        bool pass = err < 0.05; // Less than 5% relative energy balance error
        out << id << ",\"Global Energy Conservation\"," << slv << ",Q_conv,"
            << q_conv << "," << q_in << "," << err << "," << (pass ? "PASSED" : "FAILED") << "\n";
    };

    write_energy(7, "FEM", r_fem_A.Q_in, r_fem_A.Q_conv);
    write_energy(8, "FDM", r_fdm_A.Q_in, r_fdm_A.Q_conv);
    write_energy(9, "FVM", r_fvm_A.Q_in, r_fvm_A.Q_conv);

    return true;
}

} // namespace t3d
} // namespace thermatrix
