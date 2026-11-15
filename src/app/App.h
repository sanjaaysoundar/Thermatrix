#pragma once
// App.h – Top-level application class for THERMATRIX
// Manages all views, file dialogs, and the main ImGui render loop.

#include "../core/StlParser.h"
#include "../core/CsvParser.h"
#include "../core/FemSolver.h"
#include "../core/BackpropSolver.h"
#include "../core/BenchmarkEngine.h"
#include "../core/HardwareMock.h"
#include "../thermal3d/ThermalProblem.h"
#include "../thermal3d/SolverResult.h"
#include "../thermal3d/FemSolver3D.h"
#include "../thermal3d/FdmSolver3D.h"
#include "../thermal3d/FvmSolver3D.h"
#include "../thermal3d/Benchmark3D.h"

#include <string>
#include <memory>
#include <thread>
#include <atomic>
#include <mutex>
#include <functional>

// Forward-declare ImGui types to avoid pulling in all headers here
struct ImGuiIO;
struct GLFWwindow;

namespace thermatrix {

class App {
public:
    App();
    ~App();

    /// Initialize GLFW/OpenGL/ImGui.
    bool init(int width = 1440, int height = 900, const char* title = "THERMATRIX – Engineering Workbench");

    /// Main render loop – blocks until window closed.
    void run();

    /// Cleanup.
    void shutdown();

private:
    // ── render frames ───────────────────────────────────────────────────────
    void renderMainMenu();
    void renderGeometryView();
    void renderThermalView();
    void renderPerformanceView();
    void renderHardwareView();

    // ── background work ──────────────────────────────────────────────────────
    void startFemWork();
    void startBenchWork();

    // ── helpers ──────────────────────────────────────────────────────────────
    void openFileDialog(const std::string& filter,
                        std::function<void(std::string)> onSelect);
    void statusBar(const std::string& msg, bool isError = false);

    // ── state ────────────────────────────────────────────────────────────────
    GLFWwindow* window_{nullptr};
    int         active_tab_{0};  // 0=Geometry, 1=Thermal, 2=Performance, 3=Hardware

    // -- Geometry
    StlResult   stl_result_;
    CsvResult   csv_result_;
    std::string stl_path_;
    std::string csv_path_;
    std::string geo_status_;
    bool        geo_status_error_{false};

    // 3D viewer state
    float       rot_x_{20.0f}, rot_y_{30.0f};
    float       zoom_{1.0f};
    float       pan_x_{0.0f}, pan_y_{0.0f};
    bool        mouse_drag_{false};
    int         geo_mode_{0}; // 0=STL mesh, 1=temp cloud

    // -- Thermal
    FemParams            fem_params_;
    std::vector<FemResult> fem_results_;
    std::string          fem_status_;
    std::atomic<bool>    fem_running_{false};
    std::thread          fem_thread_;

    // -- Performance / Benchmark
    BenchmarkConfig      bench_cfg_;
    BenchmarkResult      bench_result_;
    std::string          bench_status_;
    std::atomic<bool>    bench_running_{false};
    std::thread          bench_thread_;
    std::mutex           bench_mutex_;
    bool                 bench_done_{false};

    // -- Hardware
    hw::HardwareMgr      hw_mgr_;
    std::string          hw_log_;
    bool                 hw_connected_{false};

    // -- Backprop
    BackpropResult       bp_result_;
    bool                 bp_done_{false};
    BackpropParams       bp_params_;

    // -- 3D Thermal Suite (Assignment 2)
    int                  th_mode_{1}; // 0 = 1D Thermal, 1 = 3D Heat Sink (FEM/FDM/FVM)
    int                  t3d_solver_choice_{3}; // 0=FEM, 1=FDM, 2=FVM, 3=Compare All
    int                  t3d_res_choice_{0}; // 0=Coarse, 1=Medium, 2=Fine
    int                  t3d_case_choice_{0}; // 0=Case A (h=10), 1=Case B (h=25)
    t3d::ThermalProblem  t3d_prob_;
    std::vector<t3d::SolverResult> t3d_results_;
    std::string          t3d_status_;
    std::atomic<bool>    t3d_running_{false};
    std::thread          t3d_thread_;
    void                 startT3DWork();

    // -- Status bar
    std::string          status_msg_;
    bool                 status_error_{false};
};

} // namespace thermatrix
