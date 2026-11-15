// FvmSolver3D.cpp  –  THERMATRIX 3D Thermal Finite Volume Method Solver
#include "FvmSolver3D.h"
#include <chrono>
#include <cmath>
#include <iostream>
#include <algorithm>

namespace thermatrix {
namespace t3d {

FvmSolver3D::FvmSolver3D(const ThermalProblem& prob)
    : prob_(prob) {}

namespace {
using Clock = std::chrono::steady_clock;
}

SolverResult FvmSolver3D::solve(ConvCase conv_case,
                               MeshResolution res,
                               const SolverParams& lparams) {
    auto t0 = Clock::now();
    HeatSinkGeometry geom;
    StructuredGrid3D grid = generate_structured_grid(geom, res);
    auto t1 = Clock::now();

    SolverResult res_obj = solve(grid, conv_case, lparams);
    res_obj.t_mesh_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    res_obj.t_total_ms += res_obj.t_mesh_ms;
    return res_obj;
}

SolverResult FvmSolver3D::solve(const StructuredGrid3D& grid,
                               ConvCase conv_case,
                               const SolverParams& lparams) {
    SolverResult result;
    result.solver_name = "FVM";
    result.case_label = (conv_case == ConvCase::A) ? "A" : "B";
    result.n_dof = grid.n_solid_cells();
    result.n_elements = grid.n_solid_cells();

    int n = grid.n_solid_cells();
    if (n == 0) {
        result.success = false;
        result.error_message = "No solid control volumes in grid";
        return result;
    }

    double h_val = prob_.h(conv_case);
    double T_inf = prob_.T_inf;
    double q_in_total = prob_.Q_total;

    double total_src_area = grid.total_source_area();
    double q_flux = (total_src_area > 1e-12) ? (q_in_total / total_src_area) : 0.0;

    auto t_as_0 = Clock::now();
    SparseBuilder builder(n, n);
    std::vector<double> b(n, 0.0);

    // 1. Internal face diffusive flux: F_PN = -k * A_f * (T_N - T_P) / d_PN
    for (const auto& face : grid.internal_faces) {
        int p = face.cell_p;
        int q = face.cell_n;

        double d_pn = 0.0;
        if (face.dir == FaceDir::Xminus || face.dir == FaceDir::Xplus) d_pn = grid.dx;
        else if (face.dir == FaceDir::Yminus || face.dir == FaceDir::Yplus) d_pn = grid.dy;
        else d_pn = grid.dz;

        // Diffusive conductance a_PN = k * Area / distance
        double a_pn = prob_.k * face.area / d_pn;

        builder.add(p, p,  a_pn);
        builder.add(p, q, -a_pn);
        builder.add(q, q,  a_pn);
        builder.add(q, p, -a_pn);
    }

    // 2. Boundary face flux
    for (const auto& bf : grid.boundary_faces) {
        int p = bf.cell_p;

        double delta_n = 0.0;
        if (bf.dir == FaceDir::Xminus || bf.dir == FaceDir::Xplus) delta_n = grid.dx;
        else if (bf.dir == FaceDir::Yminus || bf.dir == FaceDir::Yplus) delta_n = grid.dy;
        else delta_n = grid.dz;

        if (bf.bc_type == BcType::Robin) {
            // Harmonic average conductance coupling cell center P to ambient T_inf
            // R_total = (delta_n / 2) / k + 1 / h
            double R_therm = (0.5 * delta_n / prob_.k) + (1.0 / h_val);
            double g_b = bf.area / R_therm;

            builder.add(p, p, g_b);
            b[p] += g_b * T_inf;
        } else if (bf.bc_type == BcType::HeatFlux) {
            // Prescribed heat input directly into control volume
            b[p] += q_flux * bf.area;
        }
    }

    SparseCsr A = builder.finalise();
    auto t_as_1 = Clock::now();
    result.t_assemble_ms = std::chrono::duration<double, std::milli>(t_as_1 - t_as_0).count();
    result.n_nnz = A.nnz();

    // Linear solve: PCG with IC(0)
    std::vector<double> T(n, T_inf);
    auto precond = make_ic0_precond(A);

    SolverParams p = lparams;
    p.rel_tol = 1e-10;

    auto t_sol_0 = Clock::now();
    auto sol_res = pcg_solve(A, b, T, precond, p, "PCG-IC0");
    auto t_sol_1 = Clock::now();

    result.t_solve_ms = std::chrono::duration<double, std::milli>(t_sol_1 - t_sol_0).count();
    result.t_total_ms = result.t_assemble_ms + result.t_solve_ms;
    result.linear_solver = sol_res.method;
    result.iterations = sol_res.iterations;
    result.final_rel_res = sol_res.final_rel_res;
    result.success = sol_res.converged;
    result.T = T;

    // Post-processing & Energy Balance
    double t_min = 1e9, t_max = -1e9, t_sum = 0.0;
    for (double val : T) {
        t_min = std::min(t_min, val);
        t_max = std::max(t_max, val);
        t_sum += val;
    }
    result.T_min = t_min;
    result.T_max = t_max;
    result.T_avg = t_sum / n;

    double src_t_sum = 0.0;
    double src_area_sum = 0.0;
    double q_conv_sum = 0.0;

    for (const auto& bf : grid.boundary_faces) {
        int cell_idx = bf.cell_p;
        double T_cell = T[cell_idx];

        double delta_n = 0.0;
        if (bf.dir == FaceDir::Xminus || bf.dir == FaceDir::Xplus) delta_n = grid.dx;
        else if (bf.dir == FaceDir::Yminus || bf.dir == FaceDir::Yplus) delta_n = grid.dy;
        else delta_n = grid.dz;

        if (bf.bc_type == BcType::HeatFlux) {
            src_t_sum += T_cell * bf.area;
            src_area_sum += bf.area;
        } else if (bf.bc_type == BcType::Robin) {
            double R_therm = (0.5 * delta_n / prob_.k) + (1.0 / h_val);
            double g_b = bf.area / R_therm;
            q_conv_sum += g_b * (T_cell - T_inf);
        }
    }

    result.T_src = (src_area_sum > 0.0) ? (src_t_sum / src_area_sum) : t_max;
    result.Q_in = q_in_total;
    result.Q_conv = q_conv_sum;
    result.Q_balance_rel = std::abs(q_in_total - q_conv_sum) / q_in_total;
    result.peak_mem_bytes = peak_memory_bytes();

    return result;
}

} // namespace t3d
} // namespace thermatrix
