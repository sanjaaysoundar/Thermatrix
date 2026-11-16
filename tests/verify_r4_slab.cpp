// verify_r4_slab.cpp  --  Real-solver 1D-slab verification (REPLACES the hand-built 1D toy test).
// Exact solution: flux q'' into face s=0, Robin (h, Tinf) on face s=L, other four faces adiabatic.
//   T(s) = Tinf + q''/h + (q''/k) (L - s)
// This is run through the REAL FemSolver3D, FvmSolver3D and FdmSolver3D on:
//   (a) a regular Kuhn-tet box mesh   (b) a randomly perturbed (unstructured, non-orthogonal) tet mesh
//   (c) a Cartesian voxel grid.
// Errors are measured from each solver's own output. Nothing is derived from constants.
#include "../src/thermal3d/FemSolver3D.h"
#include "../src/thermal3d/FdmSolver3D.h"
#include "../src/thermal3d/FvmSolver3D.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <iostream>
#include <map>
#include <random>

using namespace thermatrix::t3d;

namespace {
struct Cfg { int n_cross; int n_axis; double Lcross; double L; int axis; double perturb; unsigned seed; };

double tet_signed_vol(const Point3D& a, const Point3D& b, const Point3D& c, const Point3D& d) {
    double ax = b.x - a.x, ay = b.y - a.y, az = b.z - a.z;
    double bx = c.x - a.x, by = c.y - a.y, bz = c.z - a.z;
    double cx = d.x - a.x, cy = d.y - a.y, cz = d.z - a.z;
    return (ax * (by * cz - bz * cy) - ay * (bx * cz - bz * cx) + az * (bx * cy - by * cx)) / 6.0;
}

// BC by position: flux at s=0 face, Robin at s=L face, adiabatic elsewhere
BcType bc_of(const Point3D& c, int axis, double L) {
    double s = (axis == 0) ? c.x : (axis == 1) ? c.y : c.z;
    if (std::abs(s) < 1e-9) return BcType::HeatFlux;
    if (std::abs(s - L) < 1e-9) return BcType::Robin;
    return BcType::Adiabatic;
}
double s_of(const Point3D& c, int axis) { return (axis == 0) ? c.x : (axis == 1) ? c.y : c.z; }

FemMesh make_box_mesh(const Cfg& cf) {
    int n[3]; double Ld[3];
    for (int a = 0; a < 3; ++a) { n[a] = (a == cf.axis) ? cf.n_axis : cf.n_cross; Ld[a] = (a == cf.axis) ? cf.L : cf.Lcross; }
    FemMesh m;
    auto nid = [&](int i, int j, int k) { return (k * (n[1] + 1) + j) * (n[0] + 1) + i; };
    std::mt19937 rng(cf.seed);
    std::uniform_real_distribution<double> U(-1.0, 1.0);
    for (int k = 0; k <= n[2]; ++k) for (int j = 0; j <= n[1]; ++j) for (int i = 0; i <= n[0]; ++i) {
        Point3D p{ i * Ld[0] / n[0], j * Ld[1] / n[1], k * Ld[2] / n[2] };
        bool interior = i > 0 && i < n[0] && j > 0 && j < n[1] && k > 0 && k < n[2];
        if (interior && cf.perturb > 0) {
            p.x += cf.perturb * (Ld[0] / n[0]) * U(rng);
            p.y += cf.perturb * (Ld[1] / n[1]) * U(rng);
            p.z += cf.perturb * (Ld[2] / n[2]) * U(rng);
        }
        m.nodes.push_back(p);
    }
    static const int perm[6][3] = { {0,1,2},{0,2,1},{1,0,2},{1,2,0},{2,0,1},{2,1,0} };
    for (int k = 0; k < n[2]; ++k) for (int j = 0; j < n[1]; ++j) for (int i = 0; i < n[0]; ++i) {
        for (int q = 0; q < 6; ++q) {
            int c[3] = { i, j, k };
            std::array<int, 4> v; v[0] = nid(c[0], c[1], c[2]);
            for (int s = 0; s < 3; ++s) { c[perm[q][s]]++; v[s + 1] = nid(c[0], c[1], c[2]); }
            double sv = tet_signed_vol(m.nodes[v[0]], m.nodes[v[1]], m.nodes[v[2]], m.nodes[v[3]]);
            if (sv < 0) std::swap(v[2], v[3]);
            TetElement e; e.v = v; e.volume = std::abs(sv);
            m.elements.push_back(e);
        }
    }
    // Boundary faces from tet faces that occur once
    std::map<std::array<int, 3>, std::pair<int, std::array<int, 3>>> fm; std::map<std::array<int, 3>, int> cnt;
    for (auto& e : m.elements) {
        static const int lf[4][3] = { {1,2,3},{0,3,2},{0,1,3},{0,2,1} };
        for (int f = 0; f < 4; ++f) {
            std::array<int, 3> t{ e.v[lf[f][0]], e.v[lf[f][1]], e.v[lf[f][2]] }; auto s = t; std::sort(s.begin(), s.end());
            cnt[s]++; fm[s] = { 0, t };
        }
    }
    for (auto& kv : cnt) if (kv.second == 1) {
        auto t = fm[kv.first].second;
        const auto &a = m.nodes[t[0]], &b = m.nodes[t[1]], &c = m.nodes[t[2]];
        double ux = b.x - a.x, uy = b.y - a.y, uz = b.z - a.z, vx = c.x - a.x, vy = c.y - a.y, vz = c.z - a.z;
        double nx = uy * vz - uz * vy, ny = uz * vx - ux * vz, nz = ux * vy - uy * vx;
        double A = 0.5 * std::sqrt(nx * nx + ny * ny + nz * nz);
        BoundaryTri bt; bt.v = t; bt.area = A;
        bt.normal = { nx / (2 * A), ny / (2 * A), nz / (2 * A) };
        Point3D cen{ (a.x + b.x + c.x) / 3, (a.y + b.y + c.y) / 3, (a.z + b.z + c.z) / 3 };
        bt.bc_type = bc_of(cen, cf.axis, cf.L);
        m.boundary_faces.push_back(bt);
    }
    return m;
}

StructuredGrid3D make_box_grid(const Cfg& cf) {
    StructuredGrid3D g; int n[3]; double Ld[3];
    for (int a = 0; a < 3; ++a) { n[a] = (a == cf.axis) ? cf.n_axis : cf.n_cross; Ld[a] = (a == cf.axis) ? cf.L : cf.Lcross; }
    g.nx = n[0]; g.ny = n[1]; g.nz = n[2]; g.dx = Ld[0] / n[0]; g.dy = Ld[1] / n[1]; g.dz = Ld[2] / n[2];
    int tot = g.nx * g.ny * g.nz; g.is_solid.assign(tot, true); g.cell_to_dof.assign(tot, -1);
    int dof = 0;
    for (int k = 0; k < g.nz; ++k) for (int j = 0; j < g.ny; ++j) for (int i = 0; i < g.nx; ++i) {
        g.cell_to_dof[g.flat_idx(i, j, k)] = dof++; g.dof_to_coord.push_back({ i, j, k });
    }
    const int di[6] = { -1,1,0,0,0,0 }, dj[6] = { 0,0,-1,1,0,0 }, dk[6] = { 0,0,0,0,-1,1 };
    const FaceDir dirs[6] = { FaceDir::Xminus, FaceDir::Xplus, FaceDir::Yminus, FaceDir::Yplus, FaceDir::Zminus, FaceDir::Zplus };
    double ar[6] = { g.dy * g.dz, g.dy * g.dz, g.dx * g.dz, g.dx * g.dz, g.dx * g.dy, g.dx * g.dy };
    for (int d_i = 0; d_i < g.n_solid_cells(); ++d_i) {
        auto c = g.dof_to_coord[d_i]; Point3D ce = g.cell_center(c.i, c.j, c.k);
        for (int d = 0; d < 6; ++d) {
            int ni = c.i + di[d], nj = c.j + dj[d], nk = c.k + dk[d];
            double fx = ce.x + 0.5 * di[d] * g.dx, fy = ce.y + 0.5 * dj[d] * g.dy, fz = ce.z + 0.5 * dk[d] * g.dz;
            if (ni >= 0 && ni < g.nx && nj >= 0 && nj < g.ny && nk >= 0 && nk < g.nz) {
                int nd = g.cell_to_dof[g.flat_idx(ni, nj, nk)];
                if (d_i < nd) g.internal_faces.push_back({ d_i, nd, dirs[d], ar[d], BcType::Adiabatic, fx, fy, fz });
            } else {
                g.boundary_faces.push_back({ d_i, -1, dirs[d], ar[d], bc_of({ fx, fy, fz }, cf.axis, cf.L), fx, fy, fz });
            }
        }
    }
    return g;
}

struct Err { double linf_rel, l2_rel, max_abs_K; };
Err measure(const std::vector<double>& T, const std::vector<double>& s, double q, double k, double h, double Tinf, double L) {
    Err e{ 0, 0, 0 }; double ss = 0;
    for (size_t i = 0; i < T.size(); ++i) {
        double ex = Tinf + q / h + q / k * (L - s[i]);
        double rel = (T[i] - ex) / (ex - Tinf);
        e.linf_rel = std::max(e.linf_rel, std::abs(rel)); ss += rel * rel;
        e.max_abs_K = std::max(e.max_abs_K, std::abs(T[i] - ex));
    }
    e.l2_rel = std::sqrt(ss / T.size()); return e;
}
} // namespace

