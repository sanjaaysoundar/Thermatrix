// run_t3d_benchmark.cpp  –  Standalone CLI Runner for THERMATRIX 3D Thermal Solvers Benchmark
// Supports single-process execution for fair memory and timing measurements (FIX 2).
//
// Usage:
//   run_t3d_benchmark --solver fem|fdm|fvm --level 1|2|3 --case A|B [--repeats 5] [--warmup 1]
//                     [--linear_solver pcg-ic0|pcg-jacobi|ssor] [--out results/]

#include "../src/thermal3d/Benchmark3D.h"
#include "../src/thermal3d/FemSolver3D.h"
#include "../src/thermal3d/FdmSolver3D.h"
#include "../src/thermal3d/FvmSolver3D.h"
#include "../src/thermal3d/HeatSinkGeometry.h"
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using namespace thermatrix::t3d;

namespace {
void print_usage(const char* prog) {
    std::cout << "Usage: " << prog << " [options]\n"
              << "Options:\n"
              << "  --solver <fem|fdm|fvm>                Thermal solver\n"
              << "  --level <1|2|3|coarse|medium|fine>   Mesh / grid resolution level\n"
              << "  --case <A|B>                         Convection case (A: h=10, B: h=25)\n"
              << "  --repeats <N>                        Number of measured runs (default: 5)\n"
              << "  --warmup <N>                         Number of warmup runs (default: 1)\n"
              << "  --linear_solver <pcg-ic0|pcg-jacobi|ssor>  Linear solver (default: pcg-ic0)\n"
              << "  --out <path>                         Output CSV file or directory\n"
              << "  --all                                Run full legacy benchmark suite\n"
              << "  --help                               Show this help message\n";
}

StageStats compute_stage_stats(std::vector<double>& v) {
    StageStats st;
    if (v.empty()) return st;
    std::sort(v.begin(), v.end());
    st.min_ms = v.front();
    st.max_ms = v.back();
    size_t n = v.size();
    if (n % 2 == 1) {
        st.med_ms = v[n / 2];
    } else {
        st.med_ms = 0.5 * (v[n / 2 - 1] + v[n / 2]);
    }
    return st;
}
} // namespace

