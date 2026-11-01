// test_stl_parser.cpp – Unit tests for ASCII STL parser
// THERMATRIX C++17

#include "../src/core/StlParser.h"
#include <cassert>
#include <iostream>
#include <fstream>
#include <cmath>
#include <cstdint>

using namespace thermatrix;

static int passed = 0, failed = 0;

#define ASSERT_TRUE(cond, msg) \
    do { if (cond) { ++passed; std::cout << "[PASS] " << msg << "\n"; } \
         else { ++failed; std::cout << "[FAIL] " << msg << "\n"; } } while(0)

// Write a temp STL file and return its path
static std::string writeTempStl(const std::string& content, const std::string& fname) {
    std::ofstream f(fname);
    f << content;
    return fname;
}

// ─── Valid triangle ───────────────────────────────────────────────────────────

void test_valid_single_triangle() {
    std::string stl =
        "solid test\n"
        "  facet normal 0 0 1\n"
        "    outer loop\n"
        "      vertex 0 0 0\n"
        "      vertex 1 0 0\n"
        "      vertex 0 1 0\n"
        "    endloop\n"
        "  endfacet\n"
        "endsolid test\n";
    auto f = writeTempStl(stl, "tmp_test_tri.stl");
    auto r = parseStlFile(f);
    ASSERT_TRUE(r.success,            "Valid triangle: parse succeeds");
    ASSERT_TRUE(r.triangle_count == 1,"Valid triangle: count=1");
    ASSERT_TRUE(r.triangles[0].v0[0] == 0.0f, "v0.x=0");
    ASSERT_TRUE(r.triangles[0].v1[0] == 1.0f, "v1.x=1");
    std::remove(f.c_str());
}

// ─── Empty file ───────────────────────────────────────────────────────────────

void test_empty_file() {
    writeTempStl("", "tmp_empty.stl");
    auto r = parseStlFile("tmp_empty.stl");
    ASSERT_TRUE(!r.success, "Empty file: parse fails");
    ASSERT_TRUE(!r.error_message.empty(), "Empty file: error message set");
    std::remove("tmp_empty.stl");
}

// ─── File not found ───────────────────────────────────────────────────────────

void test_file_not_found() {
    auto r = parseStlFile("nonexistent_file_xyz.stl");
    ASSERT_TRUE(!r.success, "Nonexistent file: parse fails");
    ASSERT_TRUE(!r.error_message.empty(), "Nonexistent: error message set");
}

// ─── Binary STL (not ASCII) ───────────────────────────────────────────────────

void test_binary_stl_rejected() {
    // Binary STL starts with 80-byte header, then triangle count, NOT "solid"
    std::ofstream f("tmp_binary.stl", std::ios::binary);
    char header[80] = {};
    std::fill(header, header+80, 'X'); // doesn't start with 'solid'
    f.write(header, 80);
    uint32_t triCount = 1;
    f.write(reinterpret_cast<char*>(&triCount), 4);
    // Write one triangle (50 bytes)
    float data[12] = {0,0,1, 0,0,0, 1,0,0, 0,1,0};
    f.write(reinterpret_cast<char*>(data), 48);
    uint16_t attr = 0;
    f.write(reinterpret_cast<char*>(&attr), 2);
    f.close();
    auto r = parseStlFile("tmp_binary.stl");
    ASSERT_TRUE(!r.success, "Binary STL: rejected");
    std::remove("tmp_binary.stl");
}

// ─── Malformed: wrong vertex count ───────────────────────────────────────────

void test_malformed_vertex_count() {
    std::string stl =
        "solid test\n"
        "  facet normal 0 0 1\n"
        "    outer loop\n"
        "      vertex 0 0 0\n"
        "      vertex 1 0 0\n"
        "    endloop\n"  // Only 2 vertices
        "  endfacet\n"
        "endsolid test\n";
    auto f = writeTempStl(stl, "tmp_bad_count.stl");
    auto r = parseStlFile(f);
    ASSERT_TRUE(!r.success, "Wrong vertex count: parse fails");
    std::remove(f.c_str());
}

// ─── Non-finite coordinates rejected ─────────────────────────────────────────

void test_non_finite_rejected() {
    std::string stl =
        "solid test\n"
        "  facet normal 0 0 1\n"
        "    outer loop\n"
        "      vertex nan 0 0\n"
        "      vertex 1 0 0\n"
        "      vertex 0 1 0\n"
        "    endloop\n"
        "  endfacet\n"
        "endsolid test\n";
    auto f = writeTempStl(stl, "tmp_nan.stl");
    auto r = parseStlFile(f);
    ASSERT_TRUE(!r.success, "NaN coordinate: parse fails");
    std::remove(f.c_str());
}

// ─── Multiple triangles + bounding box ───────────────────────────────────────

void test_multiple_triangles_bbox() {
    std::string stl =
        "solid cube\n"
        "  facet normal 0 0 1\n"
        "    outer loop\n"
        "      vertex 0 0 1\n"
        "      vertex 1 0 1\n"
        "      vertex 0 1 1\n"
        "    endloop\n"
        "  endfacet\n"
        "  facet normal 0 0 -1\n"
        "    outer loop\n"
        "      vertex 0 0 0\n"
        "      vertex 0 1 0\n"
        "      vertex 1 0 0\n"
        "    endloop\n"
        "  endfacet\n"
        "endsolid cube\n";
    auto f = writeTempStl(stl, "tmp_multi.stl");
    auto r = parseStlFile(f);
    ASSERT_TRUE(r.success,            "Multi-triangle: parse succeeds");
    ASSERT_TRUE(r.triangle_count == 2,"Multi-triangle: count=2");
    ASSERT_TRUE(r.bbox_min[2] == 0.0f,"BBox zmin=0");
    ASSERT_TRUE(r.bbox_max[2] == 1.0f,"BBox zmax=1");
    std::remove(f.c_str());
}

int main() {
    std::cout << "=== STL Parser Tests ===\n";
    test_valid_single_triangle();
    test_empty_file();
    test_file_not_found();
    test_binary_stl_rejected();
    test_malformed_vertex_count();
    test_non_finite_rejected();
    test_multiple_triangles_bbox();
    std::cout << "\n" << passed << " passed, " << failed << " failed.\n";
    return (failed > 0) ? 1 : 0;
}
