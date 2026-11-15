// Mesh3D.cpp  –  THERMATRIX 3D Mesh Infrastructure
#include "Mesh3D.h"
#include <cmath>
#include <map>
#include <set>
#include <algorithm>
#include <stdexcept>
#include <iostream>
#include <fstream>

namespace thermatrix {
namespace t3d {

double FemMesh::total_volume() const noexcept {
    double v = 0.0;
    for (const auto& e : elements) v += e.volume;
    return v;
}

double FemMesh::total_convective_area() const noexcept {
    double a = 0.0;
    for (const auto& bf : boundary_faces) {
        if (bf.bc_type == BcType::Robin) a += bf.area;
    }
    return a;
}

double FemMesh::total_source_area() const noexcept {
    double a = 0.0;
    for (const auto& bf : boundary_faces) {
        if (bf.bc_type == BcType::HeatFlux) a += bf.area;
    }
    return a;
}

double StructuredGrid3D::total_convective_area() const noexcept {
    double a = 0.0;
    for (const auto& bf : boundary_faces) {
        if (bf.bc_type == BcType::Robin) a += bf.area;
    }
    return a;
}

double StructuredGrid3D::total_source_area() const noexcept {
    double a = 0.0;
    for (const auto& bf : boundary_faces) {
        if (bf.bc_type == BcType::HeatFlux) a += bf.area;
    }
    return a;
}

// ---------------------------------------------------------------------------
// Tetrahedral Volume Helper
// ---------------------------------------------------------------------------
namespace {
double tet_volume(const Point3D& a, const Point3D& b,
                  const Point3D& c, const Point3D& d) {
    // V = |(a - d) . ((b - d) x (c - d))| / 6
    double ax = a.x - d.x, ay = a.y - d.y, az = a.z - d.z;
    double bx = b.x - d.x, by = b.y - d.y, bz = b.z - d.z;
    double cx = c.x - d.x, cy = c.y - d.y, cz = c.z - d.z;

    double cross_x = by * cz - bz * cy;
    double cross_y = bz * cx - bx * cz;
    double cross_z = bx * cy - by * cx;

    return std::abs(ax * cross_x + ay * cross_y + az * cross_z) / 6.0;
}

double tri_area(const Point3D& a, const Point3D& b, const Point3D& c,
                std::array<double, 3>& normal) {
    double abx = b.x - a.x, aby = b.y - a.y, abz = b.z - a.z;
    double acx = c.x - a.x, acy = c.y - a.y, acz = c.z - a.z;

    double nx = aby * acz - abz * acy;
    double ny = abz * acx - abx * acz;
    double nz = abx * acy - aby * acx;

    double len = std::sqrt(nx * nx + ny * ny + nz * nz);
    if (len > 1e-15) {
        normal[0] = nx / len;
        normal[1] = ny / len;
        normal[2] = nz / len;
    } else {
        normal = {0.0, 0.0, 1.0};
    }
    return 0.5 * len;
}
} // namespace

// ---------------------------------------------------------------------------
// Gmsh .msh 2.2 Reader
// ---------------------------------------------------------------------------
FemMesh load_msh_file(const std::string& filepath) {
    FemMesh mesh;
    std::ifstream in(filepath);
    if (!in) {
        throw std::runtime_error("load_msh_file: cannot open " + filepath);
    }

    std::string line;
    std::map<int, int> node_id_to_idx;

    while (std::getline(in, line)) {
        if (line.find("$Nodes") != std::string::npos) {
            int n_nodes = 0;
            if (!(in >> n_nodes)) break;
            mesh.nodes.reserve(n_nodes);
            for (int i = 0; i < n_nodes; ++i) {
                int id;
                double x, y, z;
                in >> id >> x >> y >> z;
                node_id_to_idx[id] = static_cast<int>(mesh.nodes.size());
                mesh.nodes.push_back({x, y, z});
            }
        } else if (line.find("$Elements") != std::string::npos) {
            int n_elements = 0;
            if (!(in >> n_elements)) break;
            for (int i = 0; i < n_elements; ++i) {
                int elem_id, elem_type, n_tags;
                in >> elem_id >> elem_type >> n_tags;
                std::vector<int> tags(n_tags);
                for (int t = 0; t < n_tags; ++t) in >> tags[t];

                int phys_tag = (!tags.empty()) ? tags[0] : 0;

                if (elem_type == 2) {
                    // 3-node triangle (boundary face)
                    int n0, n1, n2;
                    in >> n0 >> n1 >> n2;
                    int v0 = node_id_to_idx[n0];
                    int v1 = node_id_to_idx[n1];
                    int v2 = node_id_to_idx[n2];

                    std::array<double, 3> normal;
                    double area = tri_area(mesh.nodes[v0], mesh.nodes[v1], mesh.nodes[v2], normal);

                    BcType bc = BcType::Robin;
                    if (phys_tag == 10) bc = BcType::HeatFlux;
                    else if (phys_tag == 20) bc = BcType::Adiabatic;
                    else bc = BcType::Robin;

                    mesh.boundary_faces.push_back({{v0, v1, v2}, normal, area, bc});
                } else if (elem_type == 4) {
                    // 4-node tetrahedron (volume element)
                    int n0, n1, n2, n3;
                    in >> n0 >> n1 >> n2 >> n3;
                    int v0 = node_id_to_idx[n0];
                    int v1 = node_id_to_idx[n1];
                    int v2 = node_id_to_idx[n2];
                    int v3 = node_id_to_idx[n3];

                    double vol = tet_volume(mesh.nodes[v0], mesh.nodes[v1],
                                            mesh.nodes[v2], mesh.nodes[v3]);
                    mesh.elements.push_back({{v0, v1, v2, v3}, vol});
                } else {
                    // Skip other element types (1 for line, 15 for point)
                    std::string rest;
                    std::getline(in, rest);
                }
            }
        }
    }

    return mesh;
}

// ---------------------------------------------------------------------------
// Generate FEM Tetrahedral Mesh
// ---------------------------------------------------------------------------
FemMesh generate_fem_mesh(const HeatSinkGeometry& geom, MeshResolution res) {
    std::string msh_basename = (res == MeshResolution::Coarse) ? "heatsink_coarse.msh" :
                               (res == MeshResolution::Medium) ? "heatsink_medium.msh" :
                                                                 "heatsink_fine.msh";

    const std::vector<std::string> search_dirs = { "data/", "../data/", "bin/data/", "" };
    for (const auto& dir : search_dirs) {
        std::string cand = dir + msh_basename;
        std::ifstream check(cand);
        if (check.good()) {
            return load_msh_file(cand);
        }
    }

    // Attempt offline gmsh generator script if file not found
    std::system("python scripts/make_heatsink.py");
    for (const auto& dir : search_dirs) {
        std::string cand = dir + msh_basename;
        std::ifstream check(cand);
        if (check.good()) {
            return load_msh_file(cand);
        }
    }

    // Fallback: structured tetrahedral hex subdivision
    FemMesh mesh;
    const auto& p = geom.params();

    int scale = 1;
    if (res == MeshResolution::Medium) scale = 2;
    else if (res == MeshResolution::Fine) scale = 3;

    int nx = 16 * scale;
    double dx = p.base_length / nx;
    std::vector<double> xs(nx + 1);
    for (int i = 0; i <= nx; ++i) xs[i] = i * dx;

    // Y intervals matching fin positions exactly:
    // [0, 0.005], [0.005, 0.007], [0.007, 0.017], [0.017, 0.019],
    // [0.019, 0.029], [0.029, 0.031], [0.031, 0.041], [0.041, 0.043],
    // [0.043, 0.053], [0.053, 0.055], [0.055, 0.060]
    struct YInterval {
        double y0, y1;
        int sub_divs;
        bool is_fin;
        int fin_id;
    };

    std::vector<YInterval> y_intervals;
    y_intervals.push_back({0.0, 0.005, 1 * scale, false, -1}); // Margin
    for (int f = 0; f < p.n_fins; ++f) {
        double yc = p.fin_y0 + f * p.fin_pitch;
        double f0 = yc - 0.5 * p.fin_thick;
        double f1 = yc + 0.5 * p.fin_thick;
        y_intervals.push_back({f0, f1, 1 * scale, true, f}); // Fin f

        if (f + 1 < p.n_fins) {
            double next_yc = p.fin_y0 + (f + 1) * p.fin_pitch;
            double g0 = f1;
            double g1 = next_yc - 0.5 * p.fin_thick;
            y_intervals.push_back({g0, g1, 2 * scale, false, -1}); // Gap
        }
    }
    y_intervals.push_back({0.055, 0.060, 1 * scale, false, -1}); // Margin

    std::vector<double> ys;
    std::vector<bool> y_is_fin_block; // for each block in y
    for (const auto& iv : y_intervals) {
        double dy = (iv.y1 - iv.y0) / iv.sub_divs;
        for (int s = 0; s < iv.sub_divs; ++s) {
            if (ys.empty()) ys.push_back(iv.y0 + s * dy);
            ys.push_back(iv.y0 + (s + 1) * dy);
            y_is_fin_block.push_back(iv.is_fin);
        }
    }
    // Remove duplicate coordinates from boundary joins
    ys.erase(std::unique(ys.begin(), ys.end(), [](double a, double b) {
        return std::abs(a - b) < 1e-9;
    }), ys.end());
    int ny = static_cast<int>(ys.size()) - 1;

    // Z coordinates: Base [0, base_height], Fin [base_height, base_height + fin_height]
    int nz_base = 3 * scale;
    double dz_base = p.base_height / nz_base;
    std::vector<double> zs_base(nz_base + 1);
    for (int k = 0; k <= nz_base; ++k) zs_base[k] = k * dz_base;

    int nz_fin = 6 * scale;
    double dz_fin = p.fin_height / nz_fin;
    std::vector<double> zs_fin(nz_fin + 1);
    for (int k = 0; k <= nz_fin; ++k) zs_fin[k] = p.base_height + k * dz_fin;

    // Node manager to deduplicate vertices across base and fins
    auto point_key = [](double x, double y, double z) {
        long long ix = std::llround(x * 1e6);
        long long iy = std::llround(y * 1e6);
        long long iz = std::llround(z * 1e6);
        return std::make_tuple(ix, iy, iz);
    };

    std::map<std::tuple<long long, long long, long long>, int> node_map;
    auto get_or_add_node = [&](double x, double y, double z) -> int {
        auto key = point_key(x, y, z);
        auto it = node_map.find(key);
        if (it != node_map.end()) return it->second;
        int id = static_cast<int>(mesh.nodes.size());
        mesh.nodes.push_back({x, y, z});
        node_map[key] = id;
        return id;
    };

    // Helper: split a hex brick into 6 tetrahedra
    auto add_hex_tets = [&](int c000, int c100, int c110, int c010,
                            int c001, int c101, int c111, int c011) {
        std::array<std::array<int, 4>, 6> tet_indices = {{
            {c000, c100, c110, c111},
            {c000, c100, c101, c111},
            {c000, c001, c101, c111},
            {c000, c010, c110, c111},
            {c000, c010, c011, c111},
            {c000, c001, c011, c111}
        }};

        for (const auto& tv : tet_indices) {
            double v = tet_volume(mesh.nodes[tv[0]], mesh.nodes[tv[1]],
                                  mesh.nodes[tv[2]], mesh.nodes[tv[3]]);
            if (v > 1e-18) {
                mesh.elements.push_back({tv, v});
            }
        }
    };

    // 1. Generate Base Bricks
    for (int i = 0; i < nx; ++i) {
        for (int j = 0; j < ny; ++j) {
            for (int k = 0; k < nz_base; ++k) {
                int c000 = get_or_add_node(xs[i],   ys[j],   zs_base[k]);
                int c100 = get_or_add_node(xs[i+1], ys[j],   zs_base[k]);
                int c110 = get_or_add_node(xs[i+1], ys[j+1], zs_base[k]);
                int c010 = get_or_add_node(xs[i],   ys[j+1], zs_base[k]);

                int c001 = get_or_add_node(xs[i],   ys[j],   zs_base[k+1]);
                int c101 = get_or_add_node(xs[i+1], ys[j],   zs_base[k+1]);
                int c111 = get_or_add_node(xs[i+1], ys[j+1], zs_base[k+1]);
                int c011 = get_or_add_node(xs[i],   ys[j+1], zs_base[k+1]);

                add_hex_tets(c000, c100, c110, c010, c001, c101, c111, c011);
            }
        }
    }

    // 2. Generate Fin Bricks
    for (int j = 0; j < ny; ++j) {
        if (!y_is_fin_block[j]) continue;

        for (int i = 0; i < nx; ++i) {
            for (int k = 0; k < nz_fin; ++k) {
                int c000 = get_or_add_node(xs[i],   ys[j],   zs_fin[k]);
                int c100 = get_or_add_node(xs[i+1], ys[j],   zs_fin[k]);
                int c110 = get_or_add_node(xs[i+1], ys[j+1], zs_fin[k]);
                int c010 = get_or_add_node(xs[i],   ys[j+1], zs_fin[k]);

                int c001 = get_or_add_node(xs[i],   ys[j],   zs_fin[k+1]);
                int c101 = get_or_add_node(xs[i+1], ys[j],   zs_fin[k+1]);
                int c111 = get_or_add_node(xs[i+1], ys[j+1], zs_fin[k+1]);
                int c011 = get_or_add_node(xs[i],   ys[j+1], zs_fin[k+1]);

                add_hex_tets(c000, c100, c110, c010, c001, c101, c111, c011);
            }
        }
    }

    // 3. Extract Boundary Faces
    // A triangular face is on the boundary iff it belongs to exactly one tetrahedron.
    struct FaceKey {
        int v0, v1, v2;
        bool operator<(const FaceKey& o) const {
            if (v0 != o.v0) return v0 < o.v0;
            if (v1 != o.v1) return v1 < o.v1;
            return v2 < o.v2;
        }
    };

    struct FaceRecord {
        std::array<int, 3> original_v;
        int count { 0 };
    };

    std::map<FaceKey, FaceRecord> face_map;
    auto record_face = [&](int a, int b, int c) {
        std::array<int, 3> sorted = {a, b, c};
        std::sort(sorted.begin(), sorted.end());
        FaceKey key{sorted[0], sorted[1], sorted[2]};
        auto& rec = face_map[key];
        rec.original_v = {a, b, c};
        rec.count++;
    };

    for (const auto& elem : mesh.elements) {
        const auto& ev = elem.v;
        record_face(ev[0], ev[1], ev[2]);
        record_face(ev[0], ev[1], ev[3]);
        record_face(ev[0], ev[2], ev[3]);
        record_face(ev[1], ev[2], ev[3]);
    }

    for (const auto& pair : face_map) {
        if (pair.second.count == 1) {
            const auto& fv = pair.second.original_v;
            const auto& p0 = mesh.nodes[fv[0]];
            const auto& p1 = mesh.nodes[fv[1]];
            const auto& p2 = mesh.nodes[fv[2]];

            std::array<double, 3> normal;
            double area = tri_area(p0, p1, p2, normal);

            // Centroid
            double cx = (p0.x + p1.x + p2.x) / 3.0;
            double cy = (p0.y + p1.y + p2.y) / 3.0;
            double cz = (p0.z + p1.z + p2.z) / 3.0;

            BcType bctype = BcType::Robin;
            if (std::abs(cz) < 1e-6) {
                // Bottom underside: source patch or adiabatic
                if (geom.is_in_source_patch(cx, cy, cz)) {
                    bctype = BcType::HeatFlux;
                } else {
                    bctype = BcType::Adiabatic;
                }
            } else {
                bctype = BcType::Robin;
            }

            mesh.boundary_faces.push_back({fv, normal, area, bctype});
        }
    }

    return mesh;
}

// ---------------------------------------------------------------------------
// Generate Structured Grid (FDM / FVM)
// ---------------------------------------------------------------------------
StructuredGrid3D generate_structured_grid(const HeatSinkGeometry& geom, MeshResolution res) {
    StructuredGrid3D grid;
    const auto& p = geom.params();

    int scale = 1;
    if (res == MeshResolution::Medium) scale = 2;
    else if (res == MeshResolution::Fine) scale = 3;

    grid.nx = 30 * scale;
    grid.ny = 60 * scale;
    grid.nz = 29 * scale;

    grid.origin = {0.0, 0.0, 0.0};
    grid.dx = p.base_length / grid.nx;
    grid.dy = p.base_width / grid.ny;
    grid.dz = (p.base_height + p.fin_height) / grid.nz;

    int total_cells = grid.nx * grid.ny * grid.nz;
    grid.is_solid.assign(total_cells, false);
    grid.cell_to_dof.assign(total_cells, -1);

    int dof = 0;
    for (int k = 0; k < grid.nz; ++k) {
        for (int j = 0; j < grid.ny; ++j) {
            for (int i = 0; i < grid.nx; ++i) {
                Point3D c = grid.cell_center(i, j, k);
                if (geom.is_in_solid(c.x, c.y, c.z)) {
                    int idx = grid.flat_idx(i, j, k);
                    grid.is_solid[idx] = true;
                    grid.cell_to_dof[idx] = dof++;
                    grid.dof_to_coord.push_back({i, j, k});
                }
            }
        }
    }

    // Build faces for FVM & FDM
    double area_x = grid.dy * grid.dz;
    double area_y = grid.dx * grid.dz;
    double area_z = grid.dx * grid.dy;

    const int di[6] = { -1,  1,  0,  0,  0,  0 };
    const int dj[6] = {  0,  0, -1,  1,  0,  0 };
    const int dk[6] = {  0,  0,  0,  0, -1,  1 };
    const FaceDir dirs[6] = {
        FaceDir::Xminus, FaceDir::Xplus,
        FaceDir::Yminus, FaceDir::Yplus,
        FaceDir::Zminus, FaceDir::Zplus
    };
    const double areas[6] = { area_x, area_x, area_y, area_y, area_z, area_z };

    for (int dof_idx = 0; dof_idx < grid.n_solid_cells(); ++dof_idx) {
        const auto& c = grid.dof_to_coord[dof_idx];
        Point3D center = grid.cell_center(c.i, c.j, c.k);

        for (int d = 0; d < 6; ++d) {
            int ni = c.i + di[d];
            int nj = c.j + dj[d];
            int nk = c.k + dk[d];

            bool neighbor_solid = false;
            int n_dof = -1;

            if (ni >= 0 && ni < grid.nx &&
                nj >= 0 && nj < grid.ny &&
                nk >= 0 && nk < grid.nz) {
                int n_idx = grid.flat_idx(ni, nj, nk);
                if (grid.is_solid[n_idx]) {
                    neighbor_solid = true;
                    n_dof = grid.cell_to_dof[n_idx];
                }
            }

            double fx = center.x + 0.5 * di[d] * grid.dx;
            double fy = center.y + 0.5 * dj[d] * grid.dy;
            double fz = center.z + 0.5 * dk[d] * grid.dz;

            if (neighbor_solid) {
                // Internal face: add once when dof_idx < n_dof
                if (dof_idx < n_dof) {
                    grid.internal_faces.push_back({
                        dof_idx, n_dof, dirs[d], areas[d],
                        BcType::Adiabatic, fx, fy, fz
                    });
                }
            } else {
                // Boundary face
                BcType bctype = BcType::Robin;
                if (dirs[d] == FaceDir::Zminus && std::abs(fz) < 1e-6) {
                    if (geom.is_in_source_patch(fx, fy, fz)) {
                        bctype = BcType::HeatFlux;
                    } else {
                        bctype = BcType::Adiabatic;
                    }
                } else {
                    bctype = BcType::Robin;
                }

                grid.boundary_faces.push_back({
                    dof_idx, -1, dirs[d], areas[d],
                    bctype, fx, fy, fz
                });
            }
        }
    }

    return grid;
}

} // namespace t3d
} // namespace thermatrix
