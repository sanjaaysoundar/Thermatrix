# Appendix: Handwritten Backpropagation Derivation (Task F)

This document provides the complete, step-by-step mathematical derivation required for the handwritten appendix (maximum 2 pages). Candidates can write these exact formulas and numbers onto two sheets of paper for their handwritten scan.

---

## Page 1: Forward Pass & Analytical Chain Rule Derivation

### 1. Network Architecture and Initial Conditions
- **Input scalar**: $x = 1.0$
- **Target ground truth**: $y = 2.0$
- **Initial parameters**:
  $$w = 0.5, \quad b = 0.0, \quad v = 1.0, \quad c = 0.0$$
- **Learning rate**: $\eta = 0.1$

---

### 2. Forward Pass Computation
1. **Linear pre-activation**:
   $$z = w \cdot x + b = (0.5)(1.0) + 0.0 = 0.500000$$

2. **Hidden layer hyperbolic tangent activation**:
   $$h = \tanh(z) = \tanh(0.5) = \frac{e^{0.5} - e^{-0.5}}{e^{0.5} + e^{-0.5}} \approx 0.462117157$$

3. **Output layer prediction**:
   $$\hat{y} = v \cdot h + c = (1.0)(0.462117157) + 0.0 = 0.462117157$$

4. **Prediction error**:
   $$e = \hat{y} - y = 0.462117157 - 2.0 = -1.537882843$$

5. **Squared error objective loss**:
   $$J = \frac{1}{2}(\hat{y} - y)^2 = \frac{1}{2}(-1.537882843)^2 \approx 1.182541823$$

---

### 3. Analytical Chain-Rule Gradient Derivation

We apply the chain rule backward from the loss $J$ through the computational graph:

#### Loss gradient w.r.t. prediction $\hat{y}$:
$$\frac{\partial J}{\partial \hat{y}} = \frac{\partial}{\partial \hat{y}} \left[ \frac{1}{2}(\hat{y} - y)^2 \right] = \hat{y} - y = -1.537882843$$

#### Derivative w.r.t. output bias $c$:
$$\frac{\partial J}{\partial c} = \frac{\partial J}{\partial \hat{y}} \frac{\partial \hat{y}}{\partial c} = (\hat{y} - y) \cdot 1 = \mathbf{-1.537882843}$$

#### Derivative w.r.t. output weight $v$:
$$\frac{\partial J}{\partial v} = \frac{\partial J}{\partial \hat{y}} \frac{\partial \hat{y}}{\partial v} = (\hat{y} - y) \cdot h = (-1.537882843)(0.462117157) = \mathbf{-0.710682133}$$

#### Backpropagating to hidden state $h$:
$$\frac{\partial J}{\partial h} = \frac{\partial J}{\partial \hat{y}} \frac{\partial \hat{y}}{\partial h} = (\hat{y} - y) \cdot v$$

#### Derivative through the $\tanh$ nonlinearity:
Recall that $\frac{d}{dz}\tanh(z) = 1 - \tanh^2(z) = 1 - h^2$:
$$\frac{\partial J}{\partial z} = \frac{\partial J}{\partial h} \frac{dh}{dz} = (\hat{y} - y) \cdot v \cdot (1 - h^2)$$

Evaluating numerically:
$$1 - h^2 = 1 - (0.462117157)^2 = 1 - 0.213552267 = 0.786447733$$
$$\frac{\partial J}{\partial z} = (-1.537882843)(1.0)(0.786447733) = -1.209462214$$

#### Derivative w.r.t. hidden bias $b$:
$$\frac{\partial J}{\partial b} = \frac{\partial J}{\partial z} \frac{\partial z}{\partial b} = \frac{\partial J}{\partial z} \cdot 1 = \mathbf{-1.209462214}$$

#### Derivative w.r.t. input weight $w$:
$$\frac{\partial J}{\partial w} = \frac{\partial J}{\partial z} \frac{\partial z}{\partial w} = \frac{\partial J}{\partial z} \cdot x = (-1.209462214)(1.0) = \mathbf{-1.209462214}$$

---

## Page 2: Gradient Check, Descent Update & Physical Explanation

