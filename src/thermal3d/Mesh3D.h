#pragma once
// Mesh3D.h  –  THERMATRIX 3D Mesh Infrastructure
// Unifies FEM Tetrahedral Mesh, FDM Grid, and FVM Control Volume Grid.
// namespace thermatrix::t3d

#include "HeatSinkGeometry.h"
#include <array>
#include <vector>
#include <string>

namespace thermatrix {
namespace t3d {

enum class MeshResolution : int {
    Coarse = 0,
    Medium = 1,
    Fine   = 2
};

struct Point3D {
    double x { 0.0 };
    double y { 0.0 };
    double z { 0.0 };
};

// ---------------------------------------------------------------------------
// FEM Mesh Data Structures
// ---------------------------------------------------------------------------
struct TetElement {
    std::array<int, 4> v; // 4 node indices (P1 linear tetrahedron)
    double volume;
};

struct BoundaryTri {
    std::array<int, 3> v; // 3 node indices
    std::array<double, 3> normal;
    double area;
    BcType bc_type;
};

struct FemMesh {
    std::vector<Point3D>     nodes;
    std::vector<TetElement>  elements;
    std::vector<BoundaryTri> boundary_faces;

    int n_nodes() const noexcept { return static_cast<int>(nodes.size()); }
    int n_elements() const noexcept { return static_cast<int>(elements.size()); }
    int n_boundary_faces() const noexcept { return static_cast<int>(boundary_faces.size()); }
    double total_volume() const noexcept;
    double total_convective_area() const noexcept;
    double total_source_area() const noexcept;
};

// ---------------------------------------------------------------------------
// FDM / FVM Structured Grid Data Structures
// ---------------------------------------------------------------------------
struct GridCoord {
    int i, j, k;
};

struct VoxelFace {
    int cell_p;          // Primary cell index
    int cell_n;          // Neighbor cell index (-1 if boundary)
    FaceDir dir;         // Face direction relative to P
    double area;         // Face area [m²]
    BcType bc_type;      // BC type if cell_n == -1
    double x, y, z;      // Face center coordinates [m]
};

struct StructuredGrid3D {
    int nx { 0 }, ny { 0 }, nz { 0 };
    double dx { 0.0 }, dy { 0.0 }, dz { 0.0 };
    Point3D origin { 0.0, 0.0, 0.0 };

    // Voxel storage: size nx * ny * nz
    std::vector<bool> is_solid;
    std::vector<int>  cell_to_dof; // -1 if not solid, else 0..n_solid-1
    std::vector<GridCoord> dof_to_coord;

    // FVM cell faces
    std::vector<VoxelFace> internal_faces;
    std::vector<VoxelFace> boundary_faces;

    int n_solid_cells() const noexcept { return static_cast<int>(dof_to_coord.size()); }
    int flat_idx(int i, int j, int k) const noexcept {
        return (k * ny + j) * nx + i;
    }
    Point3D cell_center(int i, int j, int k) const noexcept {
        return { origin.x + (i + 0.5) * dx,
                 origin.y + (j + 0.5) * dy,
                 origin.z + (k + 0.5) * dz };
    }
    Point3D node_pos(int i, int j, int k) const noexcept {
        return { origin.x + i * dx,
                 origin.y + j * dy,
                 origin.z + k * dz };
    }
    double total_volume() const noexcept {
        return n_solid_cells() * (dx * dy * dz);
    }
    double total_convective_area() const noexcept;
    double total_source_area() const noexcept;
};

// ---------------------------------------------------------------------------
// Mesh Generator & MSH Reader Functions
// ---------------------------------------------------------------------------
FemMesh load_msh_file(const std::string& filepath);

FemMesh generate_fem_mesh(const HeatSinkGeometry& geom, MeshResolution res);

StructuredGrid3D generate_structured_grid(const HeatSinkGeometry& geom, MeshResolution res);

FemMesh generate_box_fem_mesh(double Lx, double Ly, double Lz, int nx, int ny, int nz);

StructuredGrid3D generate_box_structured_grid(double Lx, double Ly, double Lz, int nx, int ny, int nz);

} // namespace t3d
} // namespace thermatrix
