// test_benchmark.cpp – Unit tests for the benchmark engine
// THERMATRIX C++17

#include "../src/core/BenchmarkEngine.h"
#include <cassert>
#include <iostream>
#include <cmath>
#include <algorithm>

using namespace thermatrix;

static int passed = 0, failed = 0;

#define ASSERT_TRUE(cond, msg) \
    do { if (cond) { ++passed; std::cout << "[PASS] " << msg << "\n"; } \
         else { ++failed; std::cout << "[FAIL] " << msg << "\n"; } } while(0)

#define ASSERT_NEAR(a, b, tol, msg) \
    ASSERT_TRUE(std::abs((a)-(b)) <= (tol), msg)

// ─── Case generation reproducibility ─────────────────────────────────────────

void test_reproducibility() {
    auto c1 = generateCases(100, 40, 42u);
    auto c2 = generateCases(100, 40, 42u);
    ASSERT_TRUE(c1.size() == c2.size(), "Reproducible: same size");
    bool allMatch = true;
    for (size_t i = 0; i < c1.size(); ++i) {
        if (std::abs(c1[i].k - c2[i].k) > 1e-12 ||
            std::abs(c1[i].q - c2[i].q) > 1e-12) {
            allMatch = false;
            break;
        }
    }
    ASSERT_TRUE(allMatch, "Reproducible: same cases with same seed");
}

// ─── Different seeds give different cases ─────────────────────────────────────

void test_seed_sensitivity() {
    auto c1 = generateCases(100, 40, 42u);
    auto c2 = generateCases(100, 40, 99u);
    bool anyDiff = false;
    for (size_t i = 0; i < c1.size(); ++i) {
        if (std::abs(c1[i].k - c2[i].k) > 1e-6) { anyDiff=true; break; }
    }
    ASSERT_TRUE(anyDiff, "Different seeds produce different cases");
}

// ─── k and q are positive ────────────────────────────────────────────────────

void test_positive_params() {
    auto cases = generateCases(200, 40, 7u);
    bool allPos = true;
    for (auto& c : cases) {
        if (c.k <= 0.0 || c.q <= 0.0) { allPos=false; break; }
    }
    ASSERT_TRUE(allPos, "All generated k and q are positive");
}

// ─── Serial/parallel agreement ────────────────────────────────────────────────

void test_serial_parallel_agreement() {
    BenchmarkConfig cfg;
    cfg.n_cases    = 20;
    cfg.n_elements = 10;
    cfg.max_workers= 2;
    cfg.n_repeats  = 1;
    cfg.n_warmup   = 0;
    auto r = runBenchmark(cfg);
    ASSERT_TRUE(r.success, "Benchmark runs successfully");
    for (auto& s : r.summaries) {
        ASSERT_TRUE(s.agreement,
                    "Workers=" << s.n_workers << ": serial/parallel agreement (max_diff <= 1e-8°C)");
        ASSERT_NEAR(s.max_abs_diff, 0.0, 1e-8,
                    "Workers=" << s.n_workers << ": max_abs_diff <= 1e-8");
    }
}

// ─── Summaries contain correct worker counts ──────────────────────────────────

void test_worker_counts() {
    BenchmarkConfig cfg;
    cfg.n_cases    = 10;
    cfg.n_elements = 5;
    cfg.max_workers= 3;
    cfg.n_repeats  = 1;
    cfg.n_warmup   = 0;
    auto r = runBenchmark(cfg);
    int maxW = std::min(3, r.sysinfo.logical_cores);
    ASSERT_TRUE((int)r.summaries.size() == maxW,
                "Correct number of summaries (capped at logical CPUs)");
    for (int i = 0; i < (int)r.summaries.size(); ++i) {
        ASSERT_TRUE(r.summaries[i].n_workers == i+1,
                    "Summary[" << i << "] n_workers=" << i+1);
    }
}

// ─── Speedup for n_workers=1 is exactly 1.0 ──────────────────────────────────

void test_serial_speedup_is_one() {
    BenchmarkConfig cfg;
    cfg.n_cases = 10; cfg.n_elements = 5;
    cfg.max_workers = 1; cfg.n_repeats = 1; cfg.n_warmup = 0;
    auto r = runBenchmark(cfg);
    if (!r.summaries.empty()) {
        ASSERT_NEAR(r.summaries[0].speedup, 1.0, 1e-9, "n_workers=1: speedup=1.0");
        ASSERT_NEAR(r.summaries[0].efficiency, 1.0, 1e-9, "n_workers=1: efficiency=1.0");
    } else {
        std::cout << "[SKIP] No summaries (0 logical CPUs?)\n";
    }
}

// ─── System info populated ────────────────────────────────────────────────────

void test_sysinfo() {
    auto si = collectSysInfo();
    ASSERT_TRUE(si.logical_cores > 0, "logical_cores > 0");
    ASSERT_TRUE(!si.compiler.empty(), "Compiler string non-empty");
}

int main() {
    std::cout << "=== Benchmark Engine Tests ===\n";
    test_reproducibility();
    test_seed_sensitivity();
    test_positive_params();
    test_serial_parallel_agreement();
    test_worker_counts();
    test_serial_speedup_is_one();
    test_sysinfo();
    std::cout << "\n" << passed << " passed, " << failed << " failed.\n";
    return (failed > 0) ? 1 : 0;
}
