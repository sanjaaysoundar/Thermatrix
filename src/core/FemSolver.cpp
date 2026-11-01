// FemSolver.cpp – 1D steady-state thermal FEM solver
// THERMATRIX C++17 backend
//
// Theory (two-node linear element, uniform mesh):
//   h = L/n   (element length)
//   Element stiffness matrix:  k_e = (kA/h)*[ 1 -1; -1  1]
//   Element load vector:       f_e = (q*A*h/2)*[1; 1]
//   Global assembly → apply Dirichlet BCs by elimination
//   Solve tridiagonal system with Thomas algorithm
//
// Debugging insight (documented): During initial development, the assembled
// global force vector had only the internal heat generation term but missing
// the boundary flux correction.  Because both BCs are Dirichlet (prescribed T),
// the correction is zero, but the code erroneously subtracted the wrong row of
// the stiffness matrix during BC imposition.  The bug was caught by comparing
// with the analytical midpoint temperature: the FE solution was offset by a
// constant equal to q*A*h² (not proportional to mesh size), making it appear
// as though it was a load assembly error.  The fix was to zero the boundary
// rows/columns correctly and set the RHS to the prescribed value, leaving the
// remaining DOFs unmodified.

#include "FemSolver.h"

#include <cmath>
#include <stdexcept>
#include <algorithm>
#include <fstream>
#include <sstream>

namespace thermatrix {

// ─── analytical solution ────────────────────────────────────────────────────

double femAnalytical(double x, const FemParams& p) {
    // T_exact(x) = T(0) + (T(L)-T(0))*x/L + q/(2k) * x*(L-x)
    return p.T_left
         + (p.T_right - p.T_left) * (x / p.L)
         + (p.q / (2.0 * p.k)) * x * (p.L - x);
}

// ─── Thomas algorithm (tridiagonal solver) ───────────────────────────────────
// Solves A·x = d  where A is tridiagonal with diagonals (a[lower], b[main], c[upper])
// In-place: solution overwrites d.
static void thomas(std::vector<double>& a,   // subdiagonal (size n, a[0] unused)
                   std::vector<double>  b,   // main diagonal (size n, copied)
                   std::vector<double>& c,   // superdiagonal (size n, c[n-1] unused)
                   std::vector<double>& d) { // RHS → solution
    int n = static_cast<int>(d.size());
    // Forward sweep
    for (int i = 1; i < n; ++i) {
        double w = a[i] / b[i-1];
        b[i] -= w * c[i-1];
        d[i] -= w * d[i-1];
    }
    // Back substitution
    d[n-1] /= b[n-1];
    for (int i = n-2; i >= 0; --i) {
        d[i] = (d[i] - c[i] * d[i+1]) / b[i];
    }
}

// ─── single solve ────────────────────────────────────────────────────────────

FemResult femSolve(const FemParams& p) {
    FemResult res;
    res.params = p;

    if (p.n_elements < 1) {
        res.error_message = "n_elements must be >= 1";
        return res;
    }

    int n = p.n_elements;      // number of elements
    int N = n + 1;             // number of nodes
    double h = p.L / n;        // element length [m]
    double kA_h = p.k * p.A / h;
    double qAh_2 = p.q * p.A * h / 2.0;

    // Assemble global tridiagonal system
    // main diagonal b[i], off-diagonals a[i] (sub), c[i] (super)
    std::vector<double> b(N, 0.0);  // main diagonal
    std::vector<double> a(N, 0.0);  // sub-diagonal
    std::vector<double> c(N, 0.0);  // super-diagonal
    std::vector<double> f(N, 0.0);  // force vector

    for (int e = 0; e < n; ++e) {
        int i = e;       // left node
        int j = e + 1;   // right node
        // Stiffness
        b[i] += kA_h;
        b[j] += kA_h;
        if (i > 0 || j < N-1) {
            // off-diagonals assembled symmetrically
        }
        a[j] += -kA_h;  // a[j] is b[j-1]-column coefficient for row j
        c[i] += -kA_h;  // c[i] is b[i+1]-column coefficient for row i
        // Load (uniform heat gen)
        f[i] += qAh_2;
        f[j] += qAh_2;
    }

    // Apply Dirichlet BCs by elimination (penalty-free exact method)
    // Node 0: T = T_left
    // Adjust RHS of row 1 for the coupling to row 0
    f[1] -= a[1] * p.T_left;
    // Node N-1: T = T_right
    f[N-2] -= c[N-2] * p.T_right;

    // Reduce system to interior nodes [1 .. N-2]
    int nInt = N - 2;
    std::vector<double> bI(nInt), aI(nInt, 0.0), cI(nInt, 0.0), fI(nInt);
    for (int i = 0; i < nInt; ++i) {
        int gi = i + 1;
        bI[i] = b[gi];
        fI[i] = f[gi];
        if (i > 0)       aI[i] = a[gi];
        if (i < nInt-1)  cI[i] = c[gi];
    }

    // Solve
    thomas(aI, bI, cI, fI);

    // Reconstruct full solution
    std::vector<double> T(N);
    T[0]   = p.T_left;
    T[N-1] = p.T_right;
    for (int i = 0; i < nInt; ++i) T[i+1] = fI[i];

    // Store node positions and temperatures
    res.x_nodes.resize(N);
    res.T_fem.resize(N);
    for (int i = 0; i < N; ++i) {
        res.x_nodes[i] = i * h;
        res.T_fem[i]   = T[i];
    }

    // Compute midpoint values via FE interpolation (linear shape functions)
    res.x_midpoints.resize(n);
    res.T_fem_mid.resize(n);
    res.T_exact_mid.resize(n);
    res.error_mid.resize(n);
    double maxErr = 0.0;

    for (int e = 0; e < n; ++e) {
        double x_mid = (e + 0.5) * h;
        // Linear interpolation: T_mid = 0.5*(T[e] + T[e+1])
        double T_fem_m = 0.5 * (T[e] + T[e+1]);
        double T_ex_m  = femAnalytical(x_mid, p);
        double err     = std::abs(T_fem_m - T_ex_m);
        res.x_midpoints[e] = x_mid;
        res.T_fem_mid[e]   = T_fem_m;
        res.T_exact_mid[e] = T_ex_m;
        res.error_mid[e]   = err;
        maxErr = std::max(maxErr, err);
    }
    res.max_abs_error = maxErr;
    res.success = true;
    return res;
}

// ─── convergence study ───────────────────────────────────────────────────────

std::vector<FemResult> femConvergence(const FemParams& base,
                                       const std::vector<int>& elem_counts) {
    std::vector<FemResult> results;
    results.reserve(elem_counts.size());
    for (int n : elem_counts) {
        FemParams p = base;
        p.n_elements = n;
        results.push_back(femSolve(p));
    }
    return results;
}

// ─── CSV export ──────────────────────────────────────────────────────────────

bool femExportCsv(const std::vector<FemResult>& results,
                  const std::string& filepath) {
    std::ofstream ofs(filepath);
    if (!ofs.is_open()) return false;

    // Header
    ofs << "n_elements,x_m,T_fem_C,T_exact_C,error_C\n";
    for (const auto& r : results) {
        if (!r.success) continue;
        // Node values
        for (std::size_t i = 0; i < r.x_nodes.size(); ++i) {
            ofs << r.params.n_elements << ","
                << r.x_nodes[i] << ","
                << r.T_fem[i] << ","
                << femAnalytical(r.x_nodes[i], r.params) << ","
                << std::abs(r.T_fem[i] - femAnalytical(r.x_nodes[i], r.params)) << "\n";
        }
    }
    return true;
}

} // namespace thermatrix
