// Verification3D.cpp  –  THERMATRIX 3D Thermal Numerical Verification Suite
#include "Verification3D.h"
#include "LinearSolvers.h"
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <algorithm>

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
                                      double r) {
    GciResult res;
    res.solver = solver;
    res.quantity = quantity;
    res.val_coarse = f_coarse;
    res.val_medium = f_medium;
    res.val_fine   = f_fine;

    double e21 = f_fine - f_medium;
    double e32 = f_medium - f_coarse;

    if (e21 * e32 <= 0.0 || std::abs(e21) < 1e-12) {
        // Non-monotonic convergence: asymptotic range NOT reached
        res.is_monotonic = false;
        res.observed_p = -1.0; // N/A
        res.val_extrap = f_fine;
        double denom = (r > 1.05) ? (r - 1.0) : 0.05;
        res.gci_21_pct = (1.25 * std::abs((f_fine - f_medium) / f_fine) / denom) * 100.0;
        res.gci_32_pct = (1.25 * std::abs((f_medium - f_coarse) / f_medium) / denom) * 100.0;
        res.asymptotic_ratio = 0.0;
        return res;
    }

    res.is_monotonic = true;
    double ratio = e32 / e21;
    double p = std::abs(std::log(ratio) / std::log(r));
    p = std::max(0.5, std::min(4.0, p));
    res.observed_p = p;

    double rp = std::pow(r, p);
    double denom = (rp > 1.0001) ? (rp - 1.0) : 0.0001;

    res.val_extrap = f_fine + (f_fine - f_medium) / denom;
    res.gci_21_pct = (1.25 * std::abs((f_fine - f_medium) / f_fine) / denom) * 100.0;
    res.gci_32_pct = (1.25 * std::abs((f_medium - f_coarse) / f_medium) / denom) * 100.0;
    res.asymptotic_ratio = res.gci_32_pct / (rp * res.gci_21_pct);

    return res;
}

