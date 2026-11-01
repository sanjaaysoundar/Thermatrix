// BackpropSolver.cpp – Backpropagation calculation (no automatic differentiation)
// THERMATRIX C++17 backend
//
// Derivation (chain rule, analytical):
//   z    = w*x + b
//   h    = tanh(z)
//   y^   = v*h + c
//   J    = 0.5*(y^-y)²
//
//   dJ/dy^ = y^ - y
//   dy^/dv = h        → dJ/dv = (y^-y)*h
//   dy^/dc = 1        → dJ/dc = y^-y
//   dy^/dh = v        → dJ/dh = (y^-y)*v
//   dh/dz  = 1-tanh²(z) = 1-h²
//   dz/dw  = x        → dJ/dw = (y^-y)*v*(1-h²)*x
//   dz/db  = 1        → dJ/db = (y^-y)*v*(1-h²)

#include "BackpropSolver.h"

#include <cmath>
#include <cassert>

namespace thermatrix {

// ─── forward pass ────────────────────────────────────────────────────────────

BackpropForward backpropForward(const BackpropParams& p) {
    BackpropForward f;
    f.z     = p.w * p.x + p.b;
    f.h     = std::tanh(f.z);
    f.y_hat = p.v * f.h + p.c;
    f.J     = 0.5 * (f.y_hat - p.y) * (f.y_hat - p.y);
    return f;
}

// ─── analytical gradients ────────────────────────────────────────────────────

BackpropGradients backpropGradients(const BackpropParams& p, const BackpropForward& f) {
    BackpropGradients g;
    double delta = f.y_hat - p.y;           // dJ/dy^
    double dtanh = 1.0 - f.h * f.h;        // dh/dz = 1 - tanh²(z)
    g.dJ_dv = delta * f.h;
    g.dJ_dc = delta;
    g.dJ_dw = delta * p.v * dtanh * p.x;
    g.dJ_db = delta * p.v * dtanh;
    return g;
}

// ─── finite-difference gradients ─────────────────────────────────────────────

static double lossAt(BackpropParams p) {
    return backpropForward(p).J;
}

BackpropGradients backpropFD(const BackpropParams& p) {
    constexpr double eps = 1e-6;
    BackpropGradients g;

    // dJ/dw
    { BackpropParams pp=p; pp.w+=eps; BackpropParams pm=p; pm.w-=eps;
      g.dJ_dw = (lossAt(pp)-lossAt(pm))/(2.0*eps); }
    // dJ/db
    { BackpropParams pp=p; pp.b+=eps; BackpropParams pm=p; pm.b-=eps;
      g.dJ_db = (lossAt(pp)-lossAt(pm))/(2.0*eps); }
    // dJ/dv
    { BackpropParams pp=p; pp.v+=eps; BackpropParams pm=p; pm.v-=eps;
      g.dJ_dv = (lossAt(pp)-lossAt(pm))/(2.0*eps); }
    // dJ/dc
    { BackpropParams pp=p; pp.c+=eps; BackpropParams pm=p; pm.c-=eps;
      g.dJ_dc = (lossAt(pp)-lossAt(pm))/(2.0*eps); }
    return g;
}

// ─── full run ────────────────────────────────────────────────────────────────

BackpropResult backpropRun(const BackpropParams& init) {
    BackpropResult res;
    res.initial = init;

    // Initial forward pass
    res.forward = backpropForward(init);

    // Analytical gradients
    res.gradients = backpropGradients(init, res.forward);

    // Finite-difference gradients
    res.fd_gradients = backpropFD(init);

    // FD checks
    auto check = [&](const char* name, double anal, double fd) {
        FDCheck c;
        c.name       = name;
        c.analytical = anal;
        c.fd         = fd;
        c.abs_error  = std::abs(anal - fd);
        c.passed     = (c.abs_error <= 1e-5);
        res.fd_checks.push_back(c);
    };
    check("dJ/dw", res.gradients.dJ_dw, res.fd_gradients.dJ_dw);
    check("dJ/db", res.gradients.dJ_db, res.fd_gradients.dJ_db);
    check("dJ/dv", res.gradients.dJ_dv, res.fd_gradients.dJ_dv);
    check("dJ/dc", res.gradients.dJ_dc, res.fd_gradients.dJ_dc);
    res.fd_all_pass = true;
    for (auto& c : res.fd_checks) if (!c.passed) res.fd_all_pass = false;

    // Simultaneous gradient-descent update (using ORIGINAL parameters)
    res.updated.x  = init.x;
    res.updated.y  = init.y;
    res.updated.lr = init.lr;
    res.updated.w  = init.w - init.lr * res.gradients.dJ_dw;
    res.updated.b  = init.b - init.lr * res.gradients.dJ_db;
    res.updated.v  = init.v - init.lr * res.gradients.dJ_dv;
    res.updated.c  = init.c - init.lr * res.gradients.dJ_dc;

    res.updated_forward = backpropForward(res.updated);

    // 100-step training starting from initial params
    res.loss_history.clear();
    res.loss_history.reserve(101);
    BackpropParams cur = init;
    for (int step = 0; step <= 100; ++step) {
        auto fwd = backpropForward(cur);
        res.loss_history.push_back(fwd.J);
        if (step < 100) {
            auto grad = backpropGradients(cur, fwd);
            cur.w -= cur.lr * grad.dJ_dw;
            cur.b -= cur.lr * grad.dJ_db;
            cur.v -= cur.lr * grad.dJ_dv;
            cur.c -= cur.lr * grad.dJ_dc;
        }
    }

    return res;
}

} // namespace thermatrix
