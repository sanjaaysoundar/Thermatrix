#pragma once
// StlParser.h – ASCII STL file importer and validator
// THERMATRIX C++17 backend
// Author: THERMATRIX Project
// All units: metres (coordinates as stored in file)

#include <string>
#include <vector>
#include <array>
#include <stdexcept>

namespace thermatrix {

/// A single triangle in 3D space.
struct Triangle {
    std::array<float, 3> normal;   ///< Surface normal (from STL header; may be zero)
    std::array<float, 3> v0;       ///< Vertex 0  [m]
    std::array<float, 3> v1;       ///< Vertex 1  [m]
    std::array<float, 3> v2;       ///< Vertex 2  [m]
};

/// Result of an STL parse operation.
struct StlResult {
    bool              success{false};
    std::string       error_message;
    std::vector<Triangle> triangles;
    std::size_t       line_count{0};   ///< Lines processed
    std::size_t       triangle_count{0};
    /// Bounding box: [min_x,min_y,min_z], [max_x,max_y,max_z]
    std::array<float,3> bbox_min{};
    std::array<float,3> bbox_max{};
};

/// Parse an ASCII STL file.
/// @param filepath Absolute or relative path to the .stl file.
/// @returns StlResult; on failure success==false and error_message is set.
/// 
/// Validation performed:
///   - File must be non-empty
///   - Must begin with "solid" keyword
///   - Every facet must have exactly 3 vertices
///   - All coordinate values must be finite (no NaN/Inf)
///   - Malformed lines produce a descriptive error with line number
///
/// Tested file-size limits: up to ~50 MB / ~500 000 triangles without issue on
/// a typical desktop (limited by available RAM).  Larger files may be slow due
/// to std::vector reallocation; pre-reservation is performed for speed.
StlResult parseStlFile(const std::string& filepath);

/// Returns a short human-readable summary of the parse result.
std::string stlSummary(const StlResult& r);

} // namespace thermatrix
