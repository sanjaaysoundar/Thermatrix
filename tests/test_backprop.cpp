// test_backprop.cpp – Unit tests for the backpropagation solver
// THERMATRIX C++17

#include "../src/core/BackpropSolver.h"
#include <cassert>
#include <cmath>
#include <iostream>
#include <vector>

using namespace thermatrix;

static int passed = 0, failed = 0;

#define ASSERT_TRUE(cond, msg) \
    do { if (cond) { ++passed; std::cout << "[PASS] " << msg << "\n"; } \
         else { ++failed; std::cout << "[FAIL] " << msg << "\n"; } } while(0)

#define ASSERT_NEAR(a, b, tol, msg) \
    ASSERT_TRUE(std::abs((a)-(b)) <= (tol), msg << " (got " << (a) << ", expected " << (b) << ")")

// ─── Forward pass validation ─────────────────────────────────────────────────

void test_forward() {
    BackpropParams p; // w=0.5, b=0, v=1, c=0, x=1, y=2
    auto f = backpropForward(p);
    double z_exp    = 0.5*1.0 + 0.0;               // z = 0.5
    double h_exp    = std::tanh(z_exp);             // h = tanh(0.5) ≈ 0.46212
    double yhat_exp = 1.0 * h_exp + 0.0;           // y^ = h
    double J_exp    = 0.5*(yhat_exp - 2.0)*(yhat_exp - 2.0);
    ASSERT_NEAR(f.z,     z_exp,    1e-12, "Forward: z = w*x+b = 0.5");
    ASSERT_NEAR(f.h,     h_exp,    1e-12, "Forward: h = tanh(z)");
    ASSERT_NEAR(f.y_hat, yhat_exp, 1e-12, "Forward: y^ = v*h+c");
    ASSERT_NEAR(f.J,     J_exp,    1e-12, "Forward: J = 0.5*(y^-y)²");
    ASSERT_TRUE(f.J > 0, "Initial loss J > 0");
}

// ─── Gradient finite-difference verification ─────────────────────────────────

void test_fd_check() {
    BackpropParams p;
    auto r = backpropRun(p);
    ASSERT_TRUE(r.fd_all_pass, "All FD gradient checks pass (abs_error <= 1e-5)");
    for (auto& chk : r.fd_checks) {
        ASSERT_NEAR(chk.analytical, chk.fd, 1e-5,
                    "FD check: " << chk.name);
    }
}

// ─── Gradient values (manual computation) ────────────────────────────────────

void test_gradient_values() {
    BackpropParams p;
    auto f = backpropForward(p);
    auto g = backpropGradients(p, f);

    double delta = f.y_hat - p.y;               // y^ - y
    double dtanh = 1.0 - f.h * f.h;            // 1 - tanh²(z)

    double dJdc_exp = delta;
    double dJdv_exp = delta * f.h;
    double dJdb_exp = delta * p.v * dtanh;
    double dJdw_exp = delta * p.v * dtanh * p.x;

    ASSERT_NEAR(g.dJ_dc, dJdc_exp, 1e-12, "dJ/dc analytical");
    ASSERT_NEAR(g.dJ_dv, dJdv_exp, 1e-12, "dJ/dv analytical");
    ASSERT_NEAR(g.dJ_db, dJdb_exp, 1e-12, "dJ/db analytical");
    ASSERT_NEAR(g.dJ_dw, dJdw_exp, 1e-12, "dJ/dw analytical");
}

// ─── Gradient-descent update (simultaneous) ──────────────────────────────────

void test_gradient_update() {
    BackpropParams p;
    auto r = backpropRun(p);
    auto& g = r.gradients;
    auto& u = r.updated;

    // Updated params should equal original - lr * gradient
    ASSERT_NEAR(u.w, p.w - p.lr * g.dJ_dw, 1e-12, "Updated w = w - lr*dJ/dw");
    ASSERT_NEAR(u.b, p.b - p.lr * g.dJ_db, 1e-12, "Updated b = b - lr*dJ/db");
    ASSERT_NEAR(u.v, p.v - p.lr * g.dJ_dv, 1e-12, "Updated v = v - lr*dJ/dv");
    ASSERT_NEAR(u.c, p.c - p.lr * g.dJ_dc, 1e-12, "Updated c = c - lr*dJ/dc");
}

// ─── Loss decreases over 100 steps ───────────────────────────────────────────

void test_loss_decreasing() {
    BackpropParams p;
    auto r = backpropRun(p);
    ASSERT_TRUE(r.loss_history.size() == 101, "101 loss values (step 0..100)");
    // Overall loss should decrease substantially
    double J0 = r.loss_history.front();
    double J100 = r.loss_history.back();
    ASSERT_TRUE(J100 < J0, "Loss after 100 steps < initial loss");
    ASSERT_TRUE(J100 < J0 * 0.1, "Loss reduced by at least 90%");
}

// ─── Loss history is non-negative ────────────────────────────────────────────

void test_loss_nonnegative() {
    BackpropParams p;
    auto r = backpropRun(p);
    for (int i = 0; i < (int)r.loss_history.size(); ++i) {
        ASSERT_TRUE(r.loss_history[i] >= 0.0, "Loss[" << i << "] >= 0");
    }
}

// ─── Updated forward pass loss < initial loss ────────────────────────────────

void test_one_step_improvement() {
    BackpropParams p;
    auto r = backpropRun(p);
    ASSERT_TRUE(r.updated_forward.J < r.forward.J,
                "One gradient step reduces loss");
}

int main() {
    std::cout << "=== Backpropagation Solver Tests ===\n";
    test_forward();
    test_fd_check();
    test_gradient_values();
    test_gradient_update();
    test_loss_decreasing();
    test_loss_nonnegative();
    test_one_step_improvement();
    std::cout << "\n" << passed << " passed, " << failed << " failed.\n";
    return (failed > 0) ? 1 : 0;
}