int main(int argc, char* argv[]) {
    std::string solver_str;
    std::string level_str;
    std::string case_str;
    int repeats = 5;
    int warmup = 1;
    std::string linear_solver_str = "pcg-ic0";
    std::string out_path = "results/benchmark_results.csv";
    std::string geom_type = "heatsink";
    int fin_count = 9;
    bool run_all = false;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--solver" && i + 1 < argc) {
            solver_str = argv[++i];
        } else if (arg == "--level" && i + 1 < argc) {
            level_str = argv[++i];
        } else if (arg == "--case" && i + 1 < argc) {
            case_str = argv[++i];
        } else if (arg == "--repeats" && i + 1 < argc) {
            repeats = std::max(1, std::stoi(argv[++i]));
        } else if (arg == "--warmup" && i + 1 < argc) {
            warmup = std::max(0, std::stoi(argv[++i]));
        } else if (arg == "--linear_solver" && i + 1 < argc) {
            linear_solver_str = argv[++i];
        } else if (arg == "--geometry" && i + 1 < argc) {
            geom_type = argv[++i];
        } else if (arg == "--fins" && i + 1 < argc) {
            fin_count = std::stoi(argv[++i]);
        } else if (arg == "--out" && i + 1 < argc) {
            out_path = argv[++i];
        } else if (arg == "--all") {
            run_all = true;
        } else if (arg == "--help" || arg == "-h") {
            print_usage(argv[0]);
            return 0;
        }
    }

    if (run_all || (solver_str.empty() && level_str.empty() && case_str.empty())) {
        std::cout << "Starting THERMATRIX 3D Full Benchmark Suite (All-in-one fallback)...\n";
        Benchmark3D bench;
        auto suite = bench.run_full_benchmark(3);

        std::string csv_bench = "THERMATRIX_ASSIGNMENT_2/Results/benchmark_results.csv";
        suite.export_csv(csv_bench);
        suite.export_csv("results/benchmark_results.csv");

        std::string csv_val = "THERMATRIX_ASSIGNMENT_2/Results/validation_results.csv";
        bench.run_validation(csv_val);
        bench.run_validation("results/validation_results.csv");

        std::cout << "All benchmark runs and exports completed successfully.\n";
        return 0;
    }

    if (solver_str.empty() || level_str.empty() || case_str.empty()) {
        std::cerr << "Error: --solver, --level, and --case are required for single-process benchmark.\n";
        print_usage(argv[0]);
        return 1;
    }

    // Normalise inputs
    std::string slv_upper;
    for (char c : solver_str) slv_upper += static_cast<char>(std::toupper(c));

    MeshResolution res = MeshResolution::Coarse;
    std::string level_label = "Coarse";
    if (level_str == "1" || level_str == "coarse" || level_str == "Coarse") {
        res = MeshResolution::Coarse;
        level_label = "Coarse";
    } else if (level_str == "2" || level_str == "medium" || level_str == "Medium") {
        res = MeshResolution::Medium;
        level_label = "Medium";
    } else if (level_str == "3" || level_str == "fine" || level_str == "Fine") {
        res = MeshResolution::Fine;
        level_label = "Fine";
    } else {
        std::cerr << "Error: Unknown level '" << level_str << "' (expected 1, 2, 3 or coarse, medium, fine)\n";
        return 1;
    }

    ConvCase conv_case = ConvCase::A;
    std::string case_label = "A";
    if (case_str == "A" || case_str == "a") {
        conv_case = ConvCase::A;
        case_label = "A";
    } else if (case_str == "B" || case_str == "b") {
        conv_case = ConvCase::B;
        case_label = "B";
    } else {
        std::cerr << "Error: Unknown case '" << case_str << "' (expected A or B)\n";
        return 1;
    }

    // Setup SolverParams
    SolverParams sp;
    sp.rel_tol = 1e-10;
    sp.max_iter = 10000;
    if (linear_solver_str == "pcg-jacobi" || linear_solver_str == "jacobi" || linear_solver_str == "Jacobi") {
        sp.method = "PCG-Jacobi";
    } else if (linear_solver_str == "ssor" || linear_solver_str == "SSOR") {
        sp.method = "SSOR";
    } else {
        sp.method = "PCG-IC0";
    }

    // Ensure output target file path
    fs::path target_csv(out_path);
    if (fs::is_directory(target_csv) || out_path.back() == '/' || out_path.back() == '\\') {
        target_csv /= "benchmark_results.csv";
    }
    if (target_csv.has_parent_path()) {
        fs::create_directories(target_csv.parent_path());
    }

    HeatSinkParams hp;
    std::string msh_file;
    if (geom_type == "cube" || geom_type == "Cube") {
        hp.base_length = 0.060;
        hp.base_width = 0.060;
        hp.base_height = 0.060;
        hp.n_fins = 0;
        hp.fin_height = 0.0;
        msh_file = (res == MeshResolution::Coarse) ? "data/cube_coarse.msh" :
                   (res == MeshResolution::Medium) ? "data/cube_medium.msh" :
                                                     "data/cube_fine.msh";
        level_label = "Cube_" + level_label;
    } else {
        if (fin_count == 3) {
            hp.n_fins = 3;
            hp.fin_pitch = 0.0150;
            hp.fin_y0 = 0.0150;
            msh_file = "data/heatsink_3fins.msh";
            level_label = "3Fins_" + level_label;
        } else if (fin_count == 5) {
            hp.n_fins = 5;
            hp.fin_pitch = 0.0100;
            hp.fin_y0 = 0.0100;
            msh_file = "data/heatsink_5fins.msh";
            level_label = "5Fins_" + level_label;
        } else {
            msh_file = (res == MeshResolution::Coarse) ? "data/heatsink_coarse.msh" :
                       (res == MeshResolution::Medium) ? "data/heatsink_medium.msh" :
                                                         "data/heatsink_fine.msh";
            level_label = "9Fins_" + level_label;
        }
    }

    HeatSinkGeometry geom(hp);
    FemMesh shared_mesh;
    StructuredGrid3D shared_grid;

    auto find_file = [](const std::string& f) -> std::string {
        if (fs::exists(f)) return f;
        if (fs::exists("../" + f)) return "../" + f;
        if (fs::exists("bin/" + f)) return "bin/" + f;
        return f;
    };

    msh_file = find_file(msh_file);

    auto t_m0 = std::chrono::steady_clock::now();
    if (slv_upper == "FEM" || slv_upper == "FVM") {
        if (fs::exists(msh_file)) {
            shared_mesh = load_msh_file(msh_file);
        } else {
            shared_mesh = generate_fem_mesh(geom, res);
        }
    } else {
        shared_grid = generate_structured_grid(geom, res);
    }
    auto t_m1 = std::chrono::steady_clock::now();
    double init_mesh_ms = std::chrono::duration<double, std::milli>(t_m1 - t_m0).count();

    ThermalProblem prob;
    FemSolver3D fem(prob);
    FdmSolver3D fdm(prob);
    FvmSolver3D fvm(prob);

    // Warm-up runs
    for (int w = 0; w < warmup; ++w) {
        if (slv_upper == "FEM") fem.solve(shared_mesh, conv_case, sp);
        else if (slv_upper == "FDM") fdm.solve(shared_grid, conv_case, sp);
        else if (slv_upper == "FVM") fvm.solve(shared_mesh, conv_case, sp);
    }

    // Measured runs
    std::vector<double> v_mesh, v_as, v_sol, v_post, v_tot;
    SolverResult last_res;

    for (int r = 0; r < repeats; ++r) {
        SolverResult res_run;
        if (slv_upper == "FEM") res_run = fem.solve(shared_mesh, conv_case, sp);
        else if (slv_upper == "FDM") res_run = fdm.solve(shared_grid, conv_case, sp);
        else if (slv_upper == "FVM") res_run = fvm.solve(shared_mesh, conv_case, sp);
        else {
            std::cerr << "Error: Unknown solver '" << solver_str << "' (expected fem, fdm, or fvm)\n";
            return 1;
        }

        res_run.t_mesh_ms = init_mesh_ms;
        res_run.t_total_ms = res_run.t_mesh_ms + res_run.t_assemble_ms + res_run.t_solve_ms + res_run.t_post_ms;

        v_mesh.push_back(res_run.t_mesh_ms);
        v_as.push_back(res_run.t_assemble_ms);
        v_sol.push_back(res_run.t_solve_ms);
        v_post.push_back(res_run.t_post_ms);
        v_tot.push_back(res_run.t_total_ms);

        last_res = res_run;
    }

    // Process memory queried fresh for this isolated process
    std::size_t mem_bytes = peak_memory_bytes();
    double mem_mb = static_cast<double>(mem_bytes) / (1024.0 * 1024.0);

    BenchmarkRunStats stats;
    stats.solver = slv_upper;
    stats.level = level_label;
    stats.case_label = case_label;
    stats.n_dof = last_res.n_dof;
    stats.n_nnz = last_res.n_nnz;
    stats.n_elements = last_res.n_elements;
    stats.linear_solver = last_res.linear_solver;
    stats.iterations = last_res.iterations;
    stats.rel_tol = sp.rel_tol;
    stats.final_rel_res = last_res.final_rel_res;

    stats.t_mesh = compute_stage_stats(v_mesh);
    stats.t_assemble = compute_stage_stats(v_as);
    stats.t_solve = compute_stage_stats(v_sol);
    stats.t_post = compute_stage_stats(v_post);
    stats.t_total = compute_stage_stats(v_tot);

    stats.T_min = last_res.T_min;
    stats.T_max = last_res.T_max;
    stats.T_avg = last_res.T_avg;
    stats.T_src = last_res.T_src;

    stats.Q_in = last_res.Q_in;
    stats.Q_conv = last_res.Q_conv;
    stats.Q_balance_rel = last_res.Q_balance_rel;

    stats.peak_mem_bytes = mem_bytes;
    stats.peak_mem_mb = mem_mb;

    // Check if target CSV needs header
    bool write_header = false;
    if (!fs::exists(target_csv) || fs::file_size(target_csv) == 0) {
        write_header = true;
    }

    std::ofstream out(target_csv, std::ios::app);
    if (!out) {
        std::cerr << "Error: Unable to open output CSV: " << target_csv << "\n";
        return 1;
    }

    if (write_header) {
        out << BenchmarkRunStats::csv_header() << "\n";
    }
    out << stats.to_csv_row() << "\n";
    out.close();

    // Print summary to stdout
    std::cout << "[BENCHMARK] Solver: " << std::left << std::setw(4) << stats.solver
              << " | Level: " << std::setw(6) << stats.level
              << " | Case: " << stats.case_label
              << " | Linear: " << std::setw(10) << stats.linear_solver
              << " | DOF: " << std::setw(6) << stats.n_dof
              << " | NNZ: " << std::setw(7) << stats.n_nnz
              << " | Total(ms) Med: " << std::fixed << std::setprecision(2) << std::setw(6) << stats.t_total.med_ms
              << " (Mesh: " << stats.t_mesh.med_ms
              << ", Assem: " << stats.t_assemble.med_ms
              << ", Sol: " << stats.t_solve.med_ms
              << ", Post: " << stats.t_post.med_ms << ")"
              << " | Tmax: " << std::setprecision(2) << stats.T_max << " C"
              << " | PeakMem: " << stats.peak_mem_mb << " MB\n";

    return 0;
}
