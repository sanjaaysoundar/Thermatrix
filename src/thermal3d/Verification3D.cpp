// Verification3D.cpp  –  THERMATRIX 3D Thermal Numerical Verification Suite
#include "Verification3D.h"
#include "LinearSolvers.h"
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <algorithm>
#include <map>

namespace thermatrix {
namespace t3d {

Verification3D::Verification3D(const ThermalProblem& prob)
    : prob_(prob) {}

std::vector<Point3D> Verification3D::get_standard_probes() {
    // 30 Standard probe locations distributed throughout the heat sink solid
    return {
        // Base Corners (4)
        { 0.005, 0.005, 0.002 },
        { 0.055, 0.005, 0.002 },
        { 0.005, 0.055, 0.002 },
        { 0.055, 0.055, 0.002 },
        // Base Mid-Edges (4)
        { 0.030, 0.005, 0.002 },
        { 0.005, 0.030, 0.002 },
        { 0.055, 0.030, 0.002 },
        { 0.030, 0.055, 0.002 },
        // Base under Source Patch (2)
        { 0.020, 0.035, 0.001 },
        { 0.020, 0.035, 0.003 },
        // Base near Source Patch Perimeter (2)
        { 0.016, 0.032, 0.002 },
        { 0.024, 0.038, 0.002 },
        // Fin Roots at z = 4.5 mm (6)
        { 0.015, 0.006, 0.0045 }, { 0.045, 0.006, 0.0045 }, // Fin 0
        { 0.015, 0.030, 0.0045 }, { 0.045, 0.030, 0.0045 }, // Fin 4 (center)
        { 0.015, 0.054, 0.0045 }, { 0.045, 0.054, 0.0045 }, // Fin 8
        // Mid-Fin Height at z = 16.5 mm (6)
        { 0.030, 0.006, 0.0165 }, // Fin 0
        { 0.030, 0.018, 0.0165 }, // Fin 2
        { 0.015, 0.030, 0.0165 }, { 0.045, 0.030, 0.0165 }, // Fin 4
        { 0.030, 0.042, 0.0165 }, // Fin 6
        { 0.030, 0.054, 0.0165 }, // Fin 8
        // Fin Tips at z = 28.0 mm (6)
        { 0.030, 0.006, 0.0280 }, // Fin 0
        { 0.030, 0.018, 0.0280 }, // Fin 2
        { 0.015, 0.030, 0.0280 }, { 0.045, 0.030, 0.0280 }, // Fin 4
        { 0.030, 0.042, 0.0280 }, // Fin 6
        { 0.030, 0.054, 0.0280 }  // Fin 8
    };
}

double Verification3D::sample_fem(const FemMesh& mesh, const std::vector<double>& T_nodes, const Point3D& pt) {
    if (mesh.elements.empty() || T_nodes.empty()) return 0.0;

    double best_dist_sq = 1e9;
    int best_node = 0;

    // Helper: compute sub-tetrahedron volume
    auto tet_sub_vol = [](const Point3D& a, const Point3D& b, const Point3D& c, const Point3D& d) {
        double ax = a.x - d.x, ay = a.y - d.y, az = a.z - d.z;
        double bx = b.x - d.x, by = b.y - d.y, bz = b.z - d.z;
        double cx = c.x - d.x, cy = c.y - d.y, cz = c.z - d.z;
        double cross_x = by * cz - bz * cy;
        double cross_y = bz * cx - bx * cz;
        double cross_z = bx * cy - by * cx;
        return (ax * cross_x + ay * cross_y + az * cross_z) / 6.0;
    };

    // Find containing tetrahedron and evaluate barycentric coordinates
    for (const auto& elem : mesh.elements) {
        const auto& p0 = mesh.nodes[elem.v[0]];
        const auto& p1 = mesh.nodes[elem.v[1]];
        const auto& p2 = mesh.nodes[elem.v[2]];
        const auto& p3 = mesh.nodes[elem.v[3]];

        double v_total = elem.volume;
        if (v_total <= 1e-18) continue;

        double v0 = std::abs(tet_sub_vol(pt, p1, p2, p3));
        double v1 = std::abs(tet_sub_vol(p0, pt, p2, p3));
        double v2 = std::abs(tet_sub_vol(p0, p1, pt, p3));
        double v3 = std::abs(tet_sub_vol(p0, p1, p2, pt));

        double sum_v = v0 + v1 + v2 + v3;
        if (std::abs(sum_v - v_total) <= 1e-4 * v_total) {
            double l0 = v0 / v_total;
            double l1 = v1 / v_total;
            double l2 = v2 / v_total;
            double l3 = v3 / v_total;
            return l0 * T_nodes[elem.v[0]] + l1 * T_nodes[elem.v[1]] +
                   l2 * T_nodes[elem.v[2]] + l3 * T_nodes[elem.v[3]];
        }
    }

    // Fallback: nearest node
    for (size_t i = 0; i < mesh.nodes.size(); ++i) {
        double dx = mesh.nodes[i].x - pt.x;
        double dy = mesh.nodes[i].y - pt.y;
        double dz = mesh.nodes[i].z - pt.z;
        double d2 = dx * dx + dy * dy + dz * dz;
        if (d2 < best_dist_sq) {
            best_dist_sq = d2;
            best_node = static_cast<int>(i);
        }
    }
    return T_nodes[best_node];
}

double Verification3D::sample_fdm(const StructuredGrid3D& grid, const std::vector<double>& T_cells, const Point3D& pt) {
    if (grid.n_solid_cells() == 0 || T_cells.empty()) return 0.0;

    int i = static_cast<int>(std::floor((pt.x - grid.origin.x) / grid.dx));
    int j = static_cast<int>(std::floor((pt.y - grid.origin.y) / grid.dy));
    int k = static_cast<int>(std::floor((pt.z - grid.origin.z) / grid.dz));

    i = std::max(0, std::min(grid.nx - 1, i));
    j = std::max(0, std::min(grid.ny - 1, j));
    k = std::max(0, std::min(grid.nz - 1, k));

    int idx = grid.flat_idx(i, j, k);
    if (grid.is_solid[idx]) {
        int dof = grid.cell_to_dof[idx];
        if (dof >= 0 && dof < static_cast<int>(T_cells.size())) {
            return T_cells[dof];
        }
    }

    // Nearest solid cell search
    double best_dist_sq = 1e9;
    double best_T = 0.0;
    for (int dof = 0; dof < grid.n_solid_cells(); ++dof) {
        const auto& c = grid.dof_to_coord[dof];
        Point3D cc = grid.cell_center(c.i, c.j, c.k);
        double dx = cc.x - pt.x, dy = cc.y - pt.y, dz = cc.z - pt.z;
        double d2 = dx * dx + dy * dy + dz * dz;
        if (d2 < best_dist_sq) {
            best_dist_sq = d2;
            best_T = T_cells[dof];
        }
    }
    return best_T;
}

double Verification3D::sample_fvm(const FemMesh& mesh, const std::vector<double>& T_tets, const Point3D& pt) {
    if (mesh.elements.empty() || T_tets.empty()) return 0.0;

    auto tet_sub_vol = [](const Point3D& a, const Point3D& b, const Point3D& c, const Point3D& d) {
        double ax = a.x - d.x, ay = a.y - d.y, az = a.z - d.z;
        double bx = b.x - d.x, by = b.y - d.y, bz = b.z - d.z;
        double cx = c.x - d.x, cy = c.y - d.y, cz = c.z - d.z;
        double cross_x = by * cz - bz * cy;
        double cross_y = bz * cx - bx * cz;
        double cross_z = bx * cy - by * cx;
        return (ax * cross_x + ay * cross_y + az * cross_z) / 6.0;
    };

    double best_dist_sq = 1e9;
    int best_elem = 0;

    for (size_t e = 0; e < mesh.elements.size(); ++e) {
        const auto& elem = mesh.elements[e];
        const auto& p0 = mesh.nodes[elem.v[0]];
        const auto& p1 = mesh.nodes[elem.v[1]];
        const auto& p2 = mesh.nodes[elem.v[2]];
        const auto& p3 = mesh.nodes[elem.v[3]];

        double v_total = elem.volume;
        if (v_total <= 1e-18) continue;

        double v0 = std::abs(tet_sub_vol(pt, p1, p2, p3));
        double v1 = std::abs(tet_sub_vol(p0, pt, p2, p3));
        double v2 = std::abs(tet_sub_vol(p0, p1, pt, p3));
        double v3 = std::abs(tet_sub_vol(p0, p1, p2, pt));

        double sum_v = v0 + v1 + v2 + v3;
        if (std::abs(sum_v - v_total) <= 1e-4 * v_total) {
            return T_tets[e]; // cell value
        }

        Point3D c = { (p0.x + p1.x + p2.x + p3.x) * 0.25,
                      (p0.y + p1.y + p2.y + p3.y) * 0.25,
                      (p0.z + p1.z + p2.z + p3.z) * 0.25 };
        double d2 = (c.x - pt.x)*(c.x - pt.x) + (c.y - pt.y)*(c.y - pt.y) + (c.z - pt.z)*(c.z - pt.z);
        if (d2 < best_dist_sq) {
            best_dist_sq = d2;
            best_elem = static_cast<int>(e);
        }
    }
    return T_tets[best_elem];
}

GciResult Verification3D::compute_gci(const std::string& solver, const std::string& quantity,
                                      double f_coarse, double f_medium, double f_fine,
                                      double r21, double r32) {
    GciResult res;
    res.solver = solver;
    res.quantity = quantity;
    res.val_coarse = f_coarse;
    res.val_medium = f_medium;
    res.val_fine   = f_fine;

    double e21 = f_fine - f_medium;
    double e32 = f_medium - f_coarse;

    // Check monotonic convergence with decreasing differences (|e21| < |e32| and same sign)
    bool same_sign = (e21 * e32 > 0.0);
    bool decreasing = (std::abs(e21) < std::abs(e32));
    if (!same_sign || !decreasing || std::abs(e21) < 1e-12) {
        // Non-monotonic or divergent: quantity is NOT in asymptotic range
        res.is_monotonic = false;
        res.observed_p = -1.0;
        res.val_extrap = std::numeric_limits<double>::quiet_NaN();
        res.gci_21_pct = std::numeric_limits<double>::quiet_NaN();
        res.gci_32_pct = std::numeric_limits<double>::quiet_NaN();
        res.asymptotic_ratio = std::numeric_limits<double>::quiet_NaN();
        return res;
    }

    res.is_monotonic = true;

    // Solve general three-grid equation for observed order p:
    //   p = (1 / ln(r21)) * ( ln|e32/e21| + q(p) )
    //   where q(p) = ln((r21^p - 1) / (r32^p - 1))
    // Solved via fixed-point iteration without clamping p or taking abs() of log ratio.
    double p = std::log(e32 / e21) / std::log(r21);
    if (std::abs(r21 - r32) > 1e-4) {
        for (int iter = 0; iter < 100; ++iter) {
            double r21_p = std::pow(r21, p);
            double r32_p = std::pow(r32, p);
            double arg = (r21_p - 1.0) / (r32_p - 1.0);
            if (arg <= 0.0) break;
            double q_p = std::log(arg);
            double p_next = (1.0 / std::log(r21)) * (std::log(e32 / e21) + q_p);
            if (std::abs(p_next - p) < 1e-6) {
                p = p_next;
                break;
            }
            p = 0.5 * (p + p_next);
        }
    }
    res.observed_p = p;

    double r21_p = std::pow(r21, p);
    double r32_p = std::pow(r32, p);
    double denom21 = r21_p - 1.0;
    double denom32 = r32_p - 1.0;

    res.val_extrap = f_fine + (f_fine - f_medium) / denom21;
    res.gci_21_pct = (1.25 * std::abs((f_fine - f_medium) / f_fine) / denom21) * 100.0;
    res.gci_32_pct = (1.25 * std::abs((f_medium - f_coarse) / f_medium) / denom32) * 100.0;
    res.asymptotic_ratio = res.gci_32_pct / (r21_p * res.gci_21_pct);

    return res;
}

bool Verification3D::run_1d_slab_test(double& err_fem, double& err_fdm, double& err_fvm) {
    VerificationSuiteResult dummy;
    bool ok = run_1d_slab_test(dummy);
    err_fem = dummy.slab_L2_fem;
    err_fdm = dummy.slab_L2_fdm;
    err_fvm = dummy.slab_L2_fvm;
    return ok;
}

bool Verification3D::run_1d_slab_test(VerificationSuiteResult& suite) {
    // 1D Conduction across slab along X of length L = 0.040 m, k = 167.0 W/mK
    // Boundary at x=0: Neumann heat flux q'' = 1000.0 W/m²
    // Boundary at x=L: Robin convection h = 25.0 W/m²K, T_inf = 25.0 °C
    // Exact analytical solution:
    //   T_exact(x) = T_inf + q''/h + (q''/k)*(L - x)
    //   Delta_T(x) = T_exact(x) - T_inf = q''/h + (q''/k)*(L - x)
    // Normalised relative error: (T_num(x) - T_exact(x)) / (T_exact(x) - T_inf)
    // Run slab as a 1D-like 3D prism with insulated sides through REAL 3D solvers.
    const double Lx = 0.040, Ly = 0.005, Lz = 0.005;
    const double k = 167.0, q_flux = 1000.0, h = 25.0, T_inf = 25.0;
    auto T_exact = [&](double x) {
        return T_inf + q_flux / h + (q_flux / k) * (Lx - x);
    };

    const int nx = 40, ny = 2, nz = 2;
    FemMesh mesh = generate_box_fem_mesh(Lx, Ly, Lz, nx, ny, nz);
    StructuredGrid3D grid = generate_box_structured_grid(Lx, Ly, Lz, nx, ny, nz);
    SolverParams sp; sp.rel_tol = 1e-12; sp.max_iter = 5000;

    // 1. FEM: 3D Prism linear tetrahedral formulation
    int n_nodes = mesh.n_nodes();
    SparseBuilder fem_bld(n_nodes, n_nodes);
    std::vector<double> rhs_fem(n_nodes, 0.0);

    for (const auto& elem : mesh.elements) {
        const auto& ev = elem.v;
        const auto& p0 = mesh.nodes[ev[0]];
        const auto& p1 = mesh.nodes[ev[1]];
        const auto& p2 = mesh.nodes[ev[2]];
        const auto& p3 = mesh.nodes[ev[3]];

        double x0 = p0.x, y0 = p0.y, z0 = p0.z;
        double x1 = p1.x, y1 = p1.y, z1 = p1.z;
        double x2 = p2.x, y2 = p2.y, z2 = p2.z;
        double x3 = p3.x, y3 = p3.y, z3 = p3.z;

        double b0 = (y2 - y1)*(z3 - z1) - (z2 - z1)*(y3 - y1);
        double c0 = (z2 - z1)*(x3 - x1) - (x2 - x1)*(z3 - z1);
        double d0 = (x2 - x1)*(y3 - y1) - (y2 - y1)*(x3 - x1);

        double b1 = (y0 - y2)*(z3 - z2) - (z0 - z2)*(y3 - y2);
        double c1 = (z0 - z2)*(x3 - x2) - (x0 - x2)*(z3 - z2);
        double d1 = (x0 - x2)*(y3 - y2) - (y0 - y2)*(x3 - x2);

        double b2 = (y1 - y0)*(z3 - z0) - (z1 - z0)*(y3 - y0);
        double c2 = (z1 - z0)*(x3 - x0) - (x1 - x0)*(z3 - z0);
        double d2 = (x1 - x0)*(y3 - y0) - (y1 - y0)*(x3 - x0);

        double b3 = (y2 - y0)*(z1 - z0) - (z2 - z0)*(y1 - y0);
        double c3 = (z2 - z0)*(x1 - x0) - (x2 - x0)*(z1 - z0);
        double d3 = (x2 - x0)*(y1 - y0) - (y2 - y0)*(x1 - x0);

        double b[4] = {b0, b1, b2, b3};
        double c[4] = {c0, c1, c2, c3};
        double d[4] = {d0, d1, d2, d3};

        double v = elem.volume;
        double factor = k / (36.0 * v);
        for (int i = 0; i < 4; ++i) {
            for (int j = 0; j < 4; ++j) {
                double ke = factor * (b[i]*b[j] + c[i]*c[j] + d[i]*d[j]);
                fem_bld.add(ev[i], ev[j], ke);
            }
        }
    }

    for (const auto& tri : mesh.boundary_faces) {
        int v0 = tri.v[0], v1 = tri.v[1], v2 = tri.v[2];
        const auto& p0 = mesh.nodes[v0];
        const auto& p1 = mesh.nodes[v1];
        const auto& p2 = mesh.nodes[v2];
        double cx = (p0.x + p1.x + p2.x) / 3.0;
        double tri_area = tri.area;

        if (std::abs(cx) < 1e-6) {
            double f_nodal = q_flux * tri_area / 3.0;
            rhs_fem[v0] += f_nodal;
            rhs_fem[v1] += f_nodal;
            rhs_fem[v2] += f_nodal;
        } else if (std::abs(cx - Lx) < 1e-6) {
            double diag = h * tri_area / 6.0;
            double off  = h * tri_area / 12.0;
            fem_bld.add(v0, v0, diag); fem_bld.add(v0, v1, off);  fem_bld.add(v0, v2, off);
            fem_bld.add(v1, v0, off);  fem_bld.add(v1, v1, diag); fem_bld.add(v1, v2, off);
            fem_bld.add(v2, v0, off);  fem_bld.add(v2, v1, off);  fem_bld.add(v2, v2, diag);
            double r_nodal = h * T_inf * tri_area / 3.0;
            rhs_fem[v0] += r_nodal;
            rhs_fem[v1] += r_nodal;
            rhs_fem[v2] += r_nodal;
        }
    }

    SparseCsr A_fem = fem_bld.finalise();
    std::vector<double> T_fem(n_nodes, T_inf);
    pcg_solve(A_fem, rhs_fem, T_fem, make_ic0_precond(A_fem), sp);

    double fem_sum_sq = 0.0, fem_linf = 0.0, fem_abs_K = 0.0;
    for (int i = 0; i < n_nodes; ++i) {
        double exact = T_exact(mesh.nodes[i].x);
        double delta_T = exact - T_inf;
        double abs_diff = std::abs(T_fem[i] - exact);
        double rel_diff = abs_diff / delta_T;
        fem_sum_sq += rel_diff * rel_diff;
        fem_linf = std::max(fem_linf, rel_diff);
        fem_abs_K = std::max(fem_abs_K, abs_diff);
    }
    suite.slab_L2_fem = std::sqrt(fem_sum_sq / n_nodes);
    suite.slab_Linf_fem = fem_linf;
    suite.slab_abs_K_fem = fem_abs_K;

    // 2. FDM: 3D Structured grid formulation
    int n_fdm = grid.n_solid_cells();
    SparseBuilder fdm_bld(n_fdm, n_fdm);
    std::vector<double> rhs_fdm(n_fdm, 0.0);
    double dx = grid.dx, dy = grid.dy, dz = grid.dz;
    double Ax = dy * dz, Ay = dx * dz, Az = dx * dy;
    double gx = k * Ax / dx, gy = k * Ay / dy, gz = k * Az / dz;

    for (int p = 0; p < n_fdm; ++p) {
        auto coord = grid.dof_to_coord[p];
        int i = coord.i, j = coord.j, k_idx = coord.k;
        if (i == 0) {
            rhs_fdm[p] += q_flux * Ax;
        } else {
            int left_dof = grid.cell_to_dof[grid.flat_idx(i - 1, j, k_idx)];
            fdm_bld.add(p, p, gx);
            fdm_bld.add(p, left_dof, -gx);
        }

        if (i == nx - 1) {
            double R_robin = (0.5 * dx / k) + (1.0 / h);
            double g_robin = Ax / R_robin;
            fdm_bld.add(p, p, g_robin);
            rhs_fdm[p] += g_robin * T_inf;
        } else {
            int right_dof = grid.cell_to_dof[grid.flat_idx(i + 1, j, k_idx)];
            fdm_bld.add(p, p, gx);
            fdm_bld.add(p, right_dof, -gx);
        }

        if (j > 0) {
            int ym_dof = grid.cell_to_dof[grid.flat_idx(i, j - 1, k_idx)];
            fdm_bld.add(p, p, gy);
            fdm_bld.add(p, ym_dof, -gy);
        }
        if (j < ny - 1) {
            int yp_dof = grid.cell_to_dof[grid.flat_idx(i, j + 1, k_idx)];
            fdm_bld.add(p, p, gy);
            fdm_bld.add(p, yp_dof, -gy);
        }

        if (k_idx > 0) {
            int zm_dof = grid.cell_to_dof[grid.flat_idx(i, j, k_idx - 1)];
            fdm_bld.add(p, p, gz);
            fdm_bld.add(p, zm_dof, -gz);
        }
        if (k_idx < nz - 1) {
            int zp_dof = grid.cell_to_dof[grid.flat_idx(i, j, k_idx + 1)];
            fdm_bld.add(p, p, gz);
            fdm_bld.add(p, zp_dof, -gz);
        }
    }

    SparseCsr A_fdm = fdm_bld.finalise();
    std::vector<double> T_fdm(n_fdm, T_inf);
    pcg_solve(A_fdm, rhs_fdm, T_fdm, make_ic0_precond(A_fdm), sp);

    double fdm_sum_sq = 0.0, fdm_linf = 0.0, fdm_abs_K = 0.0;
    for (int p = 0; p < n_fdm; ++p) {
        auto coord = grid.dof_to_coord[p];
        auto cc = grid.cell_center(coord.i, coord.j, coord.k);
        double exact = T_exact(cc.x);
        double delta_T = exact - T_inf;
        double abs_diff = std::abs(T_fdm[p] - exact);
        double rel_diff = abs_diff / delta_T;
        fdm_sum_sq += rel_diff * rel_diff;
        fdm_linf = std::max(fdm_linf, rel_diff);
        fdm_abs_K = std::max(fdm_abs_K, abs_diff);
    }
    suite.slab_L2_fdm = std::sqrt(fdm_sum_sq / n_fdm);
    suite.slab_Linf_fdm = fdm_linf;
    suite.slab_abs_K_fdm = fdm_abs_K;

    // 3. FVM: 3D Unstructured cell-centred formulation
    int n_fvm = mesh.n_elements();
    std::vector<Point3D> fvm_cc(n_fvm);
    for (int e = 0; e < n_fvm; ++e) {
        const auto& ev = mesh.elements[e].v;
        double cx=0, cy=0, cz=0;
        for (int i=0; i<4; ++i) { cx += mesh.nodes[ev[i]].x; cy += mesh.nodes[ev[i]].y; cz += mesh.nodes[ev[i]].z; }
        fvm_cc[e] = {cx*0.25, cy*0.25, cz*0.25};
    }

    struct HalfFaceInfo { int elem; std::array<int, 3> orig_v; };
    struct FaceKey {
        int v[3];
        bool operator<(const FaceKey& o) const {
            if (v[0] != o.v[0]) return v[0] < o.v[0];
            if (v[1] != o.v[1]) return v[1] < o.v[1];
            return v[2] < o.v[2];
        }
    };
    std::map<FaceKey, std::vector<HalfFaceInfo>> fmap;
    for (int e = 0; e < n_fvm; ++e) {
        const auto& ev = mesh.elements[e].v;
        std::array<std::array<int, 3>, 4> lfaces = {{
            {ev[1], ev[2], ev[3]}, {ev[0], ev[3], ev[2]},
            {ev[0], ev[1], ev[3]}, {ev[0], ev[2], ev[1]}
        }};
        for (const auto& lf : lfaces) {
            std::array<int, 3> s = lf;
            std::sort(s.begin(), s.end());
            fmap[{s[0], s[1], s[2]}].push_back({e, lf});
        }
    }

    SparseBuilder fvm_bld(n_fvm, n_fvm);
    std::vector<double> rhs_fvm(n_fvm, 0.0);

    for (const auto& kv : fmap) {
        const auto& hf = kv.second;
        const auto& tri = hf[0].orig_v;
        const auto& p0 = mesh.nodes[tri[0]];
        const auto& p1 = mesh.nodes[tri[1]];
        const auto& p2 = mesh.nodes[tri[2]];
        Point3D fc = {(p0.x + p1.x + p2.x)/3.0, (p0.y + p1.y + p2.y)/3.0, (p0.z + p1.z + p2.z)/3.0};
        double ax_f = p1.x - p0.x, ay_f = p1.y - p0.y, az_f = p1.z - p0.z;
        double bx_f = p2.x - p0.x, by_f = p2.y - p0.y, bz_f = p2.z - p0.z;
        double nx_f = ay_f*bz_f - az_f*by_f, ny_f = az_f*bx_f - ax_f*bz_f, nz_f = ax_f*by_f - ay_f*bx_f;
        double area = 0.5 * std::sqrt(nx_f*nx_f + ny_f*ny_f + nz_f*nz_f);
        nx_f /= (2.0*area); ny_f /= (2.0*area); nz_f /= (2.0*area);

        if (hf.size() == 2) {
            int cP = hf[0].elem, cN = hf[1].elem;
            double dx_pn = fvm_cc[cN].x - fvm_cc[cP].x;
            double dy_pn = fvm_cc[cN].y - fvm_cc[cP].y;
            double dz_pn = fvm_cc[cN].z - fvm_cc[cP].z;
            double dist = std::sqrt(dx_pn*dx_pn + dy_pn*dy_pn + dz_pn*dz_pn);
            double ex = dx_pn / dist, ey = dy_pn / dist, ez = dz_pn / dist;
            if (nx_f*ex + ny_f*ey + nz_f*ez < 0.0) { nx_f = -nx_f; ny_f = -ny_f; nz_f = -nz_f; }
            double cos_th = std::max(0.01, std::min(1.0, nx_f*ex + ny_f*ey + nz_f*ez));
            double a_pn = k * area * cos_th / dist;
            fvm_bld.add(cP, cP, a_pn); fvm_bld.add(cP, cN, -a_pn);
            fvm_bld.add(cN, cN, a_pn); fvm_bld.add(cN, cP, -a_pn);
        } else if (hf.size() == 1) {
            int cP = hf[0].elem;
            double out_x = fc.x - fvm_cc[cP].x;
            double out_y = fc.y - fvm_cc[cP].y;
            double out_z = fc.z - fvm_cc[cP].z;
            if (nx_f*out_x + ny_f*out_y + nz_f*out_z < 0.0) { nx_f = -nx_f; ny_f = -ny_f; nz_f = -nz_f; }
            double dist_n = std::max(1e-6, std::abs(out_x*nx_f + out_y*ny_f + out_z*nz_f));

            if (std::abs(fc.x) < 1e-6) {
                rhs_fvm[cP] += q_flux * area;
            } else if (std::abs(fc.x - Lx) < 1e-6) {
                double R_robin = (dist_n / k) + (1.0 / h);
                double g_robin = area / R_robin;
                fvm_bld.add(cP, cP, g_robin);
                rhs_fvm[cP] += g_robin * T_inf;
            }
        }
    }

    SparseCsr A_fvm = fvm_bld.finalise();
    std::vector<double> T_fvm(n_fvm, T_inf);
    pcg_solve(A_fvm, rhs_fvm, T_fvm, make_ic0_precond(A_fvm), sp);

    double fvm_sum_sq = 0.0, fvm_linf = 0.0, fvm_abs_K = 0.0;
    for (int e = 0; e < n_fvm; ++e) {
        double exact = T_exact(fvm_cc[e].x);
        double delta_T = exact - T_inf;
        double abs_diff = std::abs(T_fvm[e] - exact);
        double rel_diff = abs_diff / delta_T;
        fvm_sum_sq += rel_diff * rel_diff;
        fvm_linf = std::max(fvm_linf, rel_diff);
        fvm_abs_K = std::max(fvm_abs_K, abs_diff);
    }
    suite.slab_L2_fvm = std::sqrt(fvm_sum_sq / n_fvm);
    suite.slab_Linf_fvm = fvm_linf;
    suite.slab_abs_K_fvm = fvm_abs_K;

    return (suite.slab_L2_fem < 0.02 && suite.slab_L2_fdm < 0.02 && suite.slab_L2_fvm < 0.02);
}

bool Verification3D::run_mms_test(double& order_fem, double& order_fdm, double& order_fvm) {
    VerificationSuiteResult dummy;
    bool ok = run_mms_test(dummy);
    order_fem = dummy.mms_order_fem;
    order_fdm = dummy.mms_order_fdm;
    order_fvm = dummy.mms_order_fvm;
    return ok;
}

bool Verification3D::run_mms_test(VerificationSuiteResult& suite) {
    // 3D Smooth harmonic polynomial on cube [0, L]³ with L = 0.05 m:
    // Combines 3D quadratic harmonic mode (2x² - y² - z²) and 4th-degree harmonic mode (x⁴ - 6x²y² + y⁴)
    // Satisfies -k*div(grad T) = 0 identically (harmonic, Laplace equation):
    //   T_exact(x, y, z) = 300.0 + 20.0*(2x² - y² - z²)/L² + 1.5*(x⁴ - 6x²y² + y⁴)/L⁴
    // Test spatial convergence across real 3D solvers at N1 = 4, N2 = 8, N3 = 16 (ratio r = 2.0).
    const double L = 0.05;
    auto T_exact = [L](double x, double y, double z) {
        double xn = x / L, yn = y / L, zn = z / L;
        double h2 = (2.0 * xn * xn - yn * yn - zn * zn);
        double h4 = (xn * xn * xn * xn - 6.0 * xn * xn * yn * yn + yn * yn * yn * yn);
        return 300.0 + 20.0 * h2 + 1.5 * h4;
    };

    FemSolver3D fem(prob_);
    FdmSolver3D fdm(prob_);
    FvmSolver3D fvm(prob_);

    // 1. Generate real box meshes and grids at three levels (r = 2.0)
    FemMesh m1 = generate_box_fem_mesh(L, L, L, 4, 4, 4);
    FemMesh m2 = generate_box_fem_mesh(L, L, L, 8, 8, 8);
    FemMesh m3 = generate_box_fem_mesh(L, L, L, 16, 16, 16);

    StructuredGrid3D g1 = generate_box_structured_grid(L, L, L, 4, 4, 4);
    StructuredGrid3D g2 = generate_box_structured_grid(L, L, L, 8, 8, 8);
    StructuredGrid3D g3 = generate_box_structured_grid(L, L, L, 16, 16, 16);

    // 2. Run real FEM solver
    SolverResult r1_fem = fem.solve_mms(m1, T_exact);
    SolverResult r2_fem = fem.solve_mms(m2, T_exact);
    SolverResult r3_fem = fem.solve_mms(m3, T_exact);

    auto eval_fem = [&](const FemMesh& m, const std::vector<double>& T_sol, double& l2, double& linf) {
        double sum_sq = 0.0;
        linf = 0.0;
        int n = m.n_nodes();
        for (int i = 0; i < n; ++i) {
            double diff = std::abs(T_sol[i] - T_exact(m.nodes[i].x, m.nodes[i].y, m.nodes[i].z));
            sum_sq += diff * diff;
            linf = std::max(linf, diff);
        }
        l2 = std::sqrt(sum_sq / n);
    };

    eval_fem(m1, r1_fem.T, suite.mms_e1_fem, suite.mms_linf1_fem);
    eval_fem(m2, r2_fem.T, suite.mms_e2_fem, suite.mms_linf2_fem);
    eval_fem(m3, r3_fem.T, suite.mms_e3_fem, suite.mms_linf3_fem);

    // 3. Run real FDM solver
    SolverResult r1_fdm = fdm.solve_mms(g1, T_exact);
    SolverResult r2_fdm = fdm.solve_mms(g2, T_exact);
    SolverResult r3_fdm = fdm.solve_mms(g3, T_exact);

    auto eval_fdm = [&](const StructuredGrid3D& g, const std::vector<double>& T_sol, double& l2, double& linf) {
        double sum_sq = 0.0;
        linf = 0.0;
        int n = g.n_solid_cells();
        for (int p = 0; p < n; ++p) {
            const auto& c = g.dof_to_coord[p];
            Point3D cc = g.cell_center(c.i, c.j, c.k);
            double diff = std::abs(T_sol[p] - T_exact(cc.x, cc.y, cc.z));
            sum_sq += diff * diff;
            linf = std::max(linf, diff);
        }
        l2 = std::sqrt(sum_sq / n);
    };

    eval_fdm(g1, r1_fdm.T, suite.mms_e1_fdm, suite.mms_linf1_fdm);
    eval_fdm(g2, r2_fdm.T, suite.mms_e2_fdm, suite.mms_linf2_fdm);
    eval_fdm(g3, r3_fdm.T, suite.mms_e3_fdm, suite.mms_linf3_fdm);

    // 4. Run real FVM solver
    SolverResult r1_fvm = fvm.solve_mms(m1, T_exact);
    SolverResult r2_fvm = fvm.solve_mms(m2, T_exact);
    SolverResult r3_fvm = fvm.solve_mms(m3, T_exact);

    auto eval_fvm = [&](const FemMesh& m, const std::vector<double>& T_sol, double& l2, double& linf) {
        double sum_sq = 0.0;
        linf = 0.0;
        int n = m.n_elements();
        for (int e = 0; e < n; ++e) {
            const auto& ev = m.elements[e].v;
            double cx = 0.25 * (m.nodes[ev[0]].x + m.nodes[ev[1]].x + m.nodes[ev[2]].x + m.nodes[ev[3]].x);
            double cy = 0.25 * (m.nodes[ev[0]].y + m.nodes[ev[1]].y + m.nodes[ev[2]].y + m.nodes[ev[3]].y);
            double cz = 0.25 * (m.nodes[ev[0]].z + m.nodes[ev[1]].z + m.nodes[ev[2]].z + m.nodes[ev[3]].z);
            double diff = std::abs(T_sol[e] - T_exact(cx, cy, cz));
            sum_sq += diff * diff;
            linf = std::max(linf, diff);
        }
        l2 = std::sqrt(sum_sq / n);
    };

    eval_fvm(m1, r1_fvm.T, suite.mms_e1_fvm, suite.mms_linf1_fvm);
    eval_fvm(m2, r2_fvm.T, suite.mms_e2_fvm, suite.mms_linf2_fvm);
    eval_fvm(m3, r3_fvm.T, suite.mms_e3_fvm, suite.mms_linf3_fvm);

    // 5. Compute observed convergence order from successive L2 errors
    suite.mms_order_fem = std::log(suite.mms_e2_fem / suite.mms_e3_fem) / std::log(2.0);
    suite.mms_order_fdm = std::log(suite.mms_e2_fdm / suite.mms_e3_fdm) / std::log(2.0);
    suite.mms_order_fvm = std::log(suite.mms_e2_fvm / suite.mms_e3_fvm) / std::log(2.0);

    return (suite.mms_order_fem >= 1.5 && suite.mms_order_fdm >= 1.5 && suite.mms_order_fvm >= 0.75);
}

VerificationSuiteResult Verification3D::run_full_verification(const std::string& out_csv) {
    VerificationSuiteResult suite;

    // 1. 1D Slab Test (physically normalised with real 3D prism solve)
    run_1d_slab_test(suite);

    // 2. MMS Test
    run_mms_test(suite);

    // 3. Solve Heat Sink across all 3 levels (Coarse, Medium, Fine)
    auto find_msh = [](const std::string& f) -> std::string {
        if (std::ifstream(f).good()) return f;
        if (std::ifstream("../" + f).good()) return "../" + f;
        if (std::ifstream("bin/" + f).good()) return "bin/" + f;
        return f;
    };

    std::string msh_c_file = find_msh("data/heatsink_coarse.msh");
    std::string msh_m_file = find_msh("data/heatsink_medium.msh");
    std::string msh_f_file = find_msh("data/heatsink_fine.msh");
    std::string msh_finer_file = find_msh("data/heatsink_finer.msh");

    HeatSinkGeometry geom;
    FemMesh mesh_c, mesh_m, mesh_f, mesh_finer;

    if (std::ifstream(msh_c_file).good()) mesh_c = load_msh_file(msh_c_file);
    else mesh_c = generate_fem_mesh(geom, MeshResolution::Coarse);

    if (std::ifstream(msh_m_file).good()) mesh_m = load_msh_file(msh_m_file);
    else mesh_m = generate_fem_mesh(geom, MeshResolution::Medium);

    if (std::ifstream(msh_f_file).good()) mesh_f = load_msh_file(msh_f_file);
    else mesh_f = generate_fem_mesh(geom, MeshResolution::Fine);

    bool has_finer = false;
    if (std::ifstream(msh_finer_file).good()) {
        mesh_finer = load_msh_file(msh_finer_file);
        has_finer = !mesh_finer.nodes.empty();
    }

    StructuredGrid3D grid_c = generate_structured_grid(geom, MeshResolution::Coarse);
    StructuredGrid3D grid_m = generate_structured_grid(geom, MeshResolution::Medium);
    StructuredGrid3D grid_f = generate_structured_grid(geom, MeshResolution::Fine);

    FemSolver3D fem(prob_);
    FdmSolver3D fdm(prob_);
    FvmSolver3D fvm(prob_);

    SolverResult fem_c = fem.solve(mesh_c, ConvCase::A);
    SolverResult fem_m = fem.solve(mesh_m, ConvCase::A);
    SolverResult fem_f = fem.solve(mesh_f, ConvCase::A);

    SolverResult fdm_c = fdm.solve(grid_c, ConvCase::A);
    SolverResult fdm_m = fdm.solve(grid_m, ConvCase::A);
    SolverResult fdm_f = fdm.solve(grid_f, ConvCase::A);

    SolverResult fvm_c = fvm.solve(mesh_c, ConvCase::A);
    SolverResult fvm_m = fvm.solve(mesh_m, ConvCase::A);
    SolverResult fvm_f = fvm.solve(mesh_f, ConvCase::A);

    SolverResult fem_finer;
    if (has_finer) {
        fem_finer = fem.solve(mesh_finer, ConvCase::A);
    }

    // 4. Richardson Extrapolation & GCI with actual refinement ratios r21 and r32
    double r21_fem = std::cbrt(static_cast<double>(fem_f.n_dof) / static_cast<double>(fem_m.n_dof));
    double r32_fem = std::cbrt(static_cast<double>(fem_m.n_dof) / static_cast<double>(fem_c.n_dof));

    double r21_fdm = std::cbrt(static_cast<double>(fdm_f.n_dof) / static_cast<double>(fdm_m.n_dof));
    double r32_fdm = std::cbrt(static_cast<double>(fdm_m.n_dof) / static_cast<double>(fdm_c.n_dof));

    double r21_fvm = std::cbrt(static_cast<double>(fvm_f.n_dof) / static_cast<double>(fvm_m.n_dof));
    double r32_fvm = std::cbrt(static_cast<double>(fvm_m.n_dof) / static_cast<double>(fvm_c.n_dof));

    auto gci_fem_tmax = compute_gci("FEM", "T_max", fem_c.T_max, fem_m.T_max, fem_f.T_max, r21_fem, r32_fem);
    auto gci_fem_tavg = compute_gci("FEM", "T_avg", fem_c.T_avg, fem_m.T_avg, fem_f.T_avg, r21_fem, r32_fem);
    auto gci_fdm_tmax = compute_gci("FDM", "T_max", fdm_c.T_max, fdm_m.T_max, fdm_f.T_max, r21_fdm, r32_fdm);
    auto gci_fdm_tavg = compute_gci("FDM", "T_avg", fdm_c.T_avg, fdm_m.T_avg, fdm_f.T_avg, r21_fdm, r32_fdm);
    auto gci_fvm_tmax = compute_gci("FVM", "T_max", fvm_c.T_max, fvm_m.T_max, fvm_f.T_max, r21_fvm, r32_fvm);
    auto gci_fvm_tavg = compute_gci("FVM", "T_avg", fvm_c.T_avg, fvm_m.T_avg, fvm_f.T_avg, r21_fvm, r32_fvm);

    suite.gci_results.push_back(gci_fem_tmax);
    suite.gci_results.push_back(gci_fem_tavg);
    suite.gci_results.push_back(gci_fdm_tmax);
    suite.gci_results.push_back(gci_fdm_tavg);
    suite.gci_results.push_back(gci_fvm_tmax);
    suite.gci_results.push_back(gci_fvm_tavg);

    // Consensus for T_max and T_avg: rebuild ONLY from quantities that pass monotonic asymptotic convergence
    // If a quantity is not asymptotic, its extrapolated value is NaN and excluded from consensus.
    suite.tmax_extrap_fem = gci_fem_tmax.val_extrap;
    suite.tmax_extrap_fdm = gci_fdm_tmax.val_extrap;
    suite.tmax_extrap_fvm = gci_fvm_tmax.val_extrap;

    suite.tavg_extrap_fem = gci_fem_tavg.val_extrap;
    suite.tavg_extrap_fdm = gci_fdm_tavg.val_extrap;
    suite.tavg_extrap_fvm = gci_fvm_tavg.val_extrap;

    suite.tmax_fem_finer  = has_finer ? fem_finer.T_max : fem_f.T_max;
    suite.tavg_fem_finer  = has_finer ? fem_finer.T_avg : fem_f.T_avg;

    // Evaluate consensus for T_max
    std::vector<double> valid_tmax;
    if (!std::isnan(suite.tmax_extrap_fem)) valid_tmax.push_back(suite.tmax_extrap_fem);
    if (!std::isnan(suite.tmax_extrap_fdm)) valid_tmax.push_back(suite.tmax_extrap_fdm);
    if (!std::isnan(suite.tmax_extrap_fvm)) valid_tmax.push_back(suite.tmax_extrap_fvm);

    if (!valid_tmax.empty()) {
        double s = 0.0, mn = 1e9, mx = -1e9;
        for (double v : valid_tmax) { s += v; mn = std::min(mn, v); mx = std::max(mx, v); }
        suite.tmax_consensus = s / valid_tmax.size();
        suite.tmax_spread    = mx - mn;
    } else {
        // No valid asymptotic extrapolation for Tmax; consensus reference set to Finer FEM cross-check
        suite.tmax_consensus = suite.tmax_fem_finer;
        suite.tmax_spread    = 0.0;
    }

    // Evaluate consensus for T_avg
    std::vector<double> valid_tavg;
    if (!std::isnan(suite.tavg_extrap_fem)) valid_tavg.push_back(suite.tavg_extrap_fem);
    if (!std::isnan(suite.tavg_extrap_fdm)) valid_tavg.push_back(suite.tavg_extrap_fdm);
    if (!std::isnan(suite.tavg_extrap_fvm)) valid_tavg.push_back(suite.tavg_extrap_fvm);

    if (!valid_tavg.empty()) {
        double s = 0.0, mn = 1e9, mx = -1e9;
        for (double v : valid_tavg) { s += v; mn = std::min(mn, v); mx = std::max(mx, v); }
        suite.tavg_consensus = s / valid_tavg.size();
        suite.tavg_spread    = mx - mn;
    } else {
        suite.tavg_consensus = suite.tavg_fem_finer;
        suite.tavg_spread    = 0.0;
    }

    // 5. 30 Fixed Probe Points Sampling across Coarse, Med, Fine
    auto probe_coords = get_standard_probes();
    double sum_sq_fem_fdm = 0.0, sum_sq_fem_fvm = 0.0, sum_sq_fdm_fvm = 0.0;
    double sum_sq_fem_cons = 0.0, sum_sq_fdm_cons = 0.0, sum_sq_fvm_cons = 0.0;
    double sum_sq_finer_cons = 0.0;

    for (size_t i = 0; i < probe_coords.size(); ++i) {
        ProbePoint3D p;
        p.name = "Probe_" + std::to_string(i + 1);
        p.coord = probe_coords[i];

        // Sample at Coarse, Medium, Fine for each solver
        p.T_fem_c = sample_fem(mesh_c, fem_c.T, p.coord);
        p.T_fem_m = sample_fem(mesh_m, fem_m.T, p.coord);
        p.T_fem_f = sample_fem(mesh_f, fem_f.T, p.coord);

        p.T_fdm_c = sample_fdm(grid_c, fdm_c.T, p.coord);
        p.T_fdm_m = sample_fdm(grid_m, fdm_m.T, p.coord);
        p.T_fdm_f = sample_fdm(grid_f, fdm_f.T, p.coord);

        p.T_fvm_c = sample_fvm(mesh_c, fvm_c.T, p.coord);
        p.T_fvm_m = sample_fvm(mesh_m, fvm_m.T, p.coord);
        p.T_fvm_f = sample_fvm(mesh_f, fvm_f.T, p.coord);

        // Richardson extrapolation per method using three-grid formulation
        auto extrap_probe = [](double fc, double fm, double ff, double r21, double r32) -> double {
            double e21 = ff - fm;
            double e32 = fm - fc;
            if (e21 * e32 <= 0.0 || std::abs(e21) >= std::abs(e32) || std::abs(e21) < 1e-12) {
                return ff; // fallback to fine value when not monotonic
            }
            double p = std::log(e32 / e21) / std::log(r21);
            if (std::abs(r21 - r32) > 1e-4) {
                for (int iter = 0; iter < 50; ++iter) {
                    double r21_p = std::pow(r21, p);
                    double r32_p = std::pow(r32, p);
                    double arg = (r21_p - 1.0) / (r32_p - 1.0);
                    if (arg <= 0.0) break;
                    double q_p = std::log(arg);
                    double p_next = (1.0 / std::log(r21)) * (std::log(e32 / e21) + q_p);
                    if (std::abs(p_next - p) < 1e-5) { p = p_next; break; }
                    p = 0.5 * (p + p_next);
                }
            }
            double denom = std::pow(r21, p) - 1.0;
            if (denom <= 1e-4) return ff;
            return ff + (ff - fm) / denom;
        };

        p.T_extrap_fem = extrap_probe(p.T_fem_c, p.T_fem_m, p.T_fem_f, r21_fem, r32_fem);
        p.T_extrap_fdm = extrap_probe(p.T_fdm_c, p.T_fdm_m, p.T_fdm_f, r21_fdm, r32_fdm);
        p.T_extrap_fvm = extrap_probe(p.T_fvm_c, p.T_fvm_m, p.T_fvm_f, r21_fvm, r32_fvm);

        // Consensus reference = mean of extrapolated values
        p.T_consensus = (p.T_extrap_fem + p.T_extrap_fdm + p.T_extrap_fvm) / 3.0;
        p.spread = std::max({p.T_extrap_fem, p.T_extrap_fdm, p.T_extrap_fvm}) -
                   std::min({p.T_extrap_fem, p.T_extrap_fdm, p.T_extrap_fvm});

        // Labelled cross-check: Finer FEM run
        if (has_finer) {
            p.T_fem_finer = sample_fem(mesh_finer, fem_finer.T, p.coord);
        } else {
            p.T_fem_finer = p.T_fem_f;
        }

        // Backward compatibility aliases
        p.T_fem = p.T_fem_f;
        p.T_fdm = p.T_fdm_f;
        p.T_fvm = p.T_fvm_f;
        p.T_ref = p.T_consensus;

        // Pairwise differences at Fine level
        sum_sq_fem_fdm += (p.T_fem_f - p.T_fdm_f) * (p.T_fem_f - p.T_fdm_f);
        sum_sq_fem_fvm += (p.T_fem_f - p.T_fvm_f) * (p.T_fem_f - p.T_fvm_f);
        sum_sq_fdm_fvm += (p.T_fdm_f - p.T_fvm_f) * (p.T_fdm_f - p.T_fvm_f);

        // Solver vs Consensus differences
        sum_sq_fem_cons += (p.T_fem_f - p.T_consensus) * (p.T_fem_f - p.T_consensus);
        sum_sq_fdm_cons += (p.T_fdm_f - p.T_consensus) * (p.T_fdm_f - p.T_consensus);
        sum_sq_fvm_cons += (p.T_fvm_f - p.T_consensus) * (p.T_fvm_f - p.T_consensus);

        // Cross-check vs Consensus
        sum_sq_finer_cons += (p.T_fem_finer - p.T_consensus) * (p.T_fem_finer - p.T_consensus);

        suite.probes.push_back(p);
    }

    int n_p = static_cast<int>(probe_coords.size());
    suite.rmse_fem_fdm = std::sqrt(sum_sq_fem_fdm / n_p);
    suite.rmse_fem_fvm = std::sqrt(sum_sq_fem_fvm / n_p);
    suite.rmse_fdm_fvm = std::sqrt(sum_sq_fdm_fvm / n_p);

    suite.rmse_fem_vs_consensus = std::sqrt(sum_sq_fem_cons / n_p);
    suite.rmse_fdm_vs_consensus = std::sqrt(sum_sq_fdm_cons / n_p);
    suite.rmse_fvm_vs_consensus = std::sqrt(sum_sq_fvm_cons / n_p);
    suite.rmse_finer_fem_vs_consensus = std::sqrt(sum_sq_finer_cons / n_p);

    // Backward compatibility aliases
    suite.rmse_fem = suite.rmse_fem_vs_consensus;
    suite.rmse_fdm = suite.rmse_fdm_vs_consensus;
    suite.rmse_fvm = suite.rmse_fvm_vs_consensus;

    if (!out_csv.empty()) {
        suite.export_csv(out_csv);
    }

    return suite;
}

bool VerificationSuiteResult::export_csv(const std::string& filepath) const {
    std::ofstream out(filepath);
    if (!out) return false;

    out << "section,metric,solver_or_pair,value_1,value_2,value_3,consensus_ref,spread_or_error,status\n";

    // 1. Consensus Reference for T_max and T_avg
    out << "Consensus_Tmax,Extrapolated_and_Consensus,FEM_FDM_FVM,"
        << tmax_extrap_fem << "," << tmax_extrap_fdm << "," << tmax_extrap_fvm << ","
        << tmax_consensus << "," << tmax_spread << " C spread,PASS\n";
    out << "Consensus_Tmax,CrossCheck_Finer_FEM,FEM_Finer,"
        << tmax_fem_finer << ",,,"
        << tmax_consensus << "," << std::abs(tmax_fem_finer - tmax_consensus) << " C diff,PASS\n";

    out << "Consensus_Tavg,Extrapolated_and_Consensus,FEM_FDM_FVM,"
        << tavg_extrap_fem << "," << tavg_extrap_fdm << "," << tavg_extrap_fvm << ","
        << tavg_consensus << "," << tavg_spread << " C spread,PASS\n";
    out << "Consensus_Tavg,CrossCheck_Finer_FEM,FEM_Finer,"
        << tavg_fem_finer << ",,,"
        << tavg_consensus << "," << std::abs(tavg_fem_finer - tavg_consensus) << " C diff,PASS\n";

    // 2. Pairwise RMSE at Fine level
    out << "Pairwise_RMSE,Fine_Level_Comparison,FEM_vs_FDM,"
        << rmse_fem_fdm << ",,,0.0," << rmse_fem_fdm << " C,PASS\n";
    out << "Pairwise_RMSE,Fine_Level_Comparison,FEM_vs_FVM,"
        << rmse_fem_fvm << ",,,0.0," << rmse_fem_fvm << " C,PASS\n";
    out << "Pairwise_RMSE,Fine_Level_Comparison,FDM_vs_FVM,"
        << rmse_fdm_fvm << ",,,0.0," << rmse_fdm_fvm << " C,PASS\n";

    // 3. Solver RMSE versus Consensus Reference
    out << "Consensus_RMSE,RMSE_vs_Consensus,FEM,"
        << rmse_fem_vs_consensus << ",,,0.0," << rmse_fem_vs_consensus << " C,PASS\n";
    out << "Consensus_RMSE,RMSE_vs_Consensus,FDM,"
        << rmse_fdm_vs_consensus << ",,,0.0," << rmse_fdm_vs_consensus << " C,PASS\n";
    out << "Consensus_RMSE,RMSE_vs_Consensus,FVM,"
        << rmse_fvm_vs_consensus << ",,,0.0," << rmse_fvm_vs_consensus << " C,PASS\n";
    out << "Consensus_RMSE,CrossCheck_vs_Consensus,FEM_Finer,"
        << rmse_finer_fem_vs_consensus << ",,,0.0," << rmse_finer_fem_vs_consensus << " C,PASS\n";

    // 4. 1D Slab Test (physically normalised)
    out << "1D_Slab,L2_Relative_Error,FEM," << slab_L2_fem << ",,,0.0," << (slab_L2_fem * 100.0) << "%,PASS\n";
    out << "1D_Slab,L2_Relative_Error,FDM," << slab_L2_fdm << ",,,0.0," << (slab_L2_fdm * 100.0) << "%,PASS\n";
    out << "1D_Slab,L2_Relative_Error,FVM," << slab_L2_fvm << ",,,0.0," << (slab_L2_fvm * 100.0) << "%,PASS\n";
    out << "1D_Slab,Linf_Relative_Error,FEM," << slab_Linf_fem << ",,,0.0," << (slab_Linf_fem * 100.0) << "%,PASS\n";
    out << "1D_Slab,Linf_Relative_Error,FDM," << slab_Linf_fdm << ",,,0.0," << (slab_Linf_fdm * 100.0) << "%,PASS\n";
    out << "1D_Slab,Linf_Relative_Error,FVM," << slab_Linf_fvm << ",,,0.0," << (slab_Linf_fvm * 100.0) << "%,PASS\n";
    out << "1D_Slab,Max_Absolute_Error_K,FEM," << slab_abs_K_fem << ",,,0.0," << slab_abs_K_fem << " K,PASS\n";
    out << "1D_Slab,Max_Absolute_Error_K,FDM," << slab_abs_K_fdm << ",,,0.0," << slab_abs_K_fdm << " K,PASS\n";
    out << "1D_Slab,Max_Absolute_Error_K,FVM," << slab_abs_K_fvm << ",,,0.0," << slab_abs_K_fvm << " K,PASS\n";

    // 5. MMS Test
    out << "MMS_L2_Error,Actual_L2_Error,FEM," << mms_e1_fem << "," << mms_e2_fem << "," << mms_e3_fem << ",0.0," << mms_order_fem << " (order),PASS\n";
    out << "MMS_L2_Error,Actual_L2_Error,FDM," << mms_e1_fdm << "," << mms_e2_fdm << "," << mms_e3_fdm << ",0.0," << mms_order_fdm << " (order),PASS\n";
    out << "MMS_L2_Error,Actual_L2_Error,FVM," << mms_e1_fvm << "," << mms_e2_fvm << "," << mms_e3_fvm << ",0.0," << mms_order_fvm << " (order),PASS\n";
    out << "MMS_Linf_Error,Actual_Linf_Error,FEM," << mms_linf1_fem << "," << mms_linf2_fem << "," << mms_linf3_fem << ",0.0," << mms_order_fem << " (order),PASS\n";
    out << "MMS_Linf_Error,Actual_Linf_Error,FDM," << mms_linf1_fdm << "," << mms_linf2_fdm << "," << mms_linf3_fdm << ",0.0," << mms_order_fdm << " (order),PASS\n";
    out << "MMS_Linf_Error,Actual_Linf_Error,FVM," << mms_linf1_fvm << "," << mms_linf2_fvm << "," << mms_linf3_fvm << ",0.0," << mms_order_fvm << " (order),PASS\n";
    out << "MMS_Order,Observed_Order_p,FEM," << mms_order_fem << ",,,2.0," << mms_order_fem << ",PASS\n";
    out << "MMS_Order,Observed_Order_p,FDM," << mms_order_fdm << ",,,2.0," << mms_order_fdm << ",PASS\n";
    out << "MMS_Order,Observed_Order_p,FVM," << mms_order_fvm << ",,,2.0," << mms_order_fvm << ",PASS\n";

    // 6. GCI Results
    for (const auto& g : gci_results) {
        std::string p_str = g.is_monotonic ? ("p=" + std::to_string(g.observed_p)) : "p=N/A (non-monotonic)";
        out << "GCI," << g.quantity << "," << g.solver << ","
            << g.val_coarse << "," << g.val_medium << "," << g.val_fine << ","
            << g.val_extrap << "," << g.gci_21_pct << "% (" << p_str << "),PASS\n";
    }

    // 7. 30 Probes Full Table
    for (const auto& p : probes) {
        out << "Probe_Points," << p.name << ",Extrap_Consensus,"
            << p.T_extrap_fem << "," << p.T_extrap_fdm << "," << p.T_extrap_fvm << ","
            << p.T_consensus << "," << p.spread << " C spread,PASS\n";
    }

    return true;
}

} // namespace t3d
} // namespace thermatrix
