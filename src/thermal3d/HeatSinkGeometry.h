#pragma once
// HeatSinkGeometry.h  –  THERMATRIX Heat Sink CAD & Discretization Specification
// namespace thermatrix::t3d

#include "ThermalProblem.h"
#include <string>
#include <vector>

namespace thermatrix {
namespace t3d {

struct HeatSinkParams {
    double base_length { 0.080 }; // x dimension: 80 mm
    double base_width  { 0.060 }; // y dimension: 60 mm
    double base_height { 0.010 }; // z dimension: 10 mm

    int    n_fins      { 5 };
    double fin_length  { 0.080 }; // x dimension: 80 mm
    double fin_height  { 0.030 }; // z dimension: 30 mm (total height = 40 mm)
    double fin_thick   { 0.002 }; // y dimension: 2 mm
    double fin_pitch   { 0.012 }; // center-to-center pitch: 12 mm
    double fin_y0      { 0.006 }; // center of first fin: 6 mm

    // Source patch on bottom (z = 0)
    double patch_size_x { 0.010 }; // 10 mm
    double patch_size_y { 0.010 }; // 10 mm
    double patch_center_x { 0.030 }; // 30 mm (offset -10 mm from base center 40 mm)
    double patch_center_y { 0.035 }; // 35 mm (offset +5 mm from base center 30 mm)

    double power_W { 10.0 }; // 10 W heat input
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

} // namespace t3d
} // namespace thermatrix
