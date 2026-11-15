// FemSolver3D.cpp  –  THERMATRIX 3D Thermal Finite Element Method Solver
#include "FemSolver3D.h"
#include <chrono>
#include <cmath>
#include <iostream>
#include <algorithm>

namespace thermatrix {
namespace t3d {

FemSolver3D::FemSolver3D(const ThermalProblem& prob)
    : prob_(prob) {}

namespace {
using Clock = std::chrono::steady_clock;

struct Vec3 {
    double x, y, z;
    Vec3 operator+(const Vec3& o) const { return {x + o.x, y + o.y, z + o.z}; }
    Vec3 operator-(const Vec3& o) const { return {x - o.x, y - o.y, z - o.z}; }
    Vec3 operator*(double s) const { return {x * s, y * s, z * s}; }
    double dot(const Vec3& o) const { return x * o.x + y * o.y + z * o.z; }
    Vec3 cross(const Vec3& o) const {
        return { y * o.z - z * o.y,
                 z * o.x - x * o.z,
                 x * o.y - y * o.x };
    }
};

void compute_tet_gradients(const Point3D& pt0, const Point3D& pt1,
                           const Point3D& pt2, const Point3D& pt3,
                           double vol, std::array<Vec3, 4>& grads) {
    (void)vol;
    // Construct 4x4 matrix M where column j is [1, x_j, y_j, z_j]^T
    // Shape functions satisfy N_i(p_j) = delta_ij, meaning:
    // [N_0, N_1, N_2, N_3]^T = M^(-1) * [1, x, y, z]^T
    // Therefore grad(N_i) = [ (M^-1)_{i, 1}, (M^-1)_{i, 2}, (M^-1)_{i, 3} ]^T
    double m[4][4] = {
        { 1.0,   1.0,   1.0,   1.0 },
        { pt0.x, pt1.x, pt2.x, pt3.x },
        { pt0.y, pt1.y, pt2.y, pt3.y },
        { pt0.z, pt1.z, pt2.z, pt3.z }
    };

    // Invert 4x4 matrix via Gauss-Jordan elimination
    double a[4][8];
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) a[i][j] = m[i][j];
        for (int j = 0; j < 4; ++j) a[i][j + 4] = (i == j) ? 1.0 : 0.0;
    }

    for (int i = 0; i < 4; ++i) {
        // Pivot
        int max_r = i;
        double max_val = std::abs(a[i][i]);
        for (int r = i + 1; r < 4; ++r) {
            if (std::abs(a[r][i]) > max_val) {
                max_val = std::abs(a[r][i]);
                max_r = r;
            }
        }
        if (max_r != i) {
            for (int c = 0; c < 8; ++c) std::swap(a[i][c], a[max_r][c]);
        }

        double diag = a[i][i];
        if (std::abs(diag) < 1e-300) diag = 1e-15;
        double inv_diag = 1.0 / diag;
        for (int c = 0; c < 8; ++c) a[i][c] *= inv_diag;

        for (int r = 0; r < 4; ++r) {
            if (r != i) {
                double factor = a[r][i];
                for (int c = 0; c < 8; ++c) a[r][c] -= factor * a[i][c];
            }
        }
    }

    for (int i = 0; i < 4; ++i) {
        grads[i].x = a[i][5]; // col 1 of inverse
        grads[i].y = a[i][6]; // col 2 of inverse
        grads[i].z = a[i][7]; // col 3 of inverse
    }
}
} // namespace

SolverResult FemSolver3D::solve(ConvCase conv_case,
                               MeshResolution res,
                               const SolverParams& lparams) {
    auto t0 = Clock::now();
    HeatSinkGeometry geom;
    FemMesh mesh = generate_fem_mesh(geom, res);
    auto t1 = Clock::now();

    SolverResult res_obj = solve(mesh, conv_case, lparams);
    res_obj.t_mesh_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    res_obj.t_total_ms += res_obj.t_mesh_ms;
    return res_obj;
}

