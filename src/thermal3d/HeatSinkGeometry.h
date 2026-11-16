#pragma once
// HeatSinkGeometry.h  –  THERMATRIX Heat Sink CAD & Discretization Specification
// namespace thermatrix::t3d

#include "ThermalProblem.h"
#include <string>
#include <vector>

namespace thermatrix {
namespace t3d {

struct HeatSinkParams {
    double base_length   { 0.060 }; // x dimension: 60 mm
    double base_width    { 0.060 }; // y dimension: 60 mm
    double base_height   { 0.004 }; // z dimension: 4 mm

    int    n_fins        { 9 };
    double fin_length    { 0.060 }; // x dimension: 60 mm
    double fin_height    { 0.025 }; // z dimension: 25 mm (total height = 29 mm)
    double fin_thick_root{ 0.0020 }; // 2.0 mm root thickness
    double fin_thick_tip { 0.0012 }; // 1.2 mm tip thickness
    double fin_pitch     { 0.0060 }; // center-to-center pitch: 6.0 mm (gap at root = 4.0 mm)
    double fin_y0        { 0.0060 }; // center of first fin: 6.0 mm

    // Backward-compatibility alias
    double fin_thick     { 0.0020 };

    // Source patch on bottom (z = 0): 10x10 mm, center offset dx=-10 mm, dy=+5 mm from base center (30mm, 30mm)
    double patch_size_x  { 0.010 }; // 10 mm
    double patch_size_y  { 0.010 }; // 10 mm
    double patch_center_x{ 0.020 }; // 20 mm (offset -10 mm from base center 30 mm)
    double patch_center_y{ 0.035 }; // 35 mm (offset +5 mm from base center 30 mm)

    double power_W       { 10.0 };  // 10 W heat input
};

class HeatSinkGeometry {
public:
    explicit HeatSinkGeometry(const HeatSinkParams& params = HeatSinkParams{});

    const HeatSinkParams& params() const noexcept { return params_; }

    // Solid classification
    bool is_in_solid(double x, double y, double z) const noexcept;
    bool is_in_source_patch(double x, double y, double z, double tol = 1e-6) const noexcept;

    // Face / Surface classification
    bool is_exposed_surface(double x, double y, double z, double tol = 1e-4) const noexcept;

    // Geometric properties
    double total_volume() const noexcept;
    double total_exposed_area() const noexcept;
    double base_exposed_area() const noexcept;
    double fins_exposed_area() const noexcept;
    double patch_area() const noexcept;

    // Export watertight ASCII STL representation
    bool export_stl(const std::string& filepath) const;

private:
    HeatSinkParams params_;
};

struct WatertightnessReport {
    bool is_watertight { false };
    int n_triangles { 0 };
    int n_edges { 0 };
    int n_non_manifold_edges { 0 };
    int n_inconsistent_orientations { 0 };
    double computed_volume { 0.0 };
    double volume_rel_error { 0.0 };
};

// Verify watertightness of an STL file:
// checks that every edge is shared by exactly two triangles,
// orientation is consistent, and closed volume matches expected volume.
WatertightnessReport check_stl_watertightness(const std::string& filepath, double expected_volume);

} // namespace t3d
} // namespace thermatrix