int main(int argc, char** argv) {
    const char* out = (argc > 1) ? argv[1] : "slab_verification.csv";
    FILE* f = std::fopen(out, "w");
    std::fprintf(f, "mesh,axis,solver,corr_outer,corr_alpha,n_dof,Linf_rel,L2_rel,max_abs_K,iters,Q_balance_rel\n");
    const double k = 167.0, h = 25.0, Tinf = 25.0, q = 1000.0, L = 0.040, Lc = 0.012;
    struct Case { const char* name; double perturb; };
    Case cases[] = { {"regular_tet", 0.0}, {"perturbed_tet_0.30", 0.30} };
    int fails = 0;
    for (int axis = 0; axis < 3; ++axis) for (auto& cs : cases) {
        Cfg cf{ 3, 10, Lc, L, axis, cs.perturb, 12345u };
        double A_face = Lc * Lc;
        ThermalProblem prob; prob.k = k; prob.h_A = h; prob.T_inf = Tinf; prob.Q_total = q * A_face;
        FemMesh mesh = make_box_mesh(cf);
        SolverParams sp; sp.rel_tol = 1e-12; sp.max_iter = 20000;
        // FEM
        { FemSolver3D s(prob); auto r = s.solve(mesh, ConvCase::A, sp);
          std::vector<double> sv; for (auto& nd : mesh.nodes) sv.push_back(s_of(nd, axis));
          Err e = measure(r.T, sv, q, k, h, Tinf, L);
          std::fprintf(f, "%s,%d,FEM,-,-,%d,%.6e,%.6e,%.6e,%d,%.3e\n", cs.name, axis, r.n_dof, e.linf_rel, e.l2_rel, e.max_abs_K, r.iterations, r.Q_balance_rel); }
        // FVM, several correction settings
        struct Cc { int no; double al; };
        int NO = (argc > 2) ? std::atoi(argv[2]) : 40;
        Cc ccs[] = { {1, 0.0}, {3, 0.2}, {NO, 1.0} };
        for (auto cc : ccs) {
            FvmSolver3D s(prob); s.set_correction_params(cc.no, cc.al);
            auto r = s.solve(mesh, ConvCase::A, sp);
            std::vector<double> sv;
            for (auto& el : mesh.elements) {
                Point3D c{ 0,0,0 }; for (int v = 0; v < 4; ++v) { c.x += mesh.nodes[el.v[v]].x / 4; c.y += mesh.nodes[el.v[v]].y / 4; c.z += mesh.nodes[el.v[v]].z / 4; }
                sv.push_back(s_of(c, axis));
            }
            Err e = measure(r.T, sv, q, k, h, Tinf, L);
            std::fprintf(f, "%s,%d,FVM,%d,%.2f,%d,%.6e,%.6e,%.6e,%d,%.3e\n", cs.name, axis, cc.no, cc.al, r.n_dof, e.linf_rel, e.l2_rel, e.max_abs_K, r.iterations, r.Q_balance_rel);
            std::printf("axis %d %-20s FVM n_outer=%2d alpha=%.1f  Linf_rel=%.3e  max_abs=%.3e K  outer_loops_used=%zu last_dT=%.2e\n", axis, cs.name, cc.no, cc.al, e.linf_rel, e.max_abs_K, s.outer_history().size(), s.outer_history().empty()?0.0:s.outer_history().back());
        }
    }
    // FDM on Cartesian grid (all three axes)
    for (int axis = 0; axis < 3; ++axis) {
        Cfg cf{ 3, 10, Lc, L, axis, 0.0, 1u };
        double A_face = Lc * Lc;
        ThermalProblem prob; prob.k = k; prob.h_A = h; prob.T_inf = Tinf; prob.Q_total = q * A_face;
        StructuredGrid3D g = make_box_grid(cf);
        SolverParams sp; sp.rel_tol = 1e-12; sp.max_iter = 20000;
        FdmSolver3D s(prob); auto r = s.solve(g, ConvCase::A, sp);
        std::vector<double> sv; for (auto& c : g.dof_to_coord) sv.push_back(s_of(g.cell_center(c.i, c.j, c.k), axis));
        Err e = measure(r.T, sv, q, k, h, Tinf, L);
        std::fprintf(f, "cartesian_grid,%d,FDM,-,-,%d,%.6e,%.6e,%.6e,%d,%.3e\n", axis, r.n_dof, e.linf_rel, e.l2_rel, e.max_abs_K, r.iterations, r.Q_balance_rel);
        std::printf("axis %d cartesian FDM Linf_rel=%.3e\n", axis, e.linf_rel);
    }
    std::fclose(f); return fails;
}