SolverResult FemSolver3D::solve(const FemMesh& mesh,
                               ConvCase conv_case,
                               const SolverParams& lparams) {
    SolverResult result;
    result.solver_name = "FEM";
    result.case_label = (conv_case == ConvCase::A) ? "A" : "B";
    result.n_dof = mesh.n_nodes();
    result.n_elements = mesh.n_elements();

    int n = mesh.n_nodes();
    if (n == 0) {
        result.success = false;
        result.error_message = "Empty FEM mesh";
        return result;
    }

    double h_val = prob_.h(conv_case);
    double T_inf = prob_.T_inf;
    double q_in_total = prob_.Q_total;

    // Calculate source patch area from mesh to ensure exact flux application
    double total_src_area = mesh.total_source_area();
    double q_flux = (total_src_area > 1e-12) ? (q_in_total / total_src_area) : 0.0;

    // Assembly
    auto t_as_0 = Clock::now();
    SparseBuilder builder(n, n);
    std::vector<double> b(n, 0.0);

    // 1. Element stiffness matrices: K_ij = k * V_e * (grad N_i . grad N_j)
    for (const auto& elem : mesh.elements) {
        const auto& ev = elem.v;
        std::array<Vec3, 4> grads;
        compute_tet_gradients(mesh.nodes[ev[0]], mesh.nodes[ev[1]],
                              mesh.nodes[ev[2]], mesh.nodes[ev[3]],
                              elem.volume, grads);

        double coeff = prob_.k * elem.volume;
        for (int i = 0; i < 4; ++i) {
            for (int j = 0; j < 4; ++j) {
                double kval = coeff * grads[i].dot(grads[j]);
                if (std::abs(kval) > 1e-18) {
                    builder.add(ev[i], ev[j], kval);
                }
            }
        }
    }

    // 2. Boundary conditions
    for (const auto& bf : mesh.boundary_faces) {
        const auto& fv = bf.v;
        double area = bf.area;

        if (bf.bc_type == BcType::Robin) {
            // Surface convection stiffness: h * (area / 12) * [2 1 1; 1 2 1; 1 1 2]
            double c_diag = h_val * area * (2.0 / 12.0);
            double c_off  = h_val * area * (1.0 / 12.0);

            for (int i = 0; i < 3; ++i) {
                for (int j = 0; j < 3; ++j) {
                    builder.add(fv[i], fv[j], (i == j) ? c_diag : c_off);
                }
                // RHS ambient load: h * T_inf * (area / 3)
                b[fv[i]] += h_val * T_inf * (area / 3.0);
            }
        } else if (bf.bc_type == BcType::HeatFlux) {
            // RHS prescribed heat flux into domain: q'' * (area / 3)
            double q_load = q_flux * (area / 3.0);
            for (int i = 0; i < 3; ++i) {
                b[fv[i]] += q_load;
            }
        }
    }

    SparseCsr A = builder.finalise();
    auto t_as_1 = Clock::now();
    result.t_assemble_ms = std::chrono::duration<double, std::milli>(t_as_1 - t_as_0).count();
    result.n_nnz = A.nnz();

    // Linear Solve: PCG with IC(0)
    std::vector<double> T(n, T_inf); // initial guess = T_inf
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

    // Average temperature on source patch
    double src_t_sum = 0.0;
    double src_area_sum = 0.0;
    double q_conv_sum = 0.0;

    for (const auto& bf : mesh.boundary_faces) {
        const auto& fv = bf.v;
        double face_T_avg = (T[fv[0]] + T[fv[1]] + T[fv[2]]) / 3.0;

        if (bf.bc_type == BcType::HeatFlux) {
            src_t_sum += face_T_avg * bf.area;
            src_area_sum += bf.area;
        } else if (bf.bc_type == BcType::Robin) {
            q_conv_sum += h_val * bf.area * (face_T_avg - T_inf);
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
