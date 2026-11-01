// test_csv_parser.cpp – Unit tests for temperature CSV parser
// THERMATRIX C++17

#include "../src/core/CsvParser.h"
#include <cassert>
#include <iostream>
#include <fstream>
#include <cmath>

using namespace thermatrix;

static int passed = 0, failed = 0;

#define ASSERT_TRUE(cond, msg) \
    do { if (cond) { ++passed; std::cout << "[PASS] " << msg << "\n"; } \
         else { ++failed; std::cout << "[FAIL] " << msg << "\n"; } } while(0)

#define ASSERT_NEAR(a, b, tol, msg) \
    ASSERT_TRUE(std::abs((a)-(b)) <= (tol), msg)

static void write(const std::string& fname, const std::string& content) {
    std::ofstream f(fname);
    f << content;
}

// ─── Valid CSV ────────────────────────────────────────────────────────────────

void test_valid_csv() {
    write("tmp_test.csv",
          "node_id,x_m,y_m,z_m,temperature_C\n"
          "1,0.0,0.0,0.0,100.0\n"
          "2,0.5,0.0,0.0,150.0\n"
          "3,1.0,0.0,0.0,50.0\n");
    auto r = parseCsvFile("tmp_test.csv");
    ASSERT_TRUE(r.success,              "Valid CSV: parse succeeds");
    ASSERT_TRUE(r.nodes.size() == 3,    "Valid CSV: 3 nodes");
    ASSERT_NEAR(r.temp_min, 50.0f, 1e-4,"Valid CSV: temp_min=50");
    ASSERT_NEAR(r.temp_max, 150.0f,1e-4,"Valid CSV: temp_max=150");
    ASSERT_TRUE(r.nodes[0].node_id == 1,"Valid CSV: node_id[0]=1");
    ASSERT_NEAR(r.nodes[1].x, 0.5f, 1e-4,"Valid CSV: x[1]=0.5");
    std::remove("tmp_test.csv");
}

// ─── Empty file ───────────────────────────────────────────────────────────────

void test_empty_file() {
    write("tmp_empty.csv", "");
    auto r = parseCsvFile("tmp_empty.csv");
    ASSERT_TRUE(!r.success, "Empty CSV: parse fails");
    ASSERT_TRUE(!r.error_message.empty(), "Empty CSV: error message");
    std::remove("tmp_empty.csv");
}

// ─── Header only ─────────────────────────────────────────────────────────────

void test_header_only() {
    write("tmp_hdr.csv", "node_id,x_m,y_m,z_m,temperature_C\n");
    auto r = parseCsvFile("tmp_hdr.csv");
    ASSERT_TRUE(!r.success, "Header-only CSV: parse fails (no data rows)");
    ASSERT_TRUE(!r.error_message.empty(), "Header-only: error message");
    std::remove("tmp_hdr.csv");
}

// ─── Wrong header ────────────────────────────────────────────────────────────

void test_wrong_header() {
    write("tmp_whdr.csv", "id,x,y,z,temp\n1,0,0,0,100\n");
    auto r = parseCsvFile("tmp_whdr.csv");
    ASSERT_TRUE(!r.success, "Wrong header: parse fails");
    std::remove("tmp_whdr.csv");
}

// ─── File not found ───────────────────────────────────────────────────────────

void test_file_not_found() {
    auto r = parseCsvFile("nonexistent_xyz_abc.csv");
    ASSERT_TRUE(!r.success, "Nonexistent CSV: parse fails");
}

// ─── Non-finite temperature rejected ─────────────────────────────────────────

void test_non_finite_rejected() {
    write("tmp_nan.csv",
          "node_id,x_m,y_m,z_m,temperature_C\n"
          "1,0.0,0.0,0.0,nan\n");
    auto r = parseCsvFile("tmp_nan.csv");
    ASSERT_TRUE(!r.success, "NaN temperature: parse fails");
    std::remove("tmp_nan.csv");
}

// ─── Wrong column count ───────────────────────────────────────────────────────

void test_wrong_column_count() {
    write("tmp_cols.csv",
          "node_id,x_m,y_m,z_m,temperature_C\n"
          "1,0.0,0.0,100.0\n");  // 4 fields instead of 5
    auto r = parseCsvFile("tmp_cols.csv");
    ASSERT_TRUE(!r.success, "Wrong column count: parse fails");
    std::remove("tmp_cols.csv");
}

// ─── Invalid node_id ──────────────────────────────────────────────────────────

void test_invalid_node_id() {
    write("tmp_nid.csv",
          "node_id,x_m,y_m,z_m,temperature_C\n"
          "abc,0.0,0.0,0.0,100.0\n");
    auto r = parseCsvFile("tmp_nid.csv");
    ASSERT_TRUE(!r.success, "Invalid node_id: parse fails");
    std::remove("tmp_nid.csv");
}

// ─── Temperature range correct ────────────────────────────────────────────────

void test_temperature_range() {
    write("tmp_range.csv",
          "node_id,x_m,y_m,z_m,temperature_C\n"
          "1,0,0,0,-50.0\n"
          "2,1,0,0,200.5\n"
          "3,0,1,0,75.0\n");
    auto r = parseCsvFile("tmp_range.csv");
    ASSERT_TRUE(r.success, "Temperature range CSV: parse succeeds");
    ASSERT_NEAR(r.temp_min, -50.0f, 1e-3, "temp_min=-50");
    ASSERT_NEAR(r.temp_max, 200.5f, 1e-3, "temp_max=200.5");
    std::remove("tmp_range.csv");
}

// ─── Case-insensitive header ──────────────────────────────────────────────────

void test_case_insensitive_header() {
    write("tmp_ci.csv",
          "Node_ID,X_M,Y_M,Z_M,Temperature_C\n"
          "1,0.0,0.0,0.0,100.0\n");
    auto r = parseCsvFile("tmp_ci.csv");
    ASSERT_TRUE(r.success, "Case-insensitive header: parse succeeds");
    std::remove("tmp_ci.csv");
}

int main() {
    std::cout << "=== CSV Parser Tests ===\n";
    test_valid_csv();
    test_empty_file();
    test_header_only();
    test_wrong_header();
    test_file_not_found();
    test_non_finite_rejected();
    test_wrong_column_count();
    test_invalid_node_id();
    test_temperature_range();
    test_case_insensitive_header();
    std::cout << "\n" << passed << " passed, " << failed << " failed.\n";
    return (failed > 0) ? 1 : 0;
}