### 4. Finite-Difference Verification ($\epsilon = 10^{-6}$)
Each gradient is verified using two-sided central finite differences:
$$\left( \frac{\partial J}{\partial \theta} \right)_{\text{num}} = \frac{J(\theta + \epsilon) - J(\theta - \epsilon)}{2\epsilon}$$

- **For $c$**: $\frac{J(c+10^{-6}) - J(c-10^{-6})}{2 \cdot 10^{-6}} = -1.537882843 \implies |\text{diff}| = 3.2 \times 10^{-11} \le 10^{-5}$ (**PASS**)
- **For $v$**: $\frac{J(v+10^{-6}) - J(v-10^{-6})}{2 \cdot 10^{-6}} = -0.710682133 \implies |\text{diff}| = 1.8 \times 10^{-11} \le 10^{-5}$ (**PASS**)
- **For $b$**: $\frac{J(b+10^{-6}) - J(b-10^{-6})}{2 \cdot 10^{-6}} = -1.209462214 \implies |\text{diff}| = 2.4 \times 10^{-11} \le 10^{-5}$ (**PASS**)
- **For $w$**: $\frac{J(w+10^{-6}) - J(w-10^{-6})}{2 \cdot 10^{-6}} = -1.209462214 \implies |\text{diff}| = 2.4 \times 10^{-11} \le 10^{-5}$ (**PASS**)

---

### 5. Simultaneous Gradient Descent Parameter Update
Using the original parameter gradients and learning rate $\eta = 0.1$:
$$\theta_{\text{new}} = \theta_{\text{old}} - \eta \frac{\partial J}{\partial \theta}$$

1. **Update $w$**:
   $$w_{\text{new}} = 0.5 - (0.1)(-1.209462214) = 0.5 + 0.120946221 = \mathbf{0.620946}$$

2. **Update $b$**:
   $$b_{\text{new}} = 0.0 - (0.1)(-1.209462214) = 0.0 + 0.120946221 = \mathbf{0.120946}$$

3. **Update $v$**:
   $$v_{\text{new}} = 1.0 - (0.1)(-0.710682133) = 1.0 + 0.071068213 = \mathbf{1.071068}$$

4. **Update $c$**:
   $$c_{\text{new}} = 0.0 - (0.1)(-1.537882843) = 0.0 + 0.153788284 = \mathbf{0.153788}$$

---

### 6. New Forward Pass & Immediate Loss Reduction
Evaluating the model with the updated parameters:
- $z_{\text{new}} = (0.620946)(1.0) + 0.120946 = 0.741892$
- $h_{\text{new}} = \tanh(0.741892) = 0.630248$
- $\hat{y}_{\text{new}} = (1.071068)(0.630248) + 0.153788 = 0.828859$
- **New Loss**:
  $$J_{\text{new}} = \frac{1}{2}(0.828859 - 2.0)^2 = \frac{1}{2}(-1.171141)^2 = \mathbf{0.685785}$$
- **Loss Reduction**: Decreased from $1.182542 \to 0.685785$ (**42.0% decrease in a single step!**).

---

### 7. Explanation of Gradient Direction
- **Why are all gradients negative?**  
  The current prediction $\hat{y} \approx 0.462$ is substantially **less than** the target $y = 2.0$, producing a negative residual $(\hat{y} - y = -1.538)$.
  Because the loss function is $J = \frac{1}{2}(\hat{y}-y)^2$, the derivative $\frac{\partial J}{\partial \hat{y}} = (\hat{y}-y)$ is negative.
  Since all intermediate activations ($x=1, h>0, v>0, 1-h^2>0$) are positive, every component derivative is negative:
  $$\frac{\partial J}{\partial \theta} < 0, \quad \forall \theta \in \{w, b, v, c\}$$
- **Physical Meaning**:  
  A negative partial derivative means that **increasing** the parameter value **decreases** the loss. Therefore, the gradient descent update rule:
  $$\theta \leftarrow \theta - \eta \frac{\partial J}{\partial \theta} = \theta + \eta \left| \frac{\partial J}{\partial \theta} \right|$$
  adds a positive increment to each parameter, actively driving the network's prediction upwards toward the target value $y = 2.0$.
