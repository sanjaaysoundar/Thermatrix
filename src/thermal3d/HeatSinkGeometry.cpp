// HeatSinkGeometry.cpp  –  THERMATRIX Heat Sink CAD & Discretization Specification
#include "HeatSinkGeometry.h"
#include <fstream>
#include <iomanip>
#include <cmath>
#include <sstream>

namespace thermatrix {
namespace t3d {

HeatSinkGeometry::HeatSinkGeometry(const HeatSinkParams& params)
    : params_(params) {}

bool HeatSinkGeometry::is_in_solid(double x, double y, double z) const noexcept {
    // Check bounding box
    if (x < 0.0 || x > params_.base_length ||
        y < 0.0 || y > params_.base_width ||
        z < 0.0 || z > (params_.base_height + params_.fin_height)) {
        return false;
    }

    // Base slab: z in [0, base_height]
    if (z <= params_.base_height) {
        return true;
    }

    // Fins: z in (base_height, base_height + fin_height]
    for (int i = 0; i < params_.n_fins; ++i) {
        double yc = params_.fin_y0 + i * params_.fin_pitch;
        double y_min = yc - 0.5 * params_.fin_thick;
        double y_max = yc + 0.5 * params_.fin_thick;
        // Use half-open [y_min, y_max) to prevent double-counting when voxel center lies on boundary
        if (y >= y_min && y < y_max) {
            return true;
        }
    }

    return false;
}

bool HeatSinkGeometry::is_in_source_patch(double x, double y, double z, double tol) const noexcept {
    if (std::abs(z) > tol) return false;
    double px_min = params_.patch_center_x - 0.5 * params_.patch_size_x;
    double px_max = params_.patch_center_x + 0.5 * params_.patch_size_x;
    double py_min = params_.patch_center_y - 0.5 * params_.patch_size_y;
    double py_max = params_.patch_center_y + 0.5 * params_.patch_size_y;

    return (x >= px_min - tol && x <= px_max + tol &&
            y >= py_min - tol && y <= py_max + tol);
}

double HeatSinkGeometry::total_volume() const noexcept {
    double v_base = params_.base_length * params_.base_width * params_.base_height;
    double v_fins = params_.n_fins * (params_.fin_length * params_.fin_thick * params_.fin_height);
    return v_base + v_fins;
}

double HeatSinkGeometry::base_exposed_area() const noexcept {
    // 2 x-normal sides: 2 * (base_width * base_height)
    double a_x = 2.0 * params_.base_width * params_.base_height;
    // 2 y-normal sides: 2 * (base_length * base_height)
    double a_y = 2.0 * params_.base_length * params_.base_height;
    // Top surface minus fin footprints
    double a_top = (params_.base_length * params_.base_width) -
                   params_.n_fins * (params_.fin_length * params_.fin_thick);
    return a_x + a_y + a_top;
}

double HeatSinkGeometry::fins_exposed_area() const noexcept {
    double a_lateral = 2.0 * params_.n_fins * (params_.fin_length * params_.fin_height);
    double a_ends    = 2.0 * params_.n_fins * (params_.fin_thick * params_.fin_height);
    double a_tips    = params_.n_fins * (params_.fin_length * params_.fin_thick);
    return a_lateral + a_ends + a_tips;
}

double HeatSinkGeometry::total_exposed_area() const noexcept {
    return base_exposed_area() + fins_exposed_area();
}

double HeatSinkGeometry::patch_area() const noexcept {
    return params_.patch_size_x * params_.patch_size_y;
}

namespace {
struct Vec3 { double x, y, z; };
struct Triangle { Vec3 n, v1, v2, v3; };

void add_quad(std::vector<Triangle>& tris,
              Vec3 n, Vec3 a, Vec3 b, Vec3 c, Vec3 d) {
    tris.push_back({n, a, b, c});
    tris.push_back({n, a, c, d});
}

void add_box(std::vector<Triangle>& tris,
             double x0, double x1, double y0, double y1, double z0, double z1) {
    // -X face
    add_quad(tris, {-1,0,0}, {x0,y0,z0}, {x0,y1,z0}, {x0,y1,z1}, {x0,y0,z1});
    // +X face
    add_quad(tris, {1,0,0},  {x1,y0,z0}, {x1,y0,z1}, {x1,y1,z1}, {x1,y1,z0});
    // -Y face
    add_quad(tris, {0,-1,0}, {x0,y0,z0}, {x0,y0,z1}, {x1,y0,z1}, {x1,y0,z0});
    // +Y face
    add_quad(tris, {0,1,0},  {x0,y1,z0}, {x1,y1,z0}, {x1,y1,z1}, {x0,y1,z1});
    // -Z face
    add_quad(tris, {0,0,-1}, {x0,y0,z0}, {x1,y0,z0}, {x1,y1,z0}, {x0,y1,z0});
    // +Z face
    add_quad(tris, {0,0,1},  {x0,y0,z1}, {x0,y1,z1}, {x1,y1,z1}, {x1,y0,z1});
}
} // namespace

bool HeatSinkGeometry::export_stl(const std::string& filepath) const {
    std::vector<Triangle> tris;

    // Export base box
    add_box(tris, 0.0, params_.base_length,
                  0.0, params_.base_width,
                  0.0, params_.base_height);

    // Export each fin box
    for (int i = 0; i < params_.n_fins; ++i) {
        double yc = params_.fin_y0 + i * params_.fin_pitch;
        double y0 = yc - 0.5 * params_.fin_thick;
        double y1 = yc + 0.5 * params_.fin_thick;
        add_box(tris, 0.0, params_.fin_length,
                      y0, y1,
                      params_.base_height, params_.base_height + params_.fin_height);
    }

    std::ofstream out(filepath);
    if (!out) return false;

    out << "solid HeatSink\n";
    for (const auto& t : tris) {
        out << "  facet normal " << t.n.x << " " << t.n.y << " " << t.n.z << "\n"
            << "    outer loop\n"
            << "      vertex " << t.v1.x << " " << t.v1.y << " " << t.v1.z << "\n"
            << "      vertex " << t.v2.x << " " << t.v2.y << " " << t.v2.z << "\n"
            << "      vertex " << t.v3.x << " " << t.v3.y << " " << t.v3.z << "\n"
            << "    endloop\n"
            << "  endfacet\n";
    }
    out << "endsolid HeatSink\n";
    return true;
}

} // namespace t3d
} // namespace thermatrix
