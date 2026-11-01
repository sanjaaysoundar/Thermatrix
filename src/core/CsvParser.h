#pragma once
// CsvParser.h – Temperature CSV file importer and validator
// THERMATRIX C++17 backend
//
// Expected schema: node_id,x_m,y_m,z_m,temperature_C
//   node_id     : integer node identifier
//   x_m,y_m,z_m : Cartesian coordinates in metres
//   temperature_C: temperature in degrees Celsius
//
// Provenance of example file: data/example_temperatures.csv is SYNTHETIC
// (generated programmatically; it is NOT an actual ANSYS export).

#include <string>
#include <vector>
#include <stdexcept>

namespace thermatrix {

/// A single temperature node.
struct TempNode {
    long long     node_id;
    float         x, y, z;       ///< [m]
    float         temperature;   ///< [°C]
};

/// Result of a CSV parse operation.
struct CsvResult {
    bool                  success{false};
    std::string           error_message;
    std::vector<TempNode> nodes;
    std::size_t           line_count{0};
    float                 temp_min{0.f}, temp_max{0.f};
    /// Bounding box
    float xmin{}, xmax{}, ymin{}, ymax{}, zmin{}, zmax{};
};

/// Parse a temperature CSV file.
/// @param filepath Path to CSV file.
/// @returns CsvResult; on failure success==false and error_message is set.
///
/// Validation:
///   - File must be non-empty
///   - First line must match expected column headers (case-insensitive)
///   - Every data row must have exactly 5 fields
///   - node_id must be a valid integer
///   - x, y, z, temperature_C must be finite floats
///   - Malformed rows produce a descriptive error with row number
///   - Empty files and header-only files produce a clear message
CsvResult parseCsvFile(const std::string& filepath);

/// Returns a short summary of the parse result.
std::string csvSummary(const CsvResult& r);

} // namespace thermatrix
