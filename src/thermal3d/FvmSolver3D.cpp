// FvmSolver3D.cpp  –  THERMATRIX 3D Thermal Finite Volume Method Solver
// Cell-Centred Finite Volume formulation on the SAME unstructured tetrahedral mesh as FEM.

#include "FvmSolver3D.h"
#include "HeatSinkGeometry.h"
#include <chrono>
#include <cmath>
#include <map>
#include <iostream>
#include <algorithm>
#include <numeric>

namespace thermatrix {
namespace t3d {

FvmSolver3D::FvmSolver3D(const ThermalProblem& prob)
    : prob_(prob) {}

namespace {
using Clock = std::chrono::steady_clock;

struct Vec3 {
    double x, y, z;
    Vec3 operator+(const Vec3& o) const { return {x + o.x, y + o.y, z + o.z}; }
    Vec3 operator-(const Vec3& o) const { return {x - o.x, y - o.y, z - o.z}; }
    Vec3 operator*(double s) const { return {x * s, y * s, z * s}; }
    double dot(const Vec3& o) const { return x * o.x + y * o.y + z * o.z; }
    double norm() const { return std::sqrt(dot(*this)); }
    Vec3 normalized() const {
        double n = norm();
        return (n > 1e-15) ? (*this * (1.0 / n)) : Vec3{0, 0, 1};
    }
    Vec3 cross(const Vec3& o) const {
        return { y * o.z - z * o.y,
                 z * o.x - x * o.z,
                 x * o.y - y * o.x };
    }
};

struct FaceKey {
    int v0, v1, v2;
    bool operator<(const FaceKey& o) const {
        if (v0 != o.v0) return v0 < o.v0;
        if (v1 != o.v1) return v1 < o.v1;
        return v2 < o.v2;
    }
};

struct HalfFaceInfo {
    int elem;
    std::array<int, 3> orig_v;
};
} // namespace

SolverResult FvmSolver3D::solve(ConvCase conv_case,
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

SolverResult FvmSolver3D::solve(const FemMesh& mesh,
                               ConvCase conv_case,
                               const SolverParams& lparams) {
    SolverResult result;
    result.solver_name = "FVM";
    result.case_label = (conv_case == ConvCase::A) ? "A" : "B";

    // Cell-Centred FVM: each tetrahedron is ONE control volume
    int n_cells = mesh.n_elements();
    result.n_dof = n_cells;
    result.n_elements = n_cells;

    if (n_cells == 0) {
        result.success = false;
        result.error_message = "Empty mesh elements";
        return result;
    }

    double h_val = prob_.h(conv_case);
    double T_inf = prob_.T_inf;
    double q_in_total = prob_.Q_total;

    double total_src_area = mesh.total_source_area();
    double q_flux = (total_src_area > 1e-12) ? (q_in_total / total_src_area) : 0.0;

    auto t_as_0 = Clock::now();

    // 1. Compute cell centroids
    std::vector<Vec3> cell_centers(n_cells);
    for (int e = 0; e < n_cells; ++e) {
        const auto& ev = mesh.elements[e].v;
        Vec3 c{0, 0, 0};
        for (int i = 0; i < 4; ++i) {
            c.x += mesh.nodes[ev[i]].x;
            c.y += mesh.nodes[ev[i]].y;
            c.z += mesh.nodes[ev[i]].z;
        }
        cell_centers[e] = c * 0.25;
    }

    // 2. Build face connectivity map across all tetrahedra
    std::map<FaceKey, std::vector<HalfFaceInfo>> face_map;
    for (int e = 0; e < n_cells; ++e) {
        const auto& ev = mesh.elements[e].v;
        std::array<std::array<int, 3>, 4> local_faces = {{
            {ev[1], ev[2], ev[3]},
            {ev[0], ev[3], ev[2]},
            {ev[0], ev[1], ev[3]},
            {ev[0], ev[2], ev[1]}
        }};
        for (const auto& lf : local_faces) {
            std::array<int, 3> sorted = lf;
            std::sort(sorted.begin(), sorted.end());
            face_map[FaceKey{sorted[0], sorted[1], sorted[2]}].push_back({e, lf});
        }
    }

    // Data structures for internal & boundary faces
    struct InternalFace {
        int cell_P;
        int cell_N;
        double a_PN; // orthogonal conductance
        Vec3 k_vec;  // non-orthogonal correction vector
        double area;
        double non_ortho_deg;
    };

    struct BoundaryFace {
        int cell_P;
        double area;
        double g_b;    // Robin conductance (0 for adiabatic / flux)
        double q_load; // Heat flux load [W]
        BcType bc_type;
        Vec3 center;
    };

    std::vector<InternalFace> internal_faces;
    std::vector<BoundaryFace> boundary_faces;

    double min_deg = 180.0, max_deg = 0.0, sum_deg = 0.0;

    HeatSinkGeometry geom;

    for (const auto& pair : face_map) {
        const auto& half_list = pair.second;
        const auto& v_idx = half_list[0].orig_v;
        Vec3 p0{mesh.nodes[v_idx[0]].x, mesh.nodes[v_idx[0]].y, mesh.nodes[v_idx[0]].z};
        Vec3 p1{mesh.nodes[v_idx[1]].x, mesh.nodes[v_idx[1]].y, mesh.nodes[v_idx[1]].z};
        Vec3 p2{mesh.nodes[v_idx[2]].x, mesh.nodes[v_idx[2]].y, mesh.nodes[v_idx[2]].z};

        Vec3 f_center = (p0 + p1 + p2) * (1.0 / 3.0);
        Vec3 f_norm_vec = (p1 - p0).cross(p2 - p0);
        double area = 0.5 * f_norm_vec.norm();
        Vec3 f_unit_norm = f_norm_vec.normalized();

        if (half_list.size() == 2) {
            // Internal face between cell P and cell N
            int cP = half_list[0].elem;
            int cN = half_list[1].elem;
            if (cP > cN) std::swap(cP, cN);

            Vec3 d_PN = cell_centers[cN] - cell_centers[cP];
            double dist = d_PN.norm();
            Vec3 e_PN = (dist > 1e-15) ? (d_PN * (1.0 / dist)) : Vec3{0, 0, 1};

            // Orient normal from P to N
            if (f_unit_norm.dot(e_PN) < 0.0) {
                f_unit_norm = f_unit_norm * -1.0;
            }

            double cos_theta = std::max(0.01, std::min(1.0, f_unit_norm.dot(e_PN)));
            double deg = std::acos(cos_theta) * (180.0 / 3.14159265358979323846);

            min_deg = std::min(min_deg, deg);
            max_deg = std::max(max_deg, deg);
            sum_deg += deg;

            // Orthogonal conductance: a_PN = k * Area * (e_PN . n) / dist
            double a_pn = prob_.k * area * cos_theta / dist;
            // Non-orthogonal correction vector: k_f = Area * (n - (e_PN . n) e_PN)
            Vec3 k_vec = (f_unit_norm - e_PN * cos_theta) * area;

            internal_faces.push_back({cP, cN, a_pn, k_vec, area, deg});

        } else if (half_list.size() == 1) {
            // Boundary face
            int cP = half_list[0].elem;
            Vec3 out_dir = f_center - cell_centers[cP];
            if (f_unit_norm.dot(out_dir) < 0.0) {
                f_unit_norm = f_unit_norm * -1.0;
            }

            double dist_P = std::max(1e-6, std::abs(out_dir.dot(f_unit_norm)));

            BcType bctype = BcType::Robin;
            if (std::abs(f_center.z) < 1e-6) {
                if (geom.is_in_source_patch(f_center.x, f_center.y, f_center.z)) {
                    bctype = BcType::HeatFlux;
                } else {
                    bctype = BcType::Adiabatic;
                }
            } else {
                bctype = BcType::Robin;
            }

            double g_b = 0.0;
            double q_load = 0.0;

            if (bctype == BcType::Robin) {
                // Harmonic thermal resistance coupling cell center P to ambient T_inf
                double R_therm = (dist_P / prob_.k) + (1.0 / h_val);
                g_b = area / R_therm;
            } else if (bctype == BcType::HeatFlux) {
                q_load = q_flux * area;
            }

            boundary_faces.push_back({cP, area, g_b, q_load, bctype, f_center});
        }
    }

    quality_stats_.n_internal_faces = static_cast<int>(internal_faces.size());
    quality_stats_.n_boundary_faces = static_cast<int>(boundary_faces.size());
    quality_stats_.min_non_ortho_deg = min_deg;
    quality_stats_.max_non_ortho_deg = max_deg;
    quality_stats_.mean_non_ortho_deg = (internal_faces.empty()) ? 0.0 : (sum_deg / internal_faces.size());

    // 3. Assemble FVM Matrix
    SparseBuilder builder(n_cells, n_cells);
    std::vector<double> b(n_cells, 0.0);

    for (const auto& iface : internal_faces) {
        builder.add(iface.cell_P, iface.cell_P,  iface.a_PN);
        builder.add(iface.cell_P, iface.cell_N, -iface.a_PN);
        builder.add(iface.cell_N, iface.cell_N,  iface.a_PN);
        builder.add(iface.cell_N, iface.cell_P, -iface.a_PN);
    }

    for (const auto& bface : boundary_faces) {
        if (bface.bc_type == BcType::Robin) {
            builder.add(bface.cell_P, bface.cell_P, bface.g_b);
            b[bface.cell_P] += bface.g_b * T_inf;
        } else if (bface.bc_type == BcType::HeatFlux) {
            b[bface.cell_P] += bface.q_load;
        }
    }

    SparseCsr A = builder.finalise();
    auto t_as_1 = Clock::now();
    result.t_assemble_ms = std::chrono::duration<double, std::milli>(t_as_1 - t_as_0).count();
    result.n_nnz = A.nnz();

    // Matrix symmetry and diagonal diagnostics (r4-7)
    double diff_sq = 0.0, norm_sq = 0.0;
    double min_diag = (A.nrows > 0) ? 1e30 : 0.0;
    for (int i = 0; i < A.nrows; ++i) {
        min_diag = std::min(min_diag, A.get(i, i));
        for (int p_idx = A.row_ptr[i]; p_idx < A.row_ptr[i + 1]; ++p_idx) {
            int j = A.col_idx[p_idx];
            double a_ij = A.values[p_idx];
            double a_ji = A.get(j, i);
            diff_sq += (a_ij - a_ji) * (a_ij - a_ji);
            norm_sq += a_ij * a_ij;
        }
    }
    quality_stats_.symmetry_norm = (norm_sq > 0.0) ? std::sqrt(diff_sq) / std::sqrt(norm_sq) : 0.0;
    quality_stats_.min_diagonal = min_diag;

    // 4. Linear Solve: PCG with IC(0) / Jacobi or SSOR
    std::vector<double> T(n_cells, T_inf);
    SolverParams p = lparams;
    if (p.rel_tol <= 0.0) p.rel_tol = 1e-10;

    Preconditioner precond;
    std::string method_name = p.method;
    int ic0_fallbacks = 0;
    bool ic0_completed = true;
    if (p.method == "PCG-Jacobi" || p.method == "Jacobi" || p.method == "jacobi") {
        precond = make_jacobi_precond(A);
        method_name = "PCG-Jacobi";
    } else if (p.method == "SSOR" || p.method == "ssor") {
        method_name = "SSOR";
    } else {
        try {
            precond = make_ic0_precond(A, &ic0_fallbacks);
            method_name = "PCG-IC0";
        } catch (...) {
            ic0_completed = false;
            precond = make_jacobi_precond(A);
            method_name = "PCG-Jacobi";
        }
    }
    quality_stats_.ic0_fallbacks = ic0_fallbacks;
    quality_stats_.ic0_completed = ic0_completed;

    auto t_sol_0 = Clock::now();
    // Outer deferred-correction iterations for non-orthogonal flux
    int total_iters = 0;
    double final_res = 0.0;
    bool converged = false;

    quality_stats_.outer_residuals.clear();
    int outer_loops = std::max(1, n_outer_);
    for (int corr = 0; corr < outer_loops; ++corr) {
        // Compute non-orthogonal deferred source: b_corr = b + b_no(T)
        std::vector<double> b_eff = b;
        if (corr > 0 && n_outer_ > 1 && alpha_no_ > 0.0) {
            // Compute cell gradients via Green-Gauss: grad(T)_P = (1/V_P) * sum_f (T_f * S_f,out)
            std::vector<Vec3> grad_T(n_cells, Vec3{0, 0, 0});

            // 1. Internal faces contribution to Green-Gauss gradient
            for (const auto& iface : internal_faces) {
                double T_face = 0.5 * (T[iface.cell_P] + T[iface.cell_N]);
                Vec3 d_PN = cell_centers[iface.cell_N] - cell_centers[iface.cell_P];
                double dist = d_PN.norm();
                Vec3 e_PN = (dist > 1e-15) ? (d_PN * (1.0 / dist)) : Vec3{0, 0, 1};
                double cos_th = std::max(0.01, std::min(1.0, std::cos(iface.non_ortho_deg * (3.14159265358979323846 / 180.0))));
                Vec3 S_vec = e_PN * (iface.area * cos_th) + iface.k_vec;

                grad_T[iface.cell_P] = grad_T[iface.cell_P] + S_vec * T_face;
                grad_T[iface.cell_N] = grad_T[iface.cell_N] - S_vec * T_face;
            }

            // 2. Boundary faces contribution to Green-Gauss gradient
            for (const auto& bface : boundary_faces) {
                double T_face = T[bface.cell_P];
                if (bface.bc_type == BcType::Robin) {
                    T_face = (prob_.k * T[bface.cell_P] + 1e-3 * h_val * T_inf) / (prob_.k + 1e-3 * h_val);
                }
                Vec3 out_dir = (bface.center - cell_centers[bface.cell_P]).normalized();
                Vec3 S_b = out_dir * bface.area;
                grad_T[bface.cell_P] = grad_T[bface.cell_P] + S_b * T_face;
            }

            // Divide by cell volume
            for (int e = 0; e < n_cells; ++e) {
                double v = mesh.elements[e].volume;
                if (v > 1e-18) {
                    grad_T[e] = grad_T[e] * (1.0 / v);
                }
            }

            // Non-orthogonal flux deferred correction across internal faces with Jasak bounding
            double alpha_no = alpha_no_; // under-relaxation for deferred correction
            for (const auto& iface : internal_faces) {
                Vec3 grad_f = (grad_T[iface.cell_P] + grad_T[iface.cell_N]) * 0.5;
                double F_no = -prob_.k * iface.k_vec.dot(grad_f);
                double F_lim = 0.3 * iface.a_PN * std::abs(T[iface.cell_P] - T[iface.cell_N]);
                F_no = std::max(-F_lim, std::min(F_lim, F_no));
                b_eff[iface.cell_P] -= alpha_no * F_no;
                b_eff[iface.cell_N] += alpha_no * F_no;
            }
        }

        LinearSolveResult sol_res;
        if (method_name == "SSOR") {
            sol_res = ssor_solve(A, b_eff, T, p);
        } else {
            sol_res = pcg_solve(A, b_eff, T, precond, p, method_name);
        }
        total_iters += sol_res.iterations;
        final_res = sol_res.final_rel_res;
        converged = sol_res.converged;
        quality_stats_.outer_residuals.push_back(sol_res.final_rel_res);
    }

    auto t_sol_1 = Clock::now();
    result.t_solve_ms = std::chrono::duration<double, std::milli>(t_sol_1 - t_sol_0).count();
    result.linear_solver = method_name;
    result.iterations = total_iters;
    result.final_rel_res = final_res;
    result.success = converged;
    result.T = T;

    // 5. Post-processing & Energy Balance
    auto t_post_0 = Clock::now();
    double t_min = 1e9, t_max = -1e9, t_sum = 0.0;
    for (double val : T) {
        t_min = std::min(t_min, val);
        t_max = std::max(t_max, val);
        t_sum += val;
    }
    result.T_min = t_min;
    result.T_max = t_max;
    result.T_avg_raw = t_sum / n_cells;

    // Volume-weighted average: integral(T dV) / total_volume
    double vol_sum = 0.0;
    double weighted_t_sum = 0.0;
    for (int e = 0; e < n_cells; ++e) {
        double v = mesh.elements[e].volume;
        weighted_t_sum += T[e] * v;
        vol_sum += v;
    }
    result.T_avg = (vol_sum > 0.0) ? (weighted_t_sum / vol_sum) : result.T_avg_raw;

    double src_t_sum = 0.0;
    double src_area_sum = 0.0;
    double q_conv_sum = 0.0;

    for (const auto& bf : boundary_faces) {
        double T_cell = T[bf.cell_P];
        if (bf.bc_type == BcType::HeatFlux) {
            src_t_sum += T_cell * bf.area;
            src_area_sum += bf.area;
        } else if (bf.bc_type == BcType::Robin) {
            q_conv_sum += bf.g_b * (T_cell - T_inf);
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

SolverResult FvmSolver3D::solve_mms(
    const FemMesh& mesh,
    const std::function<double(double, double, double)>& T_exact,
    const SolverParams& lparams) {
    SolverResult result;
    result.solver_name = "FVM";
    result.case_label = "MMS";
    int n_cells = mesh.n_elements();
    result.n_dof = n_cells;
    result.n_elements = n_cells;

    if (n_cells == 0) {
        result.success = false;
        result.error_message = "Empty FVM mesh";
        return result;
    }

    auto t_as_0 = Clock::now();

    // 1. Cell centroids
    std::vector<Vec3> cell_centers(n_cells);
    for (int e = 0; e < n_cells; ++e) {
        const auto& ev = mesh.elements[e].v;
        Vec3 c{0, 0, 0};
        for (int i = 0; i < 4; ++i) {
            c.x += mesh.nodes[ev[i]].x;
            c.y += mesh.nodes[ev[i]].y;
            c.z += mesh.nodes[ev[i]].z;
        }
        cell_centers[e] = c * 0.25;
    }

    // 2. Build face connectivity
    std::map<FaceKey, std::vector<HalfFaceInfo>> face_map;
    for (int e = 0; e < n_cells; ++e) {
        const auto& ev = mesh.elements[e].v;
        std::array<std::array<int, 3>, 4> local_faces = {{
            {ev[1], ev[2], ev[3]},
            {ev[0], ev[3], ev[2]},
            {ev[0], ev[1], ev[3]},
            {ev[0], ev[2], ev[1]}
        }};
        for (const auto& lf : local_faces) {
            std::array<int, 3> sorted = lf;
            std::sort(sorted.begin(), sorted.end());
            face_map[FaceKey{sorted[0], sorted[1], sorted[2]}].push_back({e, lf});
        }
    }

    struct InternalFaceMMS {
        int cell_P;
        int cell_N;
        double a_PN;
        Vec3 k_vec;
        double area;
        double cos_theta;
    };
    struct BoundaryFaceMMS {
        int cell_P;
        double area;
        double g_b;
        Vec3 center;
        Vec3 normal;
    };

    std::vector<InternalFaceMMS> internal_faces;
    std::vector<BoundaryFaceMMS> boundary_faces;

    for (const auto& kv : face_map) {
        const auto& hf_list = kv.second;
        const auto& tri_v = hf_list[0].orig_v;
        const auto& p0 = mesh.nodes[tri_v[0]];
        const auto& p1 = mesh.nodes[tri_v[1]];
        const auto& p2 = mesh.nodes[tri_v[2]];

        Vec3 v0{p0.x, p0.y, p0.z};
        Vec3 v1{p1.x, p1.y, p1.z};
        Vec3 v2{p2.x, p2.y, p2.z};
        Vec3 f_center = (v0 + v1 + v2) * (1.0 / 3.0);
        Vec3 cross = (v1 - v0).cross(v2 - v0);
        double area = 0.5 * cross.norm();
        Vec3 n_f = cross.normalized();

        if (hf_list.size() == 2) {
            int cP = hf_list[0].elem;
            int cN = hf_list[1].elem;
            if (cP > cN) std::swap(cP, cN);
            Vec3 d_PN = cell_centers[cN] - cell_centers[cP];
            double dist = d_PN.norm();
            Vec3 e_PN = (dist > 1e-15) ? (d_PN * (1.0 / dist)) : Vec3{0, 0, 1};
            if (n_f.dot(e_PN) < 0.0) n_f = n_f * -1.0;

            double cos_theta = std::max(0.01, std::min(1.0, e_PN.dot(n_f)));
            double a_PN = prob_.k * (area * cos_theta) / std::max(dist, 1e-15);
            Vec3 k_vec = (n_f - e_PN * cos_theta) * area;

            internal_faces.push_back({cP, cN, a_PN, k_vec, area, cos_theta});
        } else if (hf_list.size() == 1) {
            int cP = hf_list[0].elem;
            Vec3 out_dir = f_center - cell_centers[cP];
            if (n_f.dot(out_dir) < 0.0) n_f = n_f * -1.0;
            double dist_P = std::max(1e-6, std::abs(out_dir.dot(n_f)));
            double g_b = prob_.k * area / dist_P;
            Vec3 proj = cell_centers[cP] + n_f * dist_P;
            boundary_faces.push_back({cP, area, g_b, proj, n_f});
        }
    }

    SparseBuilder builder(n_cells, n_cells);
    std::vector<double> b(n_cells, 0.0);

    for (const auto& iface : internal_faces) {
        builder.add(iface.cell_P, iface.cell_P,  iface.a_PN);
        builder.add(iface.cell_P, iface.cell_N, -iface.a_PN);
        builder.add(iface.cell_N, iface.cell_N,  iface.a_PN);
        builder.add(iface.cell_N, iface.cell_P, -iface.a_PN);
    }

    for (const auto& bface : boundary_faces) {
        builder.add(bface.cell_P, bface.cell_P, bface.g_b);
        double T_face = T_exact(bface.center.x, bface.center.y, bface.center.z);
        b[bface.cell_P] += bface.g_b * T_face;
    }

    SparseCsr A = builder.finalise();
    auto t_as_1 = Clock::now();
    result.t_assemble_ms = std::chrono::duration<double, std::milli>(t_as_1 - t_as_0).count();
    result.n_nnz = A.nnz();

    std::vector<double> T(n_cells, 300.0);
    SolverParams p = lparams;
    if (p.rel_tol <= 0.0) p.rel_tol = 1e-12;
    if (p.max_iter <= 0) p.max_iter = 5000;

    auto t_sol_0 = Clock::now();
    auto precond = make_ic0_precond(A);
    int total_iters = 0;
    double final_res = 0.0;
    bool converged = false;

    int outer_loops = std::max(1, n_outer_);
    for (int corr = 0; corr < outer_loops; ++corr) {
        std::vector<double> b_eff = b;
        if (corr > 0 && n_outer_ > 1 && alpha_no_ > 0.0) {
            std::vector<Vec3> grad_T(n_cells, Vec3{0, 0, 0});
            for (const auto& iface : internal_faces) {
                double T_face = 0.5 * (T[iface.cell_P] + T[iface.cell_N]);
                Vec3 d_PN = cell_centers[iface.cell_N] - cell_centers[iface.cell_P];
                double dist = d_PN.norm();
                Vec3 e_PN = (dist > 1e-15) ? (d_PN * (1.0 / dist)) : Vec3{0, 0, 1};
                Vec3 S_vec = e_PN * (iface.area * iface.cos_theta) + iface.k_vec;
                grad_T[iface.cell_P] = grad_T[iface.cell_P] + S_vec * T_face;
                grad_T[iface.cell_N] = grad_T[iface.cell_N] - S_vec * T_face;
            }
            for (const auto& bface : boundary_faces) {
                double T_face = T_exact(bface.center.x, bface.center.y, bface.center.z);
                Vec3 S_b = bface.normal * bface.area;
                grad_T[bface.cell_P] = grad_T[bface.cell_P] + S_b * T_face;
            }
            for (int e = 0; e < n_cells; ++e) {
                double v = mesh.elements[e].volume;
                if (v > 1e-18) grad_T[e] = grad_T[e] * (1.0 / v);
            }
            double alpha_no = alpha_no_;
            for (const auto& iface : internal_faces) {
                Vec3 grad_f = (grad_T[iface.cell_P] + grad_T[iface.cell_N]) * 0.5;
                double F_no = -prob_.k * iface.k_vec.dot(grad_f);
                double F_lim = 0.3 * iface.a_PN * std::abs(T[iface.cell_P] - T[iface.cell_N]);
                F_no = std::max(-F_lim, std::min(F_lim, F_no));
                b_eff[iface.cell_P] -= alpha_no * F_no;
                b_eff[iface.cell_N] += alpha_no * F_no;
            }
        }
        auto sol_res = pcg_solve(A, b_eff, T, precond, p, "PCG-IC0");
        total_iters += sol_res.iterations;
        final_res = sol_res.final_rel_res;
        converged = sol_res.converged;
        quality_stats_.outer_residuals.push_back(sol_res.final_rel_res);
    }

    auto t_sol_1 = Clock::now();
    result.t_solve_ms = std::chrono::duration<double, std::milli>(t_sol_1 - t_sol_0).count();
    result.linear_solver = "PCG-IC0";
    result.iterations = total_iters;
    result.final_rel_res = final_res;
    result.success = converged;
    result.T = T;

    double t_min = 1e9, t_max = -1e9, t_sum = 0.0;
    for (double val : T) {
        t_min = std::min(t_min, val);
        t_max = std::max(t_max, val);
        t_sum += val;
    }
    result.T_min = t_min;
    result.T_max = t_max;
    result.T_avg_raw = t_sum / n_cells;
    result.T_avg = result.T_avg_raw;
    result.peak_mem_bytes = peak_memory_bytes();
    result.t_total_ms = result.t_assemble_ms + result.t_solve_ms;
    return result;
}

} // namespace t3d
} // namespace thermatrix
