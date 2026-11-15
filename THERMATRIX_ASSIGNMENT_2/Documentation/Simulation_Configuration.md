# THERMATRIX – Simulation Configuration Document
## 3D Steady-State Thermal Analysis of Extruded Aluminium Heat Sink

### 1. Physical Problem Specification
The computational domain represents an industrial aluminium extruded heat sink designed for high-density power semiconductor cooling. The analysis solves the steady-state thermal diffusion equation with convection and localized asymmetric heat dissipation:

$$-\nabla \cdot (k \nabla T) = Q \quad \text{in } \Omega$$

For constant thermal conductivity $k = 167 \text{ W/(m}\cdot\text{K)}$ (Aluminium 6061-T6) and zero internal volumetric generation in the solid body ($Q = 0$), the governing PDE reduces to Laplace's equation:

$$\nabla^2 T = 0 \quad \text{in } \Omega$$

---

### 2. Physical & Geometric Dimensions
The heat sink features a solid rectangular base plate surmounted by five longitudinal cooling fins:

| Geometric Parameter | Value [SI] | Value [mm] | Description |
|:---|:---:|:---:|:---|
| **Base Length ($L_x$)** | $0.080 \text{ m}$ | $80.0 \text{ mm}$ | Dimension along longitudinal fin extrusion |
| **Base Width ($L_y$)** | $0.060 \text{ m}$ | $60.0 \text{ mm}$ | Transverse dimension across fins |
| **Base Height ($H_{base}$)** | $0.010 \text{ m}$ | $10.0 \text{ mm}$ | Base plate thickness |
| **Number of Fins ($N_{fins}$)** | $5$ | $5$ | Straight vertical cooling fins |
| **Fin Length** | $0.080 \text{ m}$ | $80.0 \text{ mm}$ | Extruded length matching base |
| **Fin Height ($H_{fin}$)** | $0.030 \text{ m}$ | $30.0 \text{ mm}$ | Height above base (Total height = $40.0 \text{ mm}$) |
| **Fin Thickness ($t_{fin}$)** | $0.002 \text{ m}$ | $2.0 \text{ mm}$ | Slender vertical fin cross-section |
| **Fin Center Pitch** | $0.012 \text{ m}$ | $12.0 \text{ mm}$ | Center-to-center fin spacing |
| **Fin Gap Width** | $0.010 \text{ m}$ | $10.0 \text{ mm}$ | Clearance between adjacent fin surfaces |
| **Total Solid Volume** | $7.200 \times 10^{-5} \text{ m}^3$ | $72.0 \text{ cm}^3$ | Solid conduction domain volume |
| **Exposed Convective Area** | $0.0322 \text{ m}^2$ | $322.0 \text{ cm}^2$ | Area active in convective heat exchange |

---

### 3. Heat Source Configuration
- **Total Thermal Power Input ($Q_{in}$)**: $10.0 \text{ W}$
- **Source Geometry**: Flat square planar footprint on the base underside ($z = 0$)
- **Patch Dimensions**: $10.0 \text{ mm} \times 10.0 \text{ mm}$ ($0.010 \text{ m} \times 0.010 \text{ m}$)
- **Patch Surface Area ($A_{patch}$)**: $1.000 \times 10^{-4} \text{ m}^2$ ($100.0 \text{ mm}^2$)
- **Patch Center Location**:
  - Base center: $(x_c, y_c) = (40.0 \text{ mm}, 30.0 \text{ mm})$
  - Spatial Offset: $\Delta x = -10.0 \text{ mm}, \Delta y = +5.0 \text{ mm}$
  - Absolute Source Center: $(x_s, y_s, z_s) = (30.0 \text{ mm}, 35.0 \text{ mm}, 0.0 \text{ mm})$
  - Coordinates bounding box: $x \in [25, 35] \text{ mm}, y \in [30, 40] \text{ mm}, z = 0$
- **Prescribed Heat Flux**:
  $$q'' = \frac{Q_{in}}{A_{patch}} = \frac{10.0 \text{ W}}{1.0 \times 10^{-4} \text{ m}^2} = 100,000 \text{ W/m}^2 \quad (10.0 \text{ W/cm}^2)$$
- **Boundary Condition (Inward Flux)**:
  $$-k \frac{\partial T}{\partial n} = -q'' \implies k \frac{\partial T}{\partial z} = q'' \quad \text{on } \Gamma_{source}$$

---

### 4. Boundary Conditions
The external boundary $\partial\Omega$ is partitioned into three mutually exclusive regions:

1. **Source Patch ($\Gamma_{source}$)**:
   $$-k \nabla T \cdot \mathbf{n} = -q'' = -100,000 \text{ W/m}^2$$

2. **Underside Adiabatic Region ($\Gamma_{adiabatic}$)**:
   Base underside outside the source patch ($z = 0$, $(x,y) \notin \text{patch}$):
   $$-k \nabla T \cdot \mathbf{n} = 0 \quad \text{(Perfect thermal insulation)}$$

3. **Exposed Convective Boundary ($\Gamma_{conv}$)**:
   Includes base perimeter lateral faces, top surface of base between fins, and all fin lateral/tip/end faces:
   $$-k \nabla T \cdot \mathbf{n} = h (T - T_\infty)$$
   - **Ambient Temperature ($T_\infty$)**: $25.0^\circ\text{C}$ ($298.15\text{ K}$)
   - **Case A (Natural / Low Convection)**: $h = 10.0 \text{ W/(m}^2\cdot\text{K)}$
   - **Case B (Moderate Forced Convection)**: $h = 25.0 \text{ W/(m}^2\cdot\text{K)}$

---

### 5. Mesh & Grid Discretization Hierarchy
Three independent discretization resolutions are evaluated across all three solvers:

| Resolution Level | FEM Grid / Bricks | FEM Nodes (DOF) | FEM Tetrahedra | Structured Voxels | Solid Voxel DOFs |
|:---|:---:|:---:|:---:|:---:|:---:|
| **Coarse** | $16 \times 15 \times 3$ (base) + fins | $2,108$ | $7,200$ | $40 \times 30 \times 20$ | $9,600$ |
| **Medium** | $32 \times 30 \times 6$ (base) + fins | $13,101$ | $57,600$ | $60 \times 45 \times 30$ | $72,000$ |
| **Fine** | $48 \times 45 \times 9$ (base) + fins | $40,180$ | $194,400$ | $80 \times 60 \times 40$ | $248,400$ |

---

### 6. Linear Algebraic Solver Specifications
- **Algorithm**: Preconditioned Conjugate Gradient (PCG)
- **Preconditioner**: Incomplete Cholesky Factorization with Zero Fill-in (IC(0))
- **Convergence Tolerance**: Relative residual $\| \mathbf{r} \|_2 / \| \mathbf{b} \|_2 \le 1.0 \times 10^{-10}$
- **Maximum Allowed Iterations**: $10,000$
