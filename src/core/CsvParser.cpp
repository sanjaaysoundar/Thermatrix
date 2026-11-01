// CsvParser.cpp – Temperature CSV file importer and validator
// THERMATRIX C++17 backend

#include "CsvParser.h"

#include <fstream>
#include <sstream>
#include <algorithm>
#include <cmath>
#include <limits>

namespace thermatrix {

// ─── helpers ────────────────────────────────────────────────────────────────

static std::string trim(const std::string& s) {
    auto b = s.find_first_not_of(" \t\r\n");
    if (b == std::string::npos) return {};
    auto e = s.find_last_not_of(" \t\r\n");
    return s.substr(b, e - b + 1);
}

static std::string toLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c){ return std::tolower(c); });
    return s;
}

static std::vector<std::string> splitCsv(const std::string& line) {
    std::vector<std::string> cols;
    std::istringstream ss(line);
    std::string field;
    while (std::getline(ss, field, ',')) {
        cols.push_back(trim(field));
    }
    return cols;
}

// ─── main parser ────────────────────────────────────────────────────────────

CsvResult parseCsvFile(const std::string& filepath) {
    CsvResult result;

    std::ifstream ifs(filepath);
    if (!ifs.is_open()) {
        result.error_message = "Cannot open file: " + filepath;
        return result;
    }

    std::string line;
    std::size_t lineNo = 0;

    // ── Header ──────────────────────────────────────────────────────────────
    if (!std::getline(ifs, line)) {
        result.error_message = "File is empty: " + filepath;
        return result;
    }
    ++lineNo;
    auto headers = splitCsv(line);
    if (headers.size() != 5) {
        result.error_message = "Header line must have 5 columns "
                               "(node_id,x_m,y_m,z_m,temperature_C); found " +
                               std::to_string(headers.size());
        return result;
    }

    // Validate header names (case-insensitive)
    const std::string expected[5] = {"node_id","x_m","y_m","z_m","temperature_c"};
    for (int i = 0; i < 5; ++i) {
        if (toLower(headers[i]) != expected[i]) {
            result.error_message = "Header column " + std::to_string(i+1) +
                                   " expected '" + expected[i] +
                                   "' (case-insensitive), got '" + headers[i] + "'";
            return result;
        }
    }

    // ── Data rows ───────────────────────────────────────────────────────────
    constexpr float INF = std::numeric_limits<float>::infinity();
    float tmin =  INF, tmax = -INF;
    float xmin =  INF, xmax = -INF;
    float ymin =  INF, ymax = -INF;
    float zmin =  INF, zmax = -INF;

    // Reserve estimate
    result.nodes.reserve(1024);

    while (std::getline(ifs, line)) {
        ++lineNo;
        if (trim(line).empty()) continue;   // skip blank rows

        auto cols = splitCsv(line);
        if (cols.size() != 5) {
            result.error_message = "Row " + std::to_string(lineNo) +
                                   ": expected 5 columns, got " +
                                   std::to_string(cols.size());
            return result;
        }

        TempNode node{};

        // node_id
        try {
            node.node_id = std::stoll(cols[0]);
        } catch (...) {
            result.error_message = "Row " + std::to_string(lineNo) +
                                   ": cannot parse node_id '" + cols[0] + "'";
            return result;
        }

        // x, y, z, temperature
        const char* fnames[4] = {"x_m","y_m","z_m","temperature_C"};
        float* ptrs[4] = {&node.x, &node.y, &node.z, &node.temperature};
        for (int i = 0; i < 4; ++i) {
            try {
                *ptrs[i] = std::stof(cols[i+1]);
            } catch(...) {
                result.error_message = "Row " + std::to_string(lineNo) +
                                       ": cannot parse " + fnames[i] +
                                       " '" + cols[i+1] + "'";
                return result;
            }
            if (!std::isfinite(*ptrs[i])) {
                result.error_message = "Row " + std::to_string(lineNo) +
                                       ": non-finite value in " + fnames[i] +
                                       " '" + cols[i+1] + "'";
                return result;
            }
        }

        // Update ranges
        tmin = std::min(tmin, node.temperature);
        tmax = std::max(tmax, node.temperature);
        xmin = std::min(xmin, node.x); xmax = std::max(xmax, node.x);
        ymin = std::min(ymin, node.y); ymax = std::max(ymax, node.y);
        zmin = std::min(zmin, node.z); zmax = std::max(zmax, node.z);

        result.nodes.push_back(node);
    }

    result.line_count = lineNo;

    if (result.nodes.empty()) {
        result.error_message = "File has a valid header but no data rows.";
        return result;
    }

    result.temp_min = tmin; result.temp_max = tmax;
    result.xmin = xmin; result.xmax = xmax;
    result.ymin = ymin; result.ymax = ymax;
    result.zmin = zmin; result.zmax = zmax;
    result.success = true;
    return result;
}

std::string csvSummary(const CsvResult& r) {
    if (!r.success) return "CSV Error: " + r.error_message;
    std::ostringstream os;
    os << "CSV OK – " << r.nodes.size() << " nodes  |  "
       << "T [" << r.temp_min << ", " << r.temp_max << "] °C";
    return os.str();
}

} // namespace thermatrix
