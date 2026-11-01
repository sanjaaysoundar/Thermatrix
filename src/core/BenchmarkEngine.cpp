// BenchmarkEngine.cpp – Serial and parallel thermal FEM benchmark
// THERMATRIX C++17 backend

#include "BenchmarkEngine.h"
#include "FemSolver.h"

#include <chrono>
#include <thread>
#include <mutex>
#include <queue>
#include <algorithm>
#include <numeric>
#include <fstream>
#include <sstream>
#include <cmath>
#include <random>
#include <atomic>
#include <cstdlib>
#include <functional>

// Windows-specific headers for CPU/RAM info
#ifdef _WIN32
#  define WIN32_LEAN_AND_MEAN
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <windows.h>
#  include <sysinfoapi.h>
#endif

namespace thermatrix {

// ─── system info ─────────────────────────────────────────────────────────────

SysInfo collectSysInfo() {
    SysInfo si;
    si.logical_cores  = static_cast<int>(std::thread::hardware_concurrency());
    si.physical_cores = -1; // Not easily portable; left as -1

#ifdef _WIN32
    // CPU name from registry
    {
        HKEY hKey;
        if (RegOpenKeyExA(HKEY_LOCAL_MACHINE,
                          "HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0",
                          0, KEY_READ, &hKey) == ERROR_SUCCESS) {
            char buf[256] = {};
            DWORD size = sizeof(buf);
            RegQueryValueExA(hKey, "ProcessorNameString", nullptr, nullptr,
                             reinterpret_cast<LPBYTE>(buf), &size);
            si.cpu_name = buf;
            RegCloseKey(hKey);
        }
    }
    // RAM
    {
        MEMORYSTATUSEX ms{};
        ms.dwLength = sizeof(ms);
        if (GlobalMemoryStatusEx(&ms)) {
            si.ram_gb = static_cast<double>(ms.ullTotalPhys) / (1024.0*1024.0*1024.0);
        } else {
            si.ram_gb = -1.0;
        }
    }
    // Physical cores via GetLogicalProcessorInformation
    {
        DWORD len = 0;
        GetLogicalProcessorInformation(nullptr, &len);
        std::vector<SYSTEM_LOGICAL_PROCESSOR_INFORMATION> buf(len / sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION));
        if (GetLogicalProcessorInformation(buf.data(), &len)) {
            int phys = 0;
            for (auto& info : buf)
                if (info.Relationship == RelationProcessorCore) ++phys;
            si.physical_cores = phys;
        }
    }
#else
    si.cpu_name = "Unknown (non-Windows)";
    si.ram_gb   = -1.0;
#endif

    si.single_logical_cpu = (si.logical_cores <= 1);

    // Compiler info (compile-time strings)
#if defined(__GNUC__)
    si.compiler = "GCC " + std::to_string(__GNUC__) + "." +
                  std::to_string(__GNUC_MINOR__) + "." +
                  std::to_string(__GNUC_PATCHLEVEL__);
#elif defined(_MSC_VER)
    si.compiler = "MSVC " + std::to_string(_MSC_VER);
#elif defined(__clang__)
    si.compiler = "Clang " + std::to_string(__clang_major__) + "." +
                  std::to_string(__clang_minor__);
#else
    si.compiler = "Unknown";
#endif

    si.opt_flags = "-O2 -std=c++17";
    return si;
}

// ─── case generation ─────────────────────────────────────────────────────────

std::vector<BenchCase> generateCases(int n_cases, int n_elements, std::uint32_t seed) {
    std::mt19937 rng(seed);
    // k in [1, 100], q in [1000, 50000] – all positive
    std::uniform_real_distribution<double> k_dist(1.0, 100.0);
    std::uniform_real_distribution<double> q_dist(1000.0, 50000.0);

    std::vector<BenchCase> cases;
    cases.reserve(n_cases);
    for (int i = 0; i < n_cases; ++i) {
        BenchCase bc;
        bc.n_elements = n_elements;
        bc.k = k_dist(rng);
        bc.q = q_dist(rng);
        cases.push_back(bc);
    }
    return cases;
}

// ─── worker pool ─────────────────────────────────────────────────────────────

