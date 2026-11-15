#pragma once
// ThermalProblem.h  –  THERMATRIX Phase 1
// Common physics configuration shared by all 3D thermal solvers.
// namespace thermatrix::t3d
//
// Geometry: heat-sink block solved by FEM, FDM, FVM.
// All quantities in SI (m, W, K).  Temperatures reported in °C
// (using offset convention: T[°C] = T[K] - 273.15, but since we solve
// ΔT only the offset cancels; Tinf is given in °C for user convenience
// and treated as the ambient reference).

#include <array>
#include <string>

namespace thermatrix {
namespace t3d {

// ---------------------------------------------------------------------------
// 3-D axis-aligned bounding box (metres)
// ---------------------------------------------------------------------------
struct Box3 {
    double xmin, xmax;
    double ymin, ymax;
    double zmin, zmax;

    bool contains(double x, double y, double z) const noexcept {
        return x >= xmin && x <= xmax &&
               y >= ymin && y <= ymax &&
               z >= zmin && z <= zmax;
    }

    double volume() const noexcept {
        return (xmax - xmin) * (ymax - ymin) * (zmax - zmin);
    }

    double area_xy() const noexcept {   ///< area of the z-normal face
        return (xmax - xmin) * (ymax - ymin);
    }
};

// ---------------------------------------------------------------------------
// Which boundary face of the domain box a surface belongs to
// ---------------------------------------------------------------------------
enum class FaceDir : int {
    Xminus = 0,  ///< x = xmin
    Xplus  = 1,  ///< x = xmax
    Yminus = 2,  ///< y = ymin
    Yplus  = 3,  ///< y = ymax
    Zminus = 4,  ///< z = zmin  (underside / bottom of heat-sink base)
    Zplus  = 5,  ///< z = zmax  (top / fin tips)
};

// ---------------------------------------------------------------------------
// Boundary condition type for a face region
// ---------------------------------------------------------------------------
enum class BcType : int {
    Adiabatic = 0,  ///< zero heat flux (Neumann homogeneous)
    Robin     = 1,  ///< -k dT/dn = h*(T - Tinf)   (convection)
    HeatFlux  = 2,  ///< -k dT/dn = -q''  (prescribed inward flux, q'' > 0 means heat in)
};

// ---------------------------------------------------------------------------
// Convection case tag
// ---------------------------------------------------------------------------
enum class ConvCase : int {
    A = 0,  ///< h = 10 W/m²K
    B = 1,  ///< h = 25 W/m²K
};

// ---------------------------------------------------------------------------
// ThermalProblem  –  single source-of-truth for all three solvers
// ---------------------------------------------------------------------------
struct ThermalProblem {
    // --- Material -----------------------------------------------------------
    double k { 167.0 };        ///< Thermal conductivity  [W/(m·K)]  Al 6061

    // --- Geometry -----------------------------------------------------------
    // Heat-sink overall bounding box
    // Base:  80 mm × 60 mm × 10 mm
    // Fins:  5 fins, each 80 mm × 2 mm × 30 mm, spaced 10 mm centre-to-centre
    // We store the full domain box; mesh generators fill it with material.
    // For structured solvers (FDM/FVM) this is the voxel domain.
    // For FEM the same box + fin geometry is used as the STL / analytic domain.

    // Domain box (heat-sink tight-fitting bounding box)
    Box3 domain {
        0.0, 0.080,   // x: 80 mm
        0.0, 0.060,   // y: 60 mm
        0.0, 0.040    // z: 40 mm  (10 mm base + 30 mm fins)
    };

    // --- Heat source (Neumann flux patch on underside, z = 0) ---------------
    double Q_total { 10.0 };   ///< Total power input [W]

    // Source patch on z = zmin (underside).
    // Centre offset from base centre: Δx = -10 mm, Δy = +5 mm.
    // Base centre = (40 mm, 30 mm).  Patch centre = (30 mm, 35 mm).
    // Patch size 10 mm × 10 mm.
    Box3 source_patch {
        0.025, 0.035,   // x: 30±5 mm
        0.030, 0.040,   // y: 35±5 mm
        -1e-9, +1e-9    // z: z=0 plane (epsilon for containment test)
    };

