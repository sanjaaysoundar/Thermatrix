// StlParser.cpp – ASCII STL file importer and validator
// THERMATRIX C++17 backend

#include "StlParser.h"

#include <fstream>
#include <sstream>
#include <cctype>
#include <cmath>
#include <algorithm>
#include <limits>
#include <charconv>

namespace thermatrix {

// ─── helpers ────────────────────────────────────────────────────────────────

static std::string trimLeft(const std::string& s) {
    auto it = std::find_if(s.begin(), s.end(),
                           [](unsigned char c){ return !std::isspace(c); });
    return std::string(it, s.end());
}

static bool startsWith(const std::string& s, const char* prefix) {
    return s.rfind(prefix, 0) == 0;
}

static bool parseFloat3(const std::string& tok1, const std::string& tok2,
                         const std::string& tok3,
                         std::array<float,3>& out,
                         std::string& err) {
    float vals[3];
    const std::string* strs[3] = {&tok1, &tok2, &tok3};
    for (int i = 0; i < 3; ++i) {
        try {
            vals[i] = std::stof(*strs[i]);
        } catch(...) {
            err = "Cannot parse float: '" + *strs[i] + "'";
            return false;
        }
        if (!std::isfinite(vals[i])) {
            err = "Non-finite value: " + *strs[i];
            return false;
        }
    }
    out = {vals[0], vals[1], vals[2]};
    return true;
}

// ─── main parser ────────────────────────────────────────────────────────────

StlResult parseStlFile(const std::string& filepath) {
    StlResult result;

    std::ifstream ifs(filepath, std::ios::in | std::ios::ate);
    if (!ifs.is_open()) {
        result.error_message = "Cannot open file: " + filepath;
        return result;
    }
    std::streamsize fileSize = ifs.tellg();
    ifs.seekg(0);
    if (fileSize == 0) {
        result.error_message = "File is empty: " + filepath;
        return result;
    }

    // Estimate triangle count from file size for reservation
    // ASCII STL averages ~250 bytes per triangle
    std::size_t estTriangles = static_cast<std::size_t>(fileSize / 250);
    result.triangles.reserve(estTriangles);

    // Check it is ASCII STL (binary STL is not supported by this parser)
    {
        char header[6] = {};
        ifs.read(header, 5);
        ifs.seekg(0);
        std::string hdr(header, 5);
        // Convert to lowercase for check
        std::transform(hdr.begin(), hdr.end(), hdr.begin(),
                       [](unsigned char c){ return std::tolower(c); });
        if (hdr != "solid") {
            result.error_message =
                "File does not begin with 'solid' – not a valid ASCII STL. "
                "(Binary STL is not supported.)";
            return result;
        }
    }

    // Initialise bounding box
    constexpr float INF = std::numeric_limits<float>::infinity();
    result.bbox_min = { INF,  INF,  INF};
    result.bbox_max = {-INF, -INF, -INF};

    std::string line;
    std::size_t lineNo = 0;

    enum class State { Idle, InFacet, InLoop, Vertex0, Vertex1, Vertex2 };
    State state = State::Idle;
    Triangle current{};
    int vertexIdx = 0;

    auto updateBBox = [&](const std::array<float,3>& v) {
        for (int d = 0; d < 3; ++d) {
            result.bbox_min[d] = std::min(result.bbox_min[d], v[d]);
            result.bbox_max[d] = std::max(result.bbox_max[d], v[d]);
        }
    };

    while (std::getline(ifs, line)) {
        ++lineNo;
        std::string trimmed = trimLeft(line);
        // Skip blank lines
        if (trimmed.empty()) continue;

        std::istringstream ss(trimmed);
        std::string keyword;
        ss >> keyword;
        // Case-insensitive compare
        std::transform(keyword.begin(), keyword.end(), keyword.begin(),
                       [](unsigned char c){ return std::tolower(c); });

        if (keyword == "solid") {
            // Beginning of solid block (or nested – unusual but handled)
            state = State::Idle;
            continue;
        }
        if (keyword == "endsolid") {
            state = State::Idle;
            continue;
        }
        if (keyword == "facet") {
            if (state != State::Idle) {
                result.error_message = "Unexpected 'facet' at line " + std::to_string(lineNo);
                return result;
            }
            std::string normalKw, nx_s, ny_s, nz_s;
            ss >> normalKw >> nx_s >> ny_s >> nz_s;
            std::string ferr;
            if (!parseFloat3(nx_s, ny_s, nz_s, current.normal, ferr)) {
                result.error_message = "Line " + std::to_string(lineNo) + ": " + ferr;
                return result;
            }
            state = State::InFacet;
            vertexIdx = 0;
            continue;
        }
        if (keyword == "outer") {
            // "outer loop"
            if (state != State::InFacet) {
                result.error_message = "Unexpected 'outer loop' at line " + std::to_string(lineNo);
                return result;
            }
            state = State::InLoop;
            continue;
        }
        if (keyword == "vertex") {
            if (state != State::InLoop) {
                result.error_message = "Unexpected 'vertex' at line " + std::to_string(lineNo);
                return result;
            }
            std::string xs, ys, zs;
            ss >> xs >> ys >> zs;
            std::string ferr;
            std::array<float,3> v{};
            if (!parseFloat3(xs, ys, zs, v, ferr)) {
                result.error_message = "Line " + std::to_string(lineNo) + ": " + ferr;
                return result;
            }
            if      (vertexIdx == 0) { current.v0 = v; updateBBox(v); }
            else if (vertexIdx == 1) { current.v1 = v; updateBBox(v); }
            else if (vertexIdx == 2) { current.v2 = v; updateBBox(v); }
            else {
                result.error_message = "More than 3 vertices in facet ending near line " +
                                       std::to_string(lineNo);
                return result;
            }
            ++vertexIdx;
            continue;
        }
        if (keyword == "endloop") {
            if (state != State::InLoop) {
                result.error_message = "Unexpected 'endloop' at line " + std::to_string(lineNo);
                return result;
            }
            if (vertexIdx != 3) {
                result.error_message = "Facet has " + std::to_string(vertexIdx) +
                                       " vertices (expected 3) near line " +
                                       std::to_string(lineNo);
                return result;
            }
            state = State::InFacet;
            continue;
        }
        if (keyword == "endfacet") {
            if (state != State::InFacet) {
                result.error_message = "Unexpected 'endfacet' at line " + std::to_string(lineNo);
                return result;
            }
            result.triangles.push_back(current);
            state = State::Idle;
            continue;
        }
        // Unknown keyword – skip with a warning embedded in error but don't fail
        // (some exporters add comments or extra fields)
    }

    result.line_count     = lineNo;
    result.triangle_count = result.triangles.size();

    if (result.triangle_count == 0) {
        result.error_message = "File parsed successfully but contains no triangles.";
        return result;
    }

    result.success = true;
    return result;
}

std::string stlSummary(const StlResult& r) {
    if (!r.success) return "STL Error: " + r.error_message;
    std::ostringstream os;
    os << "STL OK – " << r.triangle_count << " triangles  |  "
       << "BBox [" << r.bbox_min[0] << "," << r.bbox_min[1] << "," << r.bbox_min[2] << "] – ["
       <<              r.bbox_max[0] << "," << r.bbox_max[1] << "," << r.bbox_max[2] << "] m";
    return os.str();
}

} // namespace thermatrix
