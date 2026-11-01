#pragma once
// BackpropSolver.h – Backpropagation calculator (no automatic differentiation)
// THERMATRIX C++17 backend
//
// Network:  h  = tanh(w*x + b)
//           y^ = v*h + c
//           J  = 0.5*(y^ - y)²
//
// Initial:  w=0.5, b=0, v=1, c=0
// Input:    x=1, y=2
// LR:       0.1

#include <vector>
#include <string>
#include <cstddef>

namespace thermatrix {

struct BackpropParams {
    double x{1.0};  ///< Input
    double y{2.0};  ///< Target
    double w{0.5};  ///< Weight (input layer)
    double b{0.0};  ///< Bias (input layer)
    double v{1.0};  ///< Weight (output layer)
    double c{0.0};  ///< Bias (output layer)
    double lr{0.1}; ///< Learning rate
};

struct BackpropForward {
    double z;    ///< z = w*x + b
    double h;    ///< h = tanh(z)
    double y_hat; ///< y^ = v*h + c
    double J;    ///< J = 0.5*(y^ - y)²
};

struct BackpropGradients {
    double dJ_dw;
    double dJ_db;
    double dJ_dv;
    double dJ_dc;
};

/// Finite-difference check result for one parameter.
struct FDCheck {
    std::string  name;
    double       analytical;
    double       fd;
    double       abs_error;
    bool         passed; ///< abs_error <= 1e-5
};

struct BackpropResult {
    BackpropParams      initial;
    BackpropForward     forward;
    BackpropGradients   gradients;
    BackpropGradients   fd_gradients;    ///< Central finite differences
    std::vector<FDCheck> fd_checks;
    BackpropParams      updated;          ///< After one gradient-descent step
    BackpropForward     updated_forward;  ///< Forward pass on updated params
    std::vector<double> loss_history;     ///< J over 100 steps (starting from *initial* params)
    bool                fd_all_pass{false};
};

/// Compute one forward pass.
BackpropForward backpropForward(const BackpropParams& p);

/// Compute gradients analytically via chain rule.
BackpropGradients backpropGradients(const BackpropParams& p, const BackpropForward& fwd);

/// Compute gradients via central finite differences (epsilon=1e-6).
BackpropGradients backpropFD(const BackpropParams& p);

/// Run the full backpropagation calculation, FD check, update, and 100-step training.
BackpropResult backpropRun(const BackpropParams& init);

} // namespace thermatrix