    // Derived: heat flux magnitude [W/m²] – computed from Q_total / patch_area
    double source_flux() const noexcept {
        double A = (source_patch.xmax - source_patch.xmin) *
                   (source_patch.ymax - source_patch.ymin);
        return Q_total / A;   // positive = into domain
    }

    // --- Convection ---------------------------------------------------------
    double h_A    { 10.0 };    ///< Convection coefficient, Case A  [W/(m²·K)]
    double h_B    { 25.0 };    ///< Convection coefficient, Case B  [W/(m²·K)]
    double T_inf  { 25.0 };    ///< Ambient temperature  [°C]

    /// Return h for the requested case
    double h(ConvCase c) const noexcept {
        return (c == ConvCase::A) ? h_A : h_B;
    }

    // --- Boundary condition query -------------------------------------------
    // Rules (applied in order):
    //   1. Underside (Zminus) inside source_patch  → HeatFlux
    //   2. Underside (Zminus) outside source_patch → Adiabatic
    //   3. All other exposed domain faces           → Robin (convection)
    //
    // For a point (px, py) on face dir:
    BcType face_bc(FaceDir dir, double px, double py) const noexcept {
        if (dir == FaceDir::Zminus) {
            // Check if (px, py) lies within source patch (z dimension ignored)
            if (px >= source_patch.xmin && px <= source_patch.xmax &&
                py >= source_patch.ymin && py <= source_patch.ymax) {
                return BcType::HeatFlux;
            }
            return BcType::Adiabatic;
        }
        // All other exterior faces: convection
        return BcType::Robin;
    }

    // Convenience: does a face direction belong to a convective boundary?
    // (Always true except Zminus which is mixed.)
    bool is_convective_face(FaceDir dir) const noexcept {
        return dir != FaceDir::Zminus;
    }

    // --- Fin geometry (for analytic / STL construction) ----------------------
    // 5 fins along y axis, each fin:
    //   width (x) = full domain: [0, 0.080]
    //   thickness (y) = 2 mm, centred at y_centres below
    //   height (z)    = [0.010, 0.040]  (atop 10 mm base)
    static constexpr int    N_FINS      { 5 };
    static constexpr double BASE_HEIGHT { 0.010 };   ///< Base slab z [m]
    static constexpr double FIN_HEIGHT  { 0.030 };   ///< Fin above base [m]
    static constexpr double FIN_THICK   { 0.002 };   ///< Fin thickness [m]
    // y-centres of fins (evenly spaced over 60 mm: 7.5,17.5,27.5,37.5,47.5 mm → 10mm pitch ×5)
    // Pitch = 60mm / 5 = 12mm centre-to-centre, fins at 6,18,30,42,54 mm
    static constexpr double FIN_PITCH   { 0.012 };
    static constexpr double FIN_Y0      { 0.006 };   ///< Centre of first fin

    /// True if (x,y,z) is inside the solid heat-sink material
    bool in_solid(double x, double y, double z) const noexcept {
        // Must be inside domain box
        if (x < domain.xmin || x > domain.xmax ||
            y < domain.ymin || y > domain.ymax ||
            z < domain.zmin || z > domain.zmax) return false;

        // Base slab: z ∈ [0, BASE_HEIGHT]
        if (z <= BASE_HEIGHT) return true;

        // Fin region: z ∈ (BASE_HEIGHT, domain.zmax]
        for (int i = 0; i < N_FINS; ++i) {
            double yc = FIN_Y0 + i * FIN_PITCH;
            if (y >= yc - FIN_THICK * 0.5 &&
                y <= yc + FIN_THICK * 0.5) return true;
        }
        return false;
    }

    // --- Mesh density (used as hint by all solvers) -------------------------
    // FDM/FVM: number of voxels per axis for domain bounding box
    // FEM:     target element size hint [m]
    int    nx_fdm { 40 };   ///< FDM/FVM voxels in x (domain)
    int    ny_fdm { 30 };   ///< FDM/FVM voxels in y
    int    nz_fdm { 20 };   ///< FDM/FVM voxels in z
    double h_fem  { 0.004}; ///< FEM target edge length [m]
};

} // namespace t3d
} // namespace thermatrix