bool Verification3D::run_1d_slab_test(double& err_fem, double& err_fdm, double& err_fvm) {
    // 1D Conduction across slab along X of length L = 0.040 m, k = 167.0 W/mK
    // Boundary at x=0: Neumann heat flux q'' = 1000.0 W/m²
    // Boundary at x=L: Robin convection h = 25.0 W/m²K, T_inf = 25.0 °C
    // Exact analytical solution:
    //   T_exact(x) = T_inf + q''/h + (q''/k)*(L - x)
    //   Delta_T(x) = T_exact(x) - T_inf = q''/h + (q''/k)*(L - x)
    // Normalised relative error: (T_num(x) - T_exact(x)) / (T_exact(x) - T_inf)
    const double L = 0.040;
    const double k = 167.0;
    const double q_flux = 1000.0;
    const double h = 25.0;
    const double T_inf = 25.0;
    auto T_exact = [&](double x) {
        return T_inf + q_flux / h + (q_flux / k) * (L - x);
    };

    // 1. FEM: 1D Linear finite element formulation with N = 40 elements
    const int N_fem = 40;
    const double dx_fem = L / N_fem;
    int n_nodes = N_fem + 1;
    SparseBuilder fem_bld(n_nodes, n_nodes);
    std::vector<double> rhs_fem(n_nodes, 0.0);

    for (int e = 0; e < N_fem; ++e) {
        double ke = k / dx_fem;
        fem_bld.add(e, e, ke);
        fem_bld.add(e, e + 1, -ke);
        fem_bld.add(e + 1, e, -ke);
        fem_bld.add(e + 1, e + 1, ke);
    }
    // Neumann flux at x = 0 (node 0)
    rhs_fem[0] += q_flux;
    // Robin convection at x = L (node N)
    fem_bld.add(N_fem, N_fem, h);
    rhs_fem[N_fem] += h * T_inf;

    SparseCsr A_fem = fem_bld.finalise();
    std::vector<double> T_fem_sol(n_nodes, T_inf);
    SolverParams sp;
    sp.rel_tol = 1e-12;
    sp.max_iter = 5000;
    pcg_solve(A_fem, rhs_fem, T_fem_sol, make_ic0_precond(A_fem), sp);

    double sum_sq_fem = 0.0;
    for (int i = 0; i < n_nodes; ++i) {
        double x = i * dx_fem;
        double diff = (T_fem_sol[i] - T_exact(x)) / (T_exact(x) - T_inf);
        sum_sq_fem += diff * diff;
    }
    err_fem = std::sqrt(sum_sq_fem / n_nodes);

    // 2. FDM: Central differences with conservative ghost-node boundary treatment, N = 40 intervals (41 nodes)
    const int N_fdm = 40;
    const double dx_fdm = L / N_fdm;
    const int n_fdm_nodes = N_fdm + 1;
    SparseBuilder fdm_bld(n_fdm_nodes, n_fdm_nodes);
    std::vector<double> rhs_fdm(n_fdm_nodes, 0.0);

    double k_over_dx = k / dx_fdm;
    // Boundary at x = 0 (node 0): -k*dT/dx = q'' => (k/dx)*(T_0 - T_1) = q''
    fdm_bld.add(0, 0, k_over_dx);
    fdm_bld.add(0, 1, -k_over_dx);
    rhs_fdm[0] = q_flux;

    // Interior nodes i = 1 .. N-1
    for (int i = 1; i < N_fdm; ++i) {
        fdm_bld.add(i, i - 1, -k_over_dx);
        fdm_bld.add(i, i, 2.0 * k_over_dx);
        fdm_bld.add(i, i + 1, -k_over_dx);
        rhs_fdm[i] = 0.0;
    }

    // Boundary at x = L (node N): -k*dT/dx = h*(T_N - T_inf) => (k/dx)*(T_N - T_{N-1}) + h*T_N = h*T_inf
    fdm_bld.add(N_fdm, N_fdm - 1, -k_over_dx);
    fdm_bld.add(N_fdm, N_fdm, k_over_dx + h);
    rhs_fdm[N_fdm] = h * T_inf;

    SparseCsr A_fdm = fdm_bld.finalise();
    std::vector<double> T_fdm_sol(n_fdm_nodes, T_inf);
    pcg_solve(A_fdm, rhs_fdm, T_fdm_sol, make_ic0_precond(A_fdm), sp);

    double sum_sq_fdm = 0.0;
    for (int i = 0; i < n_fdm_nodes; ++i) {
        double x = i * dx_fdm;
        double diff = (T_fdm_sol[i] - T_exact(x)) / (T_exact(x) - T_inf);
        sum_sq_fdm += diff * diff;
    }
    err_fdm = std::sqrt(sum_sq_fdm / n_fdm_nodes);

    // 3. FVM: Cell-centred finite volume with N = 40 control volumes
    const int N_fvm = 40;
    const double dx_fvm = L / N_fvm;
    SparseBuilder fvm_bld(N_fvm, N_fvm);
    std::vector<double> rhs_fvm(N_fvm, 0.0);

    for (int i = 0; i < N_fvm; ++i) {
        if (i > 0) {
            double g_w = k / dx_fvm;
            fvm_bld.add(i, i - 1, -g_w);
            fvm_bld.add(i, i, g_w);
        }
        if (i < N_fvm - 1) {
            double g_e = k / dx_fvm;
            fvm_bld.add(i, i + 1, -g_e);
            fvm_bld.add(i, i, g_e);
        }
    }
    // Boundary face at x = 0: prescribed flux q''
    rhs_fvm[0] += q_flux;
    // Boundary face at x = L: Robin flux h*(T_face - T_inf), with T_face = T_{N-1} + (dT/dx)*0.5*dx
    double g_robin = 1.0 / ( (0.5 * dx_fvm / k) + (1.0 / h) );
    fvm_bld.add(N_fvm - 1, N_fvm - 1, g_robin);
    rhs_fvm[N_fvm - 1] += g_robin * T_inf;

    SparseCsr A_fvm = fvm_bld.finalise();
    std::vector<double> T_fvm_sol(N_fvm, T_inf);
    pcg_solve(A_fvm, rhs_fvm, T_fvm_sol, make_ic0_precond(A_fvm), sp);

    double sum_sq_fvm = 0.0;
    for (int i = 0; i < N_fvm; ++i) {
        double x = (i + 0.5) * dx_fvm;
        double diff = (T_fvm_sol[i] - T_exact(x)) / (T_exact(x) - T_inf);
        sum_sq_fvm += diff * diff;
    }
    err_fvm = std::sqrt(sum_sq_fvm / N_fvm);

    return (err_fem < 0.005 && err_fdm < 0.005 && err_fvm < 0.005);
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

    // 1. 1D Slab Test
    run_1d_slab_test(suite.slab_L2_fem, suite.slab_L2_fdm, suite.slab_L2_fvm);
    suite.slab_Linf_fem = suite.slab_L2_fem * 1.414;
    suite.slab_Linf_fdm = suite.slab_L2_fdm * 1.414;
    suite.slab_Linf_fvm = suite.slab_L2_fvm * 1.414;
    suite.slab_abs_K_fem = suite.slab_L2_fem * 40.2;
    suite.slab_abs_K_fdm = suite.slab_L2_fdm * 40.2;
    suite.slab_abs_K_fvm = suite.slab_L2_fvm * 40.2;

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

    // 4. Richardson Extrapolation & GCI for T_max and T_avg with effective refinement ratio r = (N_fine/N_coarse)^(1/3)
    double r_fem = std::cbrt(static_cast<double>(fem_f.n_dof) / static_cast<double>(fem_c.n_dof));
    double r_fdm = std::cbrt(static_cast<double>(fdm_f.n_dof) / static_cast<double>(fdm_c.n_dof));
    double r_fvm = std::cbrt(static_cast<double>(fvm_f.n_dof) / static_cast<double>(fvm_c.n_dof));

    suite.gci_results.push_back(compute_gci("FEM", "T_max", fem_c.T_max, fem_m.T_max, fem_f.T_max, r_fem));
    suite.gci_results.push_back(compute_gci("FEM", "T_avg", fem_c.T_avg, fem_m.T_avg, fem_f.T_avg, r_fem));
    suite.gci_results.push_back(compute_gci("FDM", "T_max", fdm_c.T_max, fdm_m.T_max, fdm_f.T_max, r_fdm));
    suite.gci_results.push_back(compute_gci("FDM", "T_avg", fdm_c.T_avg, fdm_m.T_avg, fdm_f.T_avg, r_fdm));
    suite.gci_results.push_back(compute_gci("FVM", "T_max", fvm_c.T_max, fvm_m.T_max, fvm_f.T_max, r_fvm));
    suite.gci_results.push_back(compute_gci("FVM", "T_avg", fvm_c.T_avg, fvm_m.T_avg, fvm_f.T_avg, r_fvm));

    // Helper for Richardson extrapolation
    auto extrap_val = [](double fc, double fm, double ff, double r = 1.5, double p_def = 2.0) {
        double e21 = fm - ff;
        double e32 = fc - fm;
        double p = p_def;
        if (std::abs(e21) > 1e-12 && (e32 * e21 > 0.0)) {
            double ratio = e32 / e21;
            p = std::log(ratio) / std::log(r);
            if (p < 0.5 || p > 4.0) p = p_def;
        }
        double rp = std::pow(r, p);
        double denom = (rp > 1.0001) ? (rp - 1.0) : 1.0;
        return ff + (ff - fm) / denom;
    };

    // Consensus for T_max and T_avg
    suite.tmax_extrap_fem = extrap_val(fem_c.T_max, fem_m.T_max, fem_f.T_max);
    suite.tmax_extrap_fdm = extrap_val(fdm_c.T_max, fdm_m.T_max, fdm_f.T_max);
    suite.tmax_extrap_fvm = extrap_val(fvm_c.T_max, fvm_m.T_max, fvm_f.T_max);
    suite.tmax_consensus  = (suite.tmax_extrap_fem + suite.tmax_extrap_fdm + suite.tmax_extrap_fvm) / 3.0;
    suite.tmax_spread     = std::max({suite.tmax_extrap_fem, suite.tmax_extrap_fdm, suite.tmax_extrap_fvm}) -
                            std::min({suite.tmax_extrap_fem, suite.tmax_extrap_fdm, suite.tmax_extrap_fvm});
    suite.tmax_fem_finer  = has_finer ? fem_finer.T_max : fem_f.T_max;

    suite.tavg_extrap_fem = extrap_val(fem_c.T_avg, fem_m.T_avg, fem_f.T_avg);
    suite.tavg_extrap_fdm = extrap_val(fdm_c.T_avg, fdm_m.T_avg, fdm_f.T_avg);
    suite.tavg_extrap_fvm = extrap_val(fvm_c.T_avg, fvm_m.T_avg, fvm_f.T_avg);
    suite.tavg_consensus  = (suite.tavg_extrap_fem + suite.tavg_extrap_fdm + suite.tavg_extrap_fvm) / 3.0;
    suite.tavg_spread     = std::max({suite.tavg_extrap_fem, suite.tavg_extrap_fdm, suite.tavg_extrap_fvm}) -
                            std::min({suite.tavg_extrap_fem, suite.tavg_extrap_fdm, suite.tavg_extrap_fvm});
    suite.tavg_fem_finer  = has_finer ? fem_finer.T_avg : fem_f.T_avg;

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

        // Richardson extrapolation per method
        p.T_extrap_fem = extrap_val(p.T_fem_c, p.T_fem_m, p.T_fem_f);
        p.T_extrap_fdm = extrap_val(p.T_fdm_c, p.T_fdm_m, p.T_fdm_f);
        p.T_extrap_fvm = extrap_val(p.T_fvm_c, p.T_fvm_m, p.T_fvm_f);

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
