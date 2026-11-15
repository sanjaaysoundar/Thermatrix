// FdmSolver3D.cpp  –  THERMATRIX 3D Thermal Finite Difference Method Solver
#include "FdmSolver3D.h"
#include <chrono>
#include <cmath>
#include <iostream>
#include <algorithm>

namespace thermatrix {
namespace t3d {

FdmSolver3D::FdmSolver3D(const ThermalProblem& prob)
    : prob_(prob) {}

namespace {
using Clock = std::chrono::steady_clock;
}

SolverResult FdmSolver3D::solve(ConvCase conv_case,
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

SolverResult FdmSolver3D::solve(const StructuredGrid3D& grid,
                               ConvCase conv_case,
                               const SolverParams& lparams) {
    SolverResult result;
    result.solver_name = "FDM";
    result.case_label = (conv_case == ConvCase::A) ? "A" : "B";
    result.n_dof = grid.n_solid_cells();
    result.n_elements = grid.n_solid_cells();

    int n = grid.n_solid_cells();
    if (n == 0) {
        result.success = false;
        result.error_message = "No solid cells in grid";
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

    // Conductance per unit area for each direction
    double G_x = prob_.k * (grid.dy * grid.dz) / grid.dx;
    double G_y = prob_.k * (grid.dx * grid.dz) / grid.dy;
    double G_z = prob_.k * (grid.dx * grid.dy) / grid.dz;

    // 1. Internal faces (solid-solid coupling)
    for (const auto& face : grid.internal_faces) {
        double G = 0.0;
        if (face.dir == FaceDir::Xminus || face.dir == FaceDir::Xplus) G = G_x;
        else if (face.dir == FaceDir::Yminus || face.dir == FaceDir::Yplus) G = G_y;
        else G = G_z;

        int p = face.cell_p;
        int q = face.cell_n;

        // Laplacian stencil contribution:
        // Equation for p: ... + G * (T_p - T_q) ...
        // Equation for q: ... + G * (T_q - T_p) ...
        builder.add(p, p,  G);
        builder.add(p, q, -G);
        builder.add(q, q,  G);
        builder.add(q, p, -G);
    }

    // 2. Boundary faces (solid-air or exterior)
    for (const auto& bf : grid.boundary_faces) {
        int p = bf.cell_p;
        double half_dist = 0.0;
        if (bf.dir == FaceDir::Xminus || bf.dir == FaceDir::Xplus) half_dist = 0.5 * grid.dx;
        else if (bf.dir == FaceDir::Yminus || bf.dir == FaceDir::Yplus) half_dist = 0.5 * grid.dy;
        else half_dist = 0.5 * grid.dz;

        if (bf.bc_type == BcType::Robin) {
            // Effective heat transfer coefficient coupling cell center to ambient T_inf
            // 1 / U_eff = (half_dist / k) + (1 / h)
            double R_therm = (half_dist / prob_.k) + (1.0 / h_val);
            double G_robin = bf.area / R_therm;

            builder.add(p, p, G_robin);
            b[p] += G_robin * T_inf;
        } else if (bf.bc_type == BcType::HeatFlux) {
            // Prescribed heat flux into domain
            b[p] += q_flux * bf.area;
        }
        // Adiabatic: zero flux, zero contribution
    }

    SparseCsr A = builder.finalise();
    auto t_as_1 = Clock::now();
    result.t_assemble_ms = std::chrono::duration<double, std::milli>(t_as_1 - t_as_0).count();
    result.n_nnz = A.nnz();

    // Linear solve: PCG with IC(0) / Jacobi or SSOR
    std::vector<double> T(n, T_inf);
    SolverParams p = lparams;
    if (p.rel_tol <= 0.0) p.rel_tol = 1e-10;

    auto t_sol_0 = Clock::now();
    LinearSolveResult sol_res;
    if (p.method == "PCG-Jacobi" || p.method == "Jacobi" || p.method == "jacobi") {
        auto precond = make_jacobi_precond(A);
        sol_res = pcg_solve(A, b, T, precond, p, "PCG-Jacobi");
    } else if (p.method == "SSOR" || p.method == "ssor") {
        sol_res = ssor_solve(A, b, T, p);
    } else {
        auto precond = make_ic0_precond(A);
        sol_res = pcg_solve(A, b, T, precond, p, "PCG-IC0");
    }
    auto t_sol_1 = Clock::now();

    result.t_solve_ms = std::chrono::duration<double, std::milli>(t_sol_1 - t_sol_0).count();
    result.linear_solver = sol_res.method;
    result.iterations = sol_res.iterations;
    result.final_rel_res = sol_res.final_rel_res;
    result.success = sol_res.converged;
    result.T = T;

    // Post-processing & Energy Balance
    auto t_post_0 = Clock::now();
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

        double half_dist = 0.0;
        if (bf.dir == FaceDir::Xminus || bf.dir == FaceDir::Xplus) half_dist = 0.5 * grid.dx;
        else if (bf.dir == FaceDir::Yminus || bf.dir == FaceDir::Yplus) half_dist = 0.5 * grid.dy;
        else half_dist = 0.5 * grid.dz;

        if (bf.bc_type == BcType::HeatFlux) {
            src_t_sum += T_cell * bf.area;
            src_area_sum += bf.area;
        } else if (bf.bc_type == BcType::Robin) {
            double R_therm = (half_dist / prob_.k) + (1.0 / h_val);
            double G_robin = bf.area / R_therm;
            q_conv_sum += G_robin * (T_cell - T_inf);
        }
    }

    result.T_src = (src_area_sum > 0.0) ? (src_t_sum / src_area_sum) : t_max;
    result.Q_in = q_in_total;
    result.Q_conv = q_conv_sum;
    result.Q_balance_rel = std::abs(q_in_total - q_conv_sum) / q_in_total;
    result.peak_mem_bytes = peak_memory_bytes();
    auto t_post_1 = Clock::now();

    result.t_post_ms = std::chrono::duration<double, std::milli>(t_post_1 - t_post_0).count();
    result.t_total_ms = result.t_mesh_ms + result.t_assemble_ms + result.t_solve_ms + result.t_post_ms;

    return result;
}

} // namespace t3d
} // namespace thermatrix