static std::vector<double> solveAll(const std::vector<BenchCase>& cases,
                                     int n_workers) {
    int N = static_cast<int>(cases.size());
    std::vector<double> maxTemps(N, 0.0); // store max T at interior nodes

    if (n_workers <= 1) {
        // Serial
        for (int i = 0; i < N; ++i) {
            FemParams p;
            p.n_elements = cases[i].n_elements;
            p.k = cases[i].k;
            p.q = cases[i].q;
            auto r = femSolve(p);
            if (r.success && !r.T_fem.empty()) {
                maxTemps[i] = *std::max_element(r.T_fem.begin(), r.T_fem.end());
            }
        }
    } else {
        // Parallel bounded worker pool
        std::atomic<int> nextIdx{0};
        std::vector<std::thread> threads;
        threads.reserve(n_workers);
        for (int w = 0; w < n_workers; ++w) {
            threads.emplace_back([&]() {
                while (true) {
                    int i = nextIdx.fetch_add(1, std::memory_order_relaxed);
                    if (i >= N) break;
                    FemParams p;
                    p.n_elements = cases[i].n_elements;
                    p.k = cases[i].k;
                    p.q = cases[i].q;
                    auto r = femSolve(p);
                    if (r.success && !r.T_fem.empty()) {
                        maxTemps[i] = *std::max_element(r.T_fem.begin(), r.T_fem.end());
                    }
                }
            });
        }
        for (auto& t : threads) t.join();
    }
    return maxTemps;
}

// ─── timing utility ──────────────────────────────────────────────────────────

using Clock = std::chrono::steady_clock;
using Ms    = std::chrono::duration<double, std::milli>;

static double timeMs(const std::function<void()>& fn) {
    auto t0 = Clock::now();
    fn();
    auto t1 = Clock::now();
    return Ms(t1 - t0).count();
}

// ─── benchmark run ───────────────────────────────────────────────────────────

BenchmarkResult runBenchmark(const BenchmarkConfig& cfg) {
    BenchmarkResult res;
    auto t_launch = Clock::now();

    res.sysinfo  = collectSysInfo();
    res.n_cases  = cfg.n_cases;
    res.n_elements_per_case = cfg.n_elements;
    res.seed     = cfg.seed;

    int maxW = std::min(cfg.max_workers,
                        std::max(1, res.sysinfo.logical_cores));

    auto cases = generateCases(cfg.n_cases, cfg.n_elements, cfg.seed);

    auto t_ready = Clock::now();
    res.launch_to_ready_ms = Ms(t_ready - t_launch).count();

    // For verification: serial result
    auto serial_result = solveAll(cases, 1);

    // Test each worker count
    // worker_counts = {1, 2, 3, 4} capped at maxW
    std::vector<int> wCounts;
    for (int w = 1; w <= maxW; ++w) wCounts.push_back(w);

    // For each worker count collect reps
    std::vector<double> t1Reps; // serial repetitions for speedup base

    for (int wi = 0; wi < static_cast<int>(wCounts.size()); ++wi) {
        int nw = wCounts[wi];
        std::vector<double> times;
        times.reserve(cfg.n_repeats);

        // Warm-up (not counted)
        for (int r = 0; r < cfg.n_warmup; ++r) {
            solveAll(cases, nw);
        }

        // Measured reps
        for (int r = 0; r < cfg.n_repeats; ++r) {
            double t = timeMs([&](){ solveAll(cases, nw); });
            times.push_back(t);
            BenchRepeat br;
            br.n_workers  = nw;
            br.rep        = r + 1;
            br.compute_ms = t;
            res.raw_repeats.push_back(br);
        }

        // Median
        std::sort(times.begin(), times.end());
        double median = times[times.size()/2];
        if (nw == 1) t1Reps = times;

        // Verify agreement with serial
        auto par_result = solveAll(cases, nw);
        double maxDiff = 0.0;
        for (int i = 0; i < static_cast<int>(cases.size()); ++i) {
            maxDiff = std::max(maxDiff, std::abs(par_result[i] - serial_result[i]));
        }

        double t1_med;
        if (!t1Reps.empty()) {
            auto t1sorted = t1Reps;
            std::sort(t1sorted.begin(), t1sorted.end());
            t1_med = t1sorted[t1sorted.size()/2];
        } else {
            t1_med = median;
        }

        BenchSummary bs;
        bs.n_workers   = nw;
        bs.median_ms   = median;
        bs.speedup     = (nw == 1) ? 1.0 : (t1_med / median);
        bs.efficiency  = bs.speedup / nw;
        bs.max_abs_diff = maxDiff;
        bs.agreement   = (maxDiff <= 1e-8);
        res.summaries.push_back(bs);
    }

    res.success = true;
    return res;
}

// ─── CSV export ──────────────────────────────────────────────────────────────

bool benchmarkExportCsv(const BenchmarkResult& r, const std::string& filepath) {
    std::ofstream ofs(filepath);
    if (!ofs.is_open()) return false;
    ofs << "n_workers,rep,compute_ms\n";
    for (auto& rp : r.raw_repeats) {
        ofs << rp.n_workers << "," << rp.rep << "," << rp.compute_ms << "\n";
    }
    ofs << "\nn_workers,median_ms,speedup,efficiency,max_abs_diff_C,agreement\n";
    for (auto& s : r.summaries) {
        ofs << s.n_workers << "," << s.median_ms << "," << s.speedup << ","
            << s.efficiency << "," << s.max_abs_diff << ","
            << (s.agreement ? "PASS" : "FAIL") << "\n";
    }
    return true;
}

} // namespace thermatrix
