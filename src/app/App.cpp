// App.cpp – THERMATRIX main application
// Frontend implemented with Dear ImGui + ImPlot + OpenGL/GLFW
// All numerical computation delegated to C++ backend (core/)

#include "App.h"

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "implot.h"

#define GL_SILENCE_DEPRECATION
#if defined(_WIN32)
#  include <windows.h>
#  include <commdlg.h>
#endif
#include <GLFW/glfw3.h>

#include <cmath>
#include <cstring>
#include <algorithm>
#include <numeric>
#include <sstream>
#include <iomanip>
#include <fstream>
#include <filesystem>

namespace thermatrix {

namespace fs = std::filesystem;

static bool DragDouble(const char* label, double* v, float speed = 1.0f, double min = 0.0, double max = 0.0, const char* format = "%.4f") {
    return ImGui::DragScalar(label, ImGuiDataType_Double, v, speed, &min, &max, format);
}

// ─── colour map (blue → red) ─────────────────────────────────────────────────

static ImVec4 tempColor(float t, float tmin, float tmax) {
    float tt = (tmax > tmin) ? (t - tmin) / (tmax - tmin) : 0.5f;
    tt = std::max(0.f, std::min(1.f, tt));
    // Blue(cold) → Cyan → Green → Yellow → Red(hot)
    float r, g, b;
    if (tt < 0.25f) {
        float u = tt / 0.25f;
        r = 0.f; g = u; b = 1.f;
    } else if (tt < 0.5f) {
        float u = (tt - 0.25f) / 0.25f;
        r = 0.f; g = 1.f; b = 1.f - u;
    } else if (tt < 0.75f) {
        float u = (tt - 0.5f) / 0.25f;
        r = u; g = 1.f; b = 0.f;
    } else {
        float u = (tt - 0.75f) / 0.25f;
        r = 1.f; g = 1.f - u; b = 0.f;
    }
    return {r, g, b, 1.0f};
}

// ─── constructor / destructor ─────────────────────────────────────────────────

App::App() {
    fem_params_  = FemParams{};
    bench_cfg_   = BenchmarkConfig{};
    bp_params_   = BackpropParams{};
}

App::~App() {
    shutdown();
}

// ─── init ─────────────────────────────────────────────────────────────────────

bool App::init(int w, int h, const char* title) {
    if (!glfwInit()) return false;
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_COMPAT_PROFILE);

    window_ = glfwCreateWindow(w, h, title, nullptr, nullptr);
    if (!window_) { glfwTerminate(); return false; }

    glfwMakeContextCurrent(window_);
    glfwSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImPlot::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.IniFilename = nullptr; // Don't write imgui.ini

    // ── Style ──────────────────────────────────────────────────────────────
    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding   = 8.0f;
    style.FrameRounding    = 4.0f;
    style.GrabRounding     = 4.0f;
    style.TabRounding      = 4.0f;
    style.FramePadding     = {8.0f, 4.0f};
    style.ItemSpacing      = {8.0f, 6.0f};
    style.WindowPadding    = {12.0f, 12.0f};
    style.ScrollbarSize    = 12.0f;

    // Custom dark-blue palette
    auto& c = style.Colors;
    c[ImGuiCol_WindowBg]       = {0.08f, 0.09f, 0.12f, 1.0f};
    c[ImGuiCol_ChildBg]        = {0.10f, 0.11f, 0.15f, 1.0f};
    c[ImGuiCol_PopupBg]        = {0.10f, 0.11f, 0.15f, 0.95f};
    c[ImGuiCol_Header]         = {0.20f, 0.35f, 0.55f, 0.80f};
    c[ImGuiCol_HeaderHovered]  = {0.26f, 0.46f, 0.70f, 0.80f};
    c[ImGuiCol_HeaderActive]   = {0.26f, 0.46f, 0.70f, 1.00f};
    c[ImGuiCol_Button]         = {0.20f, 0.35f, 0.58f, 0.80f};
    c[ImGuiCol_ButtonHovered]  = {0.26f, 0.46f, 0.72f, 1.00f};
    c[ImGuiCol_ButtonActive]   = {0.14f, 0.24f, 0.45f, 1.00f};
    c[ImGuiCol_FrameBg]        = {0.14f, 0.16f, 0.22f, 1.00f};
    c[ImGuiCol_FrameBgHovered] = {0.20f, 0.24f, 0.32f, 1.00f};
    c[ImGuiCol_FrameBgActive]  = {0.24f, 0.29f, 0.40f, 1.00f};
    c[ImGuiCol_Tab]            = {0.14f, 0.16f, 0.22f, 1.00f};
    c[ImGuiCol_TabHovered]     = {0.26f, 0.46f, 0.72f, 0.80f};
    c[ImGuiCol_TabSelected]    = {0.20f, 0.35f, 0.60f, 1.00f};
    c[ImGuiCol_TitleBg]        = {0.08f, 0.09f, 0.12f, 1.00f};
    c[ImGuiCol_TitleBgActive]  = {0.12f, 0.22f, 0.38f, 1.00f};
    c[ImGuiCol_MenuBarBg]      = {0.06f, 0.07f, 0.10f, 1.00f};
    c[ImGuiCol_CheckMark]      = {0.30f, 0.65f, 1.00f, 1.00f};
    c[ImGuiCol_SliderGrab]     = {0.30f, 0.60f, 0.90f, 1.00f};
    c[ImGuiCol_PlotLines]      = {0.30f, 0.65f, 1.00f, 1.00f};
    c[ImGuiCol_PlotHistogram]  = {0.26f, 0.50f, 0.82f, 1.00f};
    c[ImGuiCol_Separator]      = {0.28f, 0.30f, 0.40f, 1.00f};
    c[ImGuiCol_Text]           = {0.92f, 0.94f, 0.98f, 1.00f};
    c[ImGuiCol_TextDisabled]   = {0.45f, 0.48f, 0.60f, 1.00f};
    c[ImGuiCol_Border]         = {0.24f, 0.28f, 0.40f, 1.00f};
    c[ImGuiCol_BorderShadow]   = {0.00f, 0.00f, 0.00f, 0.00f};

    // Font
    io.Fonts->AddFontDefault();

    ImGui_ImplGlfw_InitForOpenGL(window_, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    // Pre-compute backprop result
    bp_result_ = backpropRun(bp_params_);
    bp_done_   = true;

    // Auto-load sample STL if present
    std::string defaultStl = "data/example.stl";
    if (!fs::exists(defaultStl)) defaultStl = "../data/example.stl";
    if (fs::exists(defaultStl)) {
        stl_path_ = defaultStl;
        stl_result_ = parseStlFile(defaultStl);
    }

    // Auto-load sample CSV if present
    std::string defaultCsv = "data/example_temperatures.csv";
    if (!fs::exists(defaultCsv)) defaultCsv = "../data/example_temperatures.csv";
    if (fs::exists(defaultCsv)) {
        csv_path_ = defaultCsv;
        csv_result_ = parseCsvFile(defaultCsv);
    }

    return true;
}

// ─── shutdown ─────────────────────────────────────────────────────────────────

void App::shutdown() {
    if (fem_thread_.joinable()) fem_thread_.join();
    if (bench_thread_.joinable()) bench_thread_.join();
    if (t3d_thread_.joinable()) t3d_thread_.join();

    if (window_) {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImPlot::DestroyContext();
        ImGui::DestroyContext();
        glfwDestroyWindow(window_);
        glfwTerminate();
        window_ = nullptr;
    }
}

// ─── helpers ──────────────────────────────────────────────────────────────────

void App::statusBar(const std::string& msg, bool isError) {
    status_msg_   = msg;
    status_error_ = isError;
}

// ─── file dialog (Windows) ────────────────────────────────────────────────────

void App::openFileDialog(const std::string& filter,
                          std::function<void(std::string)> onSelect) {
#ifdef _WIN32
    OPENFILENAMEA ofn{};
    char szFile[MAX_PATH] = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner   = nullptr;
    ofn.lpstrFile   = szFile;
    ofn.nMaxFile    = MAX_PATH;
    ofn.lpstrFilter = filter.c_str();
    ofn.nFilterIndex= 1;
    ofn.Flags       = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;
    if (GetOpenFileNameA(&ofn)) {
        onSelect(std::string(szFile));
    }
#else
    // Fallback: use a text input field (handled in UI)
    (void)filter; (void)onSelect;
#endif
}

// ─── main run loop ────────────────────────────────────────────────────────────

void App::run() {
    // Set default window size for all child windows
    (void)ImGui::GetIO();

    while (!glfwWindowShouldClose(window_)) {
        glfwPollEvents();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // ── Full-screen dockspace ──────────────────────────────────────────
        ImGui::SetNextWindowPos({0, 0});
        int displayW, displayH;
        glfwGetFramebufferSize(window_, &displayW, &displayH);
        ImGui::SetNextWindowSize({(float)displayW, (float)displayH});
        ImGui::SetNextWindowBgAlpha(1.0f);
        ImGuiWindowFlags mainFlags =
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus |
            ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoScrollbar |
            ImGuiWindowFlags_MenuBar;

        ImGui::Begin("##THERMATRIX_MAIN", nullptr, mainFlags);

        // ── Menu bar ──────────────────────────────────────────────────────
        renderMainMenu();

        // ── Tab bar ───────────────────────────────────────────────────────
        if (ImGui::BeginTabBar("##tabs")) {
            if (ImGui::BeginTabItem("  Geometry  ")) {
                active_tab_ = 0;
                renderGeometryView();
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("  Thermal Analysis  ")) {
                active_tab_ = 1;
                renderThermalView();
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("  Performance  ")) {
                active_tab_ = 2;
                renderPerformanceView();
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("  Hardware  ")) {
                active_tab_ = 3;
                renderHardwareView();
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }

        // ── Status bar ────────────────────────────────────────────────────
        float sbH = 24.0f;
        ImGui::SetCursorPosY(ImGui::GetWindowHeight() - sbH - 4.0f);
        ImGui::Separator();
        ImGui::SetCursorPosY(ImGui::GetWindowHeight() - sbH);
        if (status_error_)
            ImGui::TextColored({1.f, 0.4f, 0.4f, 1.f}, "  %s", status_msg_.c_str());
        else
            ImGui::TextColored({0.6f, 0.9f, 0.6f, 1.f}, "  %s", status_msg_.c_str());

        ImGui::End();

        // ── Render ────────────────────────────────────────────────────────
        ImGui::Render();
        glViewport(0, 0, displayW, displayH);
        glClearColor(0.06f, 0.07f, 0.10f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window_);
    }
}

// ─── menu bar ─────────────────────────────────────────────────────────────────

void App::renderMainMenu() {
    if (ImGui::BeginMenuBar()) {
        ImGui::TextColored({0.35f, 0.70f, 1.00f, 1.0f}, "THERMATRIX");
        ImGui::SameLine();
        ImGui::TextDisabled("Engineering Workbench");

        ImGui::SameLine(ImGui::GetWindowWidth() - 200.0f);
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("Open STL...")) {
                openFileDialog("STL Files (*.stl)\0*.stl\0All Files (*.*)\0*.*\0",
                    [this](std::string p) {
                        stl_path_   = p;
                        stl_result_ = parseStlFile(p);
                        geo_status_ = stlSummary(stl_result_);
                        geo_status_error_ = !stl_result_.success;
                        statusBar(geo_status_, !stl_result_.success);
                    });
            }
            if (ImGui::MenuItem("Open Temperature CSV...")) {
                openFileDialog("CSV Files (*.csv)\0*.csv\0All Files (*.*)\0*.*\0",
                    [this](std::string p) {
                        csv_path_   = p;
                        csv_result_ = parseCsvFile(p);
                        geo_status_ = csvSummary(csv_result_);
                        geo_status_error_ = !csv_result_.success;
                        statusBar(geo_status_, !csv_result_.success);
                    });
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Exit")) {
                glfwSetWindowShouldClose(window_, GLFW_TRUE);
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Help")) {
            ImGui::TextDisabled("THERMATRIX v1.0  |  C++17 + ImGui + ImPlot");
            ImGui::TextDisabled("See README.md and docs/report.md");
            ImGui::EndMenu();
        }
        ImGui::EndMenuBar();
    }
}

// ─── geometry view ────────────────────────────────────────────────────────────

void App::renderGeometryView() {
    float panelW = ImGui::GetContentRegionAvail().x;
    float panelH = ImGui::GetContentRegionAvail().y - 28.0f;

    // Left control panel
    ImGui::BeginChild("##geo_ctrl", {220.0f, panelH}, true);
    ImGui::TextColored({0.4f, 0.8f, 1.0f, 1.0f}, "Geometry Controls");
    ImGui::Separator();

    ImGui::RadioButton("STL Mesh",    &geo_mode_, 0);
    ImGui::RadioButton("Temp Cloud",  &geo_mode_, 1);
    ImGui::Separator();

    if (ImGui::Button("Load Sample STL", {-1, 0})) {
        std::string p = "data/example.stl";
        if (!fs::exists(p)) p = "../data/example.stl";
        if (fs::exists(p)) {
            stl_path_   = p;
            stl_result_ = parseStlFile(p);
            statusBar(stlSummary(stl_result_), !stl_result_.success);
        }
    }
    if (ImGui::Button("Browse STL File...", {-1, 0})) {
        openFileDialog("STL Files (*.stl)\0*.stl\0All Files (*.*)\0*.*\0",
            [this](std::string p) {
                stl_path_   = p;
                stl_result_ = parseStlFile(p);
                statusBar(stlSummary(stl_result_), !stl_result_.success);
            });
    }
    if (ImGui::Button("Load Heat Sink STL", {-1, 0})) {
        std::string hs = "data/heatsink.stl";
        if (!fs::exists(hs)) hs = "../data/heatsink.stl";
        if (fs::exists(hs)) {
            stl_path_ = hs;
            stl_result_ = parseStlFile(hs);
            statusBar("Loaded Aluminium Heat Sink STL.", !stl_result_.success);
        } else {
            t3d::HeatSinkGeometry geom;
            fs::create_directories("data");
            geom.export_stl("data/heatsink.stl");
            stl_path_ = "data/heatsink.stl";
            stl_result_ = parseStlFile(stl_path_);
            statusBar("Generated and loaded Aluminium Heat Sink STL.");
        }
    }
    ImGui::TextDisabled("%s", stl_path_.empty() ? "(no file)" :
                        fs::path(stl_path_).filename().string().c_str());

    ImGui::Spacing();
    if (ImGui::Button("Load Sample CSV", {-1, 0})) {
        std::string p = "data/example_temperatures.csv";
        if (!fs::exists(p)) p = "../data/example_temperatures.csv";
        if (fs::exists(p)) {
            csv_path_   = p;
            csv_result_ = parseCsvFile(p);
            statusBar(csvSummary(csv_result_), !csv_result_.success);
        }
    }
    if (ImGui::Button("Browse CSV File...", {-1, 0})) {
        openFileDialog("CSV Files (*.csv)\0*.csv\0All Files (*.*)\0*.*\0",
            [this](std::string p) {
                csv_path_   = p;
                csv_result_ = parseCsvFile(p);
                statusBar(csvSummary(csv_result_), !csv_result_.success);
            });
    }
    ImGui::TextDisabled("%s", csv_path_.empty() ? "(no file)" :
                        fs::path(csv_path_).filename().string().c_str());

    ImGui::Separator();
    ImGui::TextColored({0.4f, 0.8f, 1.0f, 1.0f}, "Camera");
    ImGui::SliderFloat("RotX",  &rot_x_, -180.0f, 180.0f);
    ImGui::SliderFloat("RotY",  &rot_y_, -180.0f, 180.0f);
    ImGui::SliderFloat("Zoom",  &zoom_,   0.1f,   5.0f);
    ImGui::SliderFloat("PanX",  &pan_x_, -2.0f,   2.0f);
    ImGui::SliderFloat("PanY",  &pan_y_, -2.0f,   2.0f);
    if (ImGui::Button("Fit to View", {-1, 0})) {
        rot_x_ = 20.0f; rot_y_ = 30.0f;
        zoom_  = 1.0f;  pan_x_ = 0.0f; pan_y_ = 0.0f;
    }

    ImGui::Separator();
    // Stats
    if (geo_mode_ == 0 && stl_result_.success) {
        ImGui::TextColored({0.6f,0.9f,0.6f,1.0f}, "Triangles: %zu", stl_result_.triangle_count);
        ImGui::TextDisabled("BBox X: %.3f – %.3f m", stl_result_.bbox_min[0], stl_result_.bbox_max[0]);
        ImGui::TextDisabled("BBox Y: %.3f – %.3f m", stl_result_.bbox_min[1], stl_result_.bbox_max[1]);
        ImGui::TextDisabled("BBox Z: %.3f – %.3f m", stl_result_.bbox_min[2], stl_result_.bbox_max[2]);
    }
    if (geo_mode_ == 1 && csv_result_.success) {
        ImGui::TextColored({0.6f,0.9f,0.6f,1.0f}, "Nodes: %zu", csv_result_.nodes.size());
        ImGui::TextDisabled("T range: %.1f – %.1f °C", csv_result_.temp_min, csv_result_.temp_max);
    }
    ImGui::EndChild();

    ImGui::SameLine();

    // 3D viewport (drawn with ImDrawList)
    ImVec2 vp_pos  = ImGui::GetCursorScreenPos();
    float  vp_w    = panelW - 220.0f - ImGui::GetStyle().ItemSpacing.x * 2;
    float  vp_h    = panelH;

    ImGui::BeginChild("##geo_vp", {vp_w, vp_h}, false,
                      ImGuiWindowFlags_NoScrollbar);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    // Background
    dl->AddRectFilled(vp_pos, {vp_pos.x + vp_w, vp_pos.y + vp_h},
                      IM_COL32(15, 18, 28, 255), 4.0f);

    // Grid lines
    {
        int gridN = 10;
        float gs  = std::min(vp_w, vp_h) / (gridN + 2);
        float cx = vp_pos.x + vp_w * 0.5f, cy = vp_pos.y + vp_h * 0.5f;
        for (int i = -gridN; i <= gridN; ++i) {
            float t = i * gs;
            dl->AddLine({cx + t, vp_pos.y + 4}, {cx + t, vp_pos.y + vp_h - 4},
                        IM_COL32(35,45,65,255), 1.0f);
            dl->AddLine({vp_pos.x + 4, cy + t}, {vp_pos.x + vp_w - 4, cy + t},
                        IM_COL32(35,45,65,255), 1.0f);
        }
    }

    // Handle mouse drag for rotation
    {
        ImGui::SetCursorPos({0, 0});
        ImGui::InvisibleButton("##vp_drag", {vp_w, vp_h});
        if (ImGui::IsItemActive()) {
            ImVec2 delta = ImGui::GetMouseDragDelta(0, 0);
            ImGui::ResetMouseDragDelta(0);
            rot_y_ += delta.x * 0.5f;
            rot_x_ += delta.y * 0.5f;
        }
        float wheel = ImGui::GetIO().MouseWheel;
        if (ImGui::IsItemHovered() && wheel != 0.f) {
            zoom_ = std::max(0.1f, zoom_ + wheel * 0.1f);
        }
    }

    // Simple project3D using rotation + orthographic projection
    float cx = vp_pos.x + vp_w * 0.5f + pan_x_ * 100.0f;
    float cy = vp_pos.y + vp_h * 0.5f + pan_y_ * 100.0f;
    float sx = zoom_ * std::min(vp_w, vp_h) * 0.4f;
    float sy = sx;

    float rX  = rot_x_ * 3.14159f / 180.0f;
    float rY  = rot_y_ * 3.14159f / 180.0f;
    float cX  = cosf(rX), sX = sinf(rX);
    float cY  = cosf(rY), sY = sinf(rY);

    auto project = [&](float px, float py, float pz) -> ImVec2 {
        // Rotate around Y then X
        float rx = cY*px + sY*pz;
        float ry = py;
        float rz = -sY*px + cY*pz;
        float fx = rx;
        float fy = cX*ry - sX*rz;
        return {cx + fx * sx, cy - fy * sy};
    };

    if (geo_mode_ == 0) {
        // STL mesh
        if (stl_result_.success && !stl_result_.triangles.empty()) {
            // Compute centroid for centering
            double sumX=0,sumY=0,sumZ=0;
            for (auto& t : stl_result_.triangles) {
                sumX += t.v0[0]+t.v1[0]+t.v2[0];
                sumY += t.v0[1]+t.v1[1]+t.v2[1];
                sumZ += t.v0[2]+t.v1[2]+t.v2[2];
            }
            int N3 = (int)(stl_result_.triangles.size() * 3);
            float mX=(float)(sumX/N3), mY=(float)(sumY/N3), mZ=(float)(sumZ/N3);
            float span = std::max({stl_result_.bbox_max[0]-stl_result_.bbox_min[0],
                                   stl_result_.bbox_max[1]-stl_result_.bbox_min[1],
                                   stl_result_.bbox_max[2]-stl_result_.bbox_min[2], 1e-6f});

            // Draw triangles (limit to first 5000 for performance in ImDrawList)
            int lim = (int)std::min(stl_result_.triangles.size(), (size_t)5000);
            for (int ti = 0; ti < lim; ++ti) {
                auto& tri = stl_result_.triangles[ti];
                auto p0 = project((tri.v0[0]-mX)/span, (tri.v0[1]-mY)/span, (tri.v0[2]-mZ)/span);
                auto p1 = project((tri.v1[0]-mX)/span, (tri.v1[1]-mY)/span, (tri.v1[2]-mZ)/span);
                auto p2 = project((tri.v2[0]-mX)/span, (tri.v2[1]-mY)/span, (tri.v2[2]-mZ)/span);
                // Shade by normal z-component
                float nz = tri.normal[2];
                uint8_t br = (uint8_t)(140 + (int)(nz * 80));
                dl->AddTriangle(p0, p1, p2, IM_COL32(br, br+10, br+30, 180), 0.8f);
            }

            if (stl_result_.triangles.size() > 5000) {
                dl->AddText({vp_pos.x+6, vp_pos.y+6},
                            IM_COL32(255,200,80,255),
                            "Display limited to 5000 triangles");
            }
            // Label
            char buf[64];
            snprintf(buf, sizeof(buf), "%zu triangles", stl_result_.triangle_count);
            dl->AddText({vp_pos.x+6, vp_pos.y + vp_h - 20},
                        IM_COL32(150,200,255,255), buf);
        } else {
            // Placeholder text
            const char* msg = stl_result_.success ? "No triangles" : "Load an STL file";
            ImVec2 ts = ImGui::CalcTextSize(msg);
            dl->AddText({cx - ts.x/2, cy - ts.y/2},
                        IM_COL32(100,120,160,255), msg);
        }
    } else {
        // Temperature point cloud
        if (csv_result_.success && !csv_result_.nodes.empty()) {
            float mX=(csv_result_.xmin+csv_result_.xmax)/2.f;
            float mY=(csv_result_.ymin+csv_result_.ymax)/2.f;
            float mZ=(csv_result_.zmin+csv_result_.zmax)/2.f;
            float span = std::max({csv_result_.xmax-csv_result_.xmin,
                                   csv_result_.ymax-csv_result_.ymin,
                                   csv_result_.zmax-csv_result_.zmin, 1e-6f});

            int lim = (int)std::min(csv_result_.nodes.size(), (size_t)20000);
            for (int ni = 0; ni < lim; ++ni) {
                auto& nd = csv_result_.nodes[ni];
                auto pp = project((nd.x-mX)/span, (nd.y-mY)/span, (nd.z-mZ)/span);
                ImVec4 col = tempColor(nd.temperature, csv_result_.temp_min, csv_result_.temp_max);
                ImU32  colU = ImGui::ColorConvertFloat4ToU32(col);
                dl->AddCircleFilled(pp, 3.0f, colU);
            }

            // Temperature legend bar
            float lbX = vp_pos.x + vp_w - 50.0f;
            float lbY1 = vp_pos.y + 30.0f;
            float lbY2 = vp_pos.y + vp_h - 50.0f;
            int steps = 20;
            for (int s = 0; s < steps; ++s) {
                float frac = (float)s / (steps-1);
                float T = csv_result_.temp_min + frac*(csv_result_.temp_max-csv_result_.temp_min);
                ImVec4 col = tempColor(T, csv_result_.temp_min, csv_result_.temp_max);
                ImU32 colU = ImGui::ColorConvertFloat4ToU32(col);
                float y0 = lbY2 - frac*(lbY2-lbY1);
                float y1 = y0 - (lbY2-lbY1)/steps;
                dl->AddRectFilled({lbX, y1}, {lbX+18, y0}, colU);
            }
            char tBuf[32];
            snprintf(tBuf, sizeof(tBuf), "%.1f°C", csv_result_.temp_max);
            dl->AddText({lbX+20, lbY1}, IM_COL32(255,255,255,220), tBuf);
            snprintf(tBuf, sizeof(tBuf), "%.1f°C", csv_result_.temp_min);
            dl->AddText({lbX+20, lbY2-14}, IM_COL32(255,255,255,220), tBuf);

            // Inspection: show info for node under mouse
            ImVec2 mpos = ImGui::GetMousePos();
            float best = 25.0f;
            const TempNode* sel = nullptr;
            for (int ni = 0; ni < lim; ++ni) {
                auto& nd = csv_result_.nodes[ni];
                auto pp = project((nd.x-mX)/span, (nd.y-mY)/span, (nd.z-mZ)/span);
                float d = (mpos.x-pp.x)*(mpos.x-pp.x)+(mpos.y-pp.y)*(mpos.y-pp.y);
                if (d < best) { best=d; sel=&nd; }
            }
            if (sel) {
                char nbuf[128];
                snprintf(nbuf, sizeof(nbuf),
                         "Node %lld  (%.3f, %.3f, %.3f) m  T=%.2f°C",
                         (long long)sel->node_id, sel->x, sel->y, sel->z, sel->temperature);
                dl->AddText({vp_pos.x+6, vp_pos.y+vp_h-20},
                            IM_COL32(220,240,255,255), nbuf);
            }
        } else {
            const char* msg = csv_result_.success ? "No nodes" : "Load a Temperature CSV file";
            ImVec2 ts = ImGui::CalcTextSize(msg);
            dl->AddText({cx - ts.x/2, cy - ts.y/2},
                        IM_COL32(100,120,160,255), msg);
        }
    }
    ImGui::EndChild();
}

// ─── thermal analysis view ────────────────────────────────────────────────────

void App::renderThermalView() {
    ImGui::RadioButton("3D Heat Sink Multi-Solver (FEM / FDM / FVM)", &th_mode_, 1);
    ImGui::SameLine();
    ImGui::RadioButton("1D Rod Benchmark (Round 1)", &th_mode_, 0);
    ImGui::Separator();

    float panelH = ImGui::GetContentRegionAvail().y - 28.0f;

    if (th_mode_ == 1) {
        // ── 3D Heat Sink Multi-Solver View ──────────────────────────────────
        ImGui::BeginChild("##t3d_ctrl", {280.0f, panelH}, true);
        ImGui::TextColored({0.4f, 0.8f, 1.0f, 1.0f}, "3D Geometry & Physics");
        ImGui::Separator();
        ImGui::TextDisabled("Object: Aluminium Heat Sink");
        ImGui::TextDisabled("Base: 80x60x10 mm  Fins: 5x (80x2x30 mm)");
        ImGui::TextDisabled("Material: Al 6061 (k = 167 W/mK)");
        ImGui::TextDisabled("Source: 10W on 10x10mm offset patch");
        ImGui::Spacing();

        ImGui::TextColored({0.4f, 0.8f, 1.0f, 1.0f}, "Convection Environment");
        ImGui::RadioButton("Case A: h = 10 W/m²K (Natural)", &t3d_case_choice_, 0);
        ImGui::RadioButton("Case B: h = 25 W/m²K (Forced)",  &t3d_case_choice_, 1);
        ImGui::Separator();

        ImGui::TextColored({0.4f, 0.8f, 1.0f, 1.0f}, "Numerical Solver Selection");
        ImGui::RadioButton("FEM (Linear Tets)",         &t3d_solver_choice_, 0);
        ImGui::RadioButton("FDM (7-pt Grid)",           &t3d_solver_choice_, 1);
        ImGui::RadioButton("FVM (Cell Control Vol)",    &t3d_solver_choice_, 2);
        ImGui::RadioButton("Compare All (FEM+FDM+FVM)",  &t3d_solver_choice_, 3);
        ImGui::Separator();

        ImGui::TextColored({0.4f, 0.8f, 1.0f, 1.0f}, "Mesh Resolution");
        ImGui::RadioButton("Coarse (~2.1k - 9.6k DOF)",  &t3d_res_choice_, 0);
        ImGui::RadioButton("Medium (~13k - 72k DOF)",    &t3d_res_choice_, 1);
        ImGui::RadioButton("Fine (~40k - 248k DOF)",     &t3d_res_choice_, 2);
        ImGui::Separator();

        if (t3d_running_) {
            ImGui::TextColored({1.0f, 0.8f, 0.2f, 1.0f}, "Solving 3D Thermal System...");
        } else {
            if (ImGui::Button("Run 3D Simulation", {-1, 0})) {
                startT3DWork();
            }
        }
        ImGui::Spacing();

        if (ImGui::Button("Export Results CSV...", {-1, 0})) {
#ifdef _WIN32
            OPENFILENAMEA ofn{};
            char szFile[MAX_PATH] = "simulation_results.csv";
            ofn.lStructSize = sizeof(ofn);
            ofn.lpstrFile   = szFile;
            ofn.nMaxFile    = MAX_PATH;
            ofn.lpstrFilter = "CSV Files\0*.csv\0";
            ofn.Flags       = OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;
            if (GetSaveFileNameA(&ofn)) {
                std::ofstream f(szFile);
                if (f) {
                    f << t3d::SolverResult::csv_header() << "\n";
                    for (const auto& r : t3d_results_) f << r.to_csv_row() << "\n";
                    statusBar("Exported 3D simulation results.");
                }
            }
#endif
        }

        ImGui::EndChild();

        ImGui::SameLine();

        float vpW = ImGui::GetContentRegionAvail().x;
        ImGui::BeginChild("##t3d_view", {vpW, panelH}, false);

        // Simulation Summary Table
        ImGui::TextColored({0.4f, 0.8f, 1.0f, 1.0f}, "Numerical Solver Results Comparison");
        ImGui::Separator();

        if (!t3d_results_.empty()) {
            ImGui::Columns(10, "##t3d_table", true);
            ImGui::Text("Solver");   ImGui::NextColumn();
            ImGui::Text("Case");     ImGui::NextColumn();
            ImGui::Text("DOF");      ImGui::NextColumn();
            ImGui::Text("Iters");    ImGui::NextColumn();
            ImGui::Text("Time(ms)"); ImGui::NextColumn();
            ImGui::Text("Tmin(°C)"); ImGui::NextColumn();
            ImGui::Text("Tmax(°C)"); ImGui::NextColumn();
            ImGui::Text("Tavg(°C)"); ImGui::NextColumn();
            ImGui::Text("Tsrc(°C)"); ImGui::NextColumn();
            ImGui::Text("Balance Err"); ImGui::NextColumn();
            ImGui::Separator();

            for (const auto& r : t3d_results_) {
                ImGui::Text("%s", r.solver_name.c_str()); ImGui::NextColumn();
                ImGui::Text("%s", r.case_label.c_str());  ImGui::NextColumn();
                ImGui::Text("%d", r.n_dof);               ImGui::NextColumn();
                ImGui::Text("%d", r.iterations);          ImGui::NextColumn();
                ImGui::Text("%.1f", r.t_total_ms);        ImGui::NextColumn();
                ImGui::Text("%.2f", r.T_min);             ImGui::NextColumn();
                ImGui::Text("%.2f", r.T_max);             ImGui::NextColumn();
                ImGui::Text("%.2f", r.T_avg);             ImGui::NextColumn();
                ImGui::Text("%.2f", r.T_src);             ImGui::NextColumn();
                ImGui::Text("%.2e", r.Q_balance_rel);     ImGui::NextColumn();
            }
            ImGui::Columns(1);
            ImGui::Spacing();
        } else {
            ImGui::TextDisabled("Click 'Run 3D Simulation' to execute the selected solver(s).");
            ImGui::Spacing();
        }

        // 3D Heat Sink Temperature Viewport
        ImVec2 vp_pos = ImGui::GetCursorScreenPos();
        float viewH = panelH - 180.0f;
        ImGui::BeginChild("##t3d_3d", {vpW - 10, viewH}, true, ImGuiWindowFlags_NoScrollbar);
        ImDrawList* dl = ImGui::GetWindowDrawList();

        dl->AddRectFilled(vp_pos, {vp_pos.x + vpW - 10, vp_pos.y + viewH},
                          IM_COL32(14, 18, 26, 255), 4.0f);

        // Projector
        float cx = vp_pos.x + (vpW - 10) * 0.5f + pan_x_ * 100.0f;
        float cy = vp_pos.y + viewH * 0.5f + pan_y_ * 100.0f;
        float sx = zoom_ * std::min(vpW - 10, viewH) * 4.0f;
        float sy = sx;

        float rX = rot_x_ * 3.14159f / 180.0f;
        float rY = rot_y_ * 3.14159f / 180.0f;
        float cX = cosf(rX), sX = sinf(rX);
        float cY = cosf(rY), sY = sinf(rY);

        auto project3d = [&](float px, float py, float pz) -> ImVec2 {
            float rx = cY*px + sY*pz;
            float ry = py;
            float rz = -sY*px + cY*pz;
            float fx = rx;
            float fy = cX*ry - sX*rz;
            return {cx + fx * sx, cy - fy * sy};
        };

        // Render heat sink wireframe / shaded blocks
        // Base box: [-0.04, 0.04] x [-0.03, 0.03] x [-0.02, -0.01]
        double t_lo = 54.0, t_hi = 60.0;
        if (!t3d_results_.empty()) {
            t_lo = t3d_results_[0].T_min;
            t_hi = t3d_results_[0].T_max;
        }

        auto draw_box = [&](double x0, double x1, double y0, double y1, double z0, double z1, double temp) {
            ImVec4 colV = tempColor((float)temp, (float)t_lo, (float)t_hi);
            ImU32 col = ImGui::ColorConvertFloat4ToU32(colV);
            ImU32 edge = IM_COL32(40, 45, 60, 200);

            ImVec2 p000 = project3d(x0, y0, z0);
            ImVec2 p100 = project3d(x1, y0, z0);
            ImVec2 p110 = project3d(x1, y1, z0);
            ImVec2 p010 = project3d(x0, y1, z0); (void)p010;
            ImVec2 p001 = project3d(x0, y0, z1);
            ImVec2 p101 = project3d(x1, y0, z1);
            ImVec2 p111 = project3d(x1, y1, z1);
            ImVec2 p011 = project3d(x0, y1, z1);

            // Shaded top face
            dl->AddQuadFilled(p001, p101, p111, p011, col);
            dl->AddQuad(p001, p101, p111, p011, edge, 1.0f);
            // Front face
            dl->AddQuadFilled(p000, p100, p101, p001, col);
            dl->AddQuad(p000, p100, p101, p001, edge, 1.0f);
            // Right face
            dl->AddQuadFilled(p100, p110, p111, p101, col);
            dl->AddQuad(p100, p110, p111, p101, edge, 1.0f);
        };

        // Draw Base
        draw_box(-0.040, 0.040, -0.030, 0.030, -0.015, -0.005, (t_lo + t_hi)*0.5);

        // Draw 5 Fins
        double fin_y_offsets[5] = { -0.024, -0.012, 0.0, 0.012, 0.024 };
        for (int f = 0; f < 5; ++f) {
            double fy = fin_y_offsets[f];
            draw_box(-0.040, 0.040, fy - 0.001, fy + 0.001, -0.005, 0.025, t_lo + 0.2*(t_hi - t_lo));
        }

        // Colorbar legend
        float lbX = vp_pos.x + 15.0f, lbY1 = vp_pos.y + 20.0f, lbY2 = vp_pos.y + viewH - 25.0f;
        int steps = 20;
        for (int s = 0; s < steps; ++s) {
            float frac = (float)s / (steps - 1);
            float T = t_lo + frac * (t_hi - t_lo);
            ImVec4 c = tempColor(T, (float)t_lo, (float)t_hi);
            float y0 = lbY2 - frac * (lbY2 - lbY1);
            float y1 = y0 - (lbY2 - lbY1) / steps;
            dl->AddRectFilled({lbX, y1}, {lbX + 16, y0}, ImGui::ColorConvertFloat4ToU32(c));
        }
        char bMax[32], bMin[32];
        snprintf(bMax, sizeof(bMax), "%.1f°C", t_hi);
        snprintf(bMin, sizeof(bMin), "%.1f°C", t_lo);
        dl->AddText({lbX + 22, lbY1}, IM_COL32(255, 255, 255, 240), bMax);
        dl->AddText({lbX + 22, lbY2 - 14}, IM_COL32(255, 255, 255, 240), bMin);

        dl->AddText({vp_pos.x + 10, vp_pos.y + viewH - 18},
                    IM_COL32(200, 220, 240, 255),
                    "Drag to rotate  |  Mouse wheel to zoom  |  Aluminium 6061 Heat Sink");

        ImGui::EndChild();
        ImGui::EndChild();
        return;
    }

    // ── 1D FEM View (Round 1) ───────────────────────────────────────────────
    ImGui::BeginChild("##th_ctrl", {260.0f, panelH}, true);
    ImGui::TextColored({0.4f, 0.8f, 1.0f, 1.0f}, "FEM Parameters");
    ImGui::Separator();

    ImGui::TextDisabled("Assignment defaults (read-only):");
    ImGui::TextDisabled("L=1m, A=1e-4m², k=10W/mK, q=10000W/m³");
    ImGui::TextDisabled("T(0)=100°C, T(L)=20°C");
    ImGui::Spacing();

    DragDouble("L [m]",      &fem_params_.L,  0.01f, 0.1, 10.0);
    DragDouble("A [m²]",     &fem_params_.A,  0.0f, 0.0, 1.0);
    DragDouble("k [W/mK]",   &fem_params_.k,  0.1f, 0.1, 1000.0);
    DragDouble("q [W/m³]",   &fem_params_.q,  100.f, 0.0, 1e6);
    DragDouble("T(0) [°C]",  &fem_params_.T_left,  1.0f, -1000, 1000);
    DragDouble("T(L) [°C]",  &fem_params_.T_right, 1.0f, -1000, 1000);
    ImGui::Separator();

    if (ImGui::Button("Reset to Assignment Defaults", {-1, 0})) {
        fem_params_ = FemParams{};
    }

    ImGui::Spacing();
    if (fem_running_) {
        ImGui::TextColored({1.0f, 0.8f, 0.2f, 1.0f}, "Running FEM...");
        if (ImGui::Button("Cancel", {-1, 0})) {
            // Signal cancel (FEM runs fast so this is mostly cosmetic)
        }
    } else {
        if (ImGui::Button("Run FEM Convergence Study", {-1, 0})) {
            startFemWork();
        }
    }

    ImGui::Spacing();
    if (!fem_results_.empty()) {
        ImGui::Separator();
        ImGui::TextColored({0.4f, 0.8f, 1.0f, 1.0f}, "Convergence Results");
        ImGui::Columns(2, "##fem_conv");
        ImGui::Text("Elements"); ImGui::NextColumn();
        ImGui::Text("Max Err [°C]"); ImGui::NextColumn();
        ImGui::Separator();
        for (auto& r : fem_results_) {
            ImGui::Text("%d", r.params.n_elements); ImGui::NextColumn();
            ImGui::Text("%.3e", r.max_abs_error);   ImGui::NextColumn();
        }
        ImGui::Columns(1);
        ImGui::Spacing();
        if (ImGui::Button("Export FEM CSV...", {-1, 0})) {
#ifdef _WIN32
            OPENFILENAMEA ofn{};
            char szFile[MAX_PATH] = "fem_results.csv";
            ofn.lStructSize = sizeof(ofn);
            ofn.lpstrFile   = szFile;
            ofn.nMaxFile    = MAX_PATH;
            ofn.lpstrFilter = "CSV Files\0*.csv\0";
            ofn.Flags       = OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;
            if (GetSaveFileNameA(&ofn)) {
                if (femExportCsv(fem_results_, szFile))
                    statusBar("FEM results exported.");
                else
                    statusBar("Export failed.", true);
            }
#endif
        }
    }
    ImGui::EndChild();

    ImGui::SameLine();

    float vpW = ImGui::GetContentRegionAvail().x;

    ImGui::BeginChild("##th_plots", {vpW, panelH}, false);
    if (!fem_results_.empty()) {
        // ── Temperature vs Position plot ────────────────────────────────────
        if (ImPlot::BeginPlot("Temperature vs. Position [m]",
                              {vpW - 10, panelH * 0.55f})) {
            ImPlot::SetupAxes("Position x [m]", "Temperature [°C]",
                              ImPlotAxisFlags_AutoFit, ImPlotAxisFlags_AutoFit);

            // Analytical solution
            std::vector<double> xA(200), TA(200);
            for (int i = 0; i < 200; ++i) {
                xA[i] = fem_params_.L * i / 199.0;
                TA[i] = femAnalytical(xA[i], fem_params_);
            }
            ImPlotSpec specAna;
            specAna.LineColor = {1.0f, 0.5f, 0.0f, 1.0f};
            specAna.LineWeight = 2.0f;
            ImPlot::PlotLine("Analytical", xA.data(), TA.data(), 200, specAna);

            // FEM results
            const ImVec4 colors[] = {
                {0.3f,0.7f,1.0f,1.0f}, {0.4f,1.0f,0.4f,1.0f},
                {1.0f,0.4f,1.0f,1.0f}, {1.0f,0.9f,0.3f,1.0f}
            };
            for (int ri = 0; ri < (int)fem_results_.size(); ++ri) {
                auto& r = fem_results_[ri];
                char lbl[32];
                snprintf(lbl, sizeof(lbl), "FEM n=%d", r.params.n_elements);
                ImPlotSpec specFem;
                specFem.LineColor = colors[ri % 4];
                specFem.LineWeight = 1.2f;
                ImPlot::PlotLine(lbl,
                                 r.x_nodes.data(),
                                 r.T_fem.data(),
                                 (int)r.x_nodes.size(),
                                 specFem);
            }
            ImPlot::EndPlot();
        }

        // ── Error vs n_elements plot ────────────────────────────────────────
        std::vector<double> ns, errs;
        for (auto& r : fem_results_) {
            ns.push_back((double)r.params.n_elements);
            errs.push_back(r.max_abs_error);
        }
        if (ImPlot::BeginPlot("Convergence: Max Absolute Error vs. Mesh",
                              {vpW - 10, panelH * 0.42f})) {
            ImPlot::SetupAxes("Number of Elements", "Max |T_FEM - T_exact| at midpoints [°C]",
                              ImPlotAxisFlags_AutoFit, ImPlotAxisFlags_AutoFit);
            ImPlot::SetupAxisScale(ImAxis_X1, ImPlotScale_Log10);
            ImPlot::SetupAxisScale(ImAxis_Y1, ImPlotScale_Log10);
            ImPlotSpec specErr;
            specErr.LineColor = {0.3f,0.8f,1.0f,1.0f};
            specErr.LineWeight = 2.0f;
            specErr.Marker = ImPlotMarker_Circle;
            specErr.MarkerSize = 6.0f;
            ImPlot::PlotLine("Max Error", ns.data(), errs.data(), (int)ns.size(), specErr);
            ImPlot::EndPlot();
        }
    } else {
        float cw = vpW * 0.5f, ch = panelH * 0.5f;
        ImGui::SetCursorPos({cw - 120, ch - 10});
        ImGui::TextColored({0.5f,0.6f,0.8f,1.0f},
                           "Press 'Run FEM Convergence Study' to compute results.");
    }
    ImGui::EndChild();

    // Backprop section below thermal (if space)
}

// ─── performance view ─────────────────────────────────────────────────────────

void App::renderPerformanceView() {
    float panelH = ImGui::GetContentRegionAvail().y - 28.0f;

    ImGui::BeginChild("##perf_ctrl", {280.0f, panelH}, true);
    ImGui::TextColored({0.4f, 0.8f, 1.0f, 1.0f}, "Benchmark Configuration");
    ImGui::Separator();

    ImGui::DragInt("Cases",      &bench_cfg_.n_cases,      10, 10, 10000);
    ImGui::DragInt("Elements",   &bench_cfg_.n_elements,    1, 1, 400);
    ImGui::DragInt("Max Workers",&bench_cfg_.max_workers,   1, 1, 64);
    ImGui::DragInt("Repeats",    &bench_cfg_.n_repeats,     1, 1, 20);
    ImGui::DragInt("Warmup",     &bench_cfg_.n_warmup,      1, 0, 5);

    ImGui::Spacing();
    if (bench_running_) {
        ImGui::TextColored({1.0f, 0.8f, 0.2f, 1.0f}, "Benchmark running...");
    } else {
        if (ImGui::Button("Run Benchmark", {-1, 0})) {
            startBenchWork();
        }
    }

    if (bench_done_) {
        std::lock_guard<std::mutex> lk(bench_mutex_);
        auto& r = bench_result_;
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::TextColored({0.4f, 0.8f, 1.0f, 1.0f}, "System Info");
        ImGui::TextWrapped("CPU: %s", r.sysinfo.cpu_name.c_str());
        ImGui::TextDisabled("Logical CPUs: %d", r.sysinfo.logical_cores);
        ImGui::TextDisabled("Physical CPUs: %d", r.sysinfo.physical_cores);
        if (r.sysinfo.ram_gb > 0)
            ImGui::TextDisabled("RAM: %.1f GB", r.sysinfo.ram_gb);
        ImGui::TextDisabled("Compiler: %s", r.sysinfo.compiler.c_str());
        ImGui::TextDisabled("Flags: %s", r.sysinfo.opt_flags.c_str());
        ImGui::TextDisabled("Setup time: %.1f ms", r.launch_to_ready_ms);
        ImGui::Spacing();
        ImGui::TextColored({0.4f, 0.8f, 1.0f, 1.0f}, "Summary");
        ImGui::Columns(4, "##bench_sum");
        ImGui::Text("Workers"); ImGui::NextColumn();
        ImGui::Text("Median ms"); ImGui::NextColumn();
        ImGui::Text("Speedup"); ImGui::NextColumn();
        ImGui::Text("Efficiency"); ImGui::NextColumn();
        ImGui::Separator();
        for (auto& s : r.summaries) {
            ImGui::Text("%d",    s.n_workers); ImGui::NextColumn();
            ImGui::Text("%.1f", s.median_ms); ImGui::NextColumn();
            ImGui::Text("%.2f", s.speedup);   ImGui::NextColumn();
            ImGui::Text("%.2f", s.efficiency);ImGui::NextColumn();
        }
        ImGui::Columns(1);
        ImGui::Spacing();
        bool allPass = true;
        for (auto& s : r.summaries) if (!s.agreement) allPass=false;
        if (allPass)
            ImGui::TextColored({0.3f,0.9f,0.3f,1.0f}, "Serial/parallel agreement: PASS (max diff <= 1e-8 °C)");
        else
            ImGui::TextColored({1.0f,0.4f,0.4f,1.0f}, "Serial/parallel agreement: FAIL");

        if (r.sysinfo.single_logical_cpu)
            ImGui::TextWrapped("[NOTE] Single logical CPU detected – speedup cannot be meaningfully assessed.");

        if (ImGui::Button("Export CSV...", {-1,0})) {
#ifdef _WIN32
            OPENFILENAMEA ofn{};
            char szFile[MAX_PATH] = "benchmark_results.csv";
            ofn.lStructSize = sizeof(ofn);
            ofn.lpstrFile   = szFile;
            ofn.nMaxFile    = MAX_PATH;
            ofn.lpstrFilter = "CSV Files\0*.csv\0";
            ofn.Flags       = OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;
            if (GetSaveFileNameA(&ofn)) {
                benchmarkExportCsv(bench_result_, szFile);
                statusBar("Benchmark results exported.");
            }
#endif
        }
    }
    ImGui::EndChild();

    ImGui::SameLine();

    float vpW = ImGui::GetContentRegionAvail().x;

    ImGui::BeginChild("##perf_plots", {vpW, panelH}, false);

    if (bench_done_) {
        std::lock_guard<std::mutex> lk(bench_mutex_);
        auto& r = bench_result_;

        // Speedup chart
        std::vector<double> workers, speedups, efficiencies;
        for (auto& s : r.summaries) {
            workers.push_back((double)s.n_workers);
            speedups.push_back(s.speedup);
            efficiencies.push_back(s.efficiency);
        }

        float halfH = (panelH - 10.0f) * 0.48f;

        if (ImPlot::BeginPlot("Speedup (T1/Tp)", {vpW - 10, halfH})) {
            ImPlot::SetupAxes("Number of Workers p", "Speedup T1/Tp",
                              ImPlotAxisFlags_AutoFit, ImPlotAxisFlags_None);
            ImPlot::SetupAxisLimits(ImAxis_Y1, 0.0, std::max(4.0, speedups.empty()?1.0:*std::max_element(speedups.begin(),speedups.end())*1.3), ImGuiCond_Always);
            // Ideal speedup reference
            std::vector<double> wIdeal = workers;
            ImPlotSpec sIdeal1;
            sIdeal1.LineColor = {0.5f,0.5f,0.5f,0.7f};
            sIdeal1.LineWeight = 1.0f;
            ImPlot::PlotLine("Ideal", wIdeal.data(), wIdeal.data(), (int)wIdeal.size(), sIdeal1);
            ImPlotSpec sMeas1;
            sMeas1.LineColor = {0.3f,0.7f,1.0f,1.0f};
            sMeas1.LineWeight = 2.5f;
            sMeas1.Marker = ImPlotMarker_Square;
            sMeas1.MarkerSize = 7.0f;
            ImPlot::PlotLine("Measured", workers.data(), speedups.data(), (int)workers.size(), sMeas1);
            ImPlot::EndPlot();
        }
        if (ImPlot::BeginPlot("Efficiency (T1/Tp)/p", {vpW - 10, halfH})) {
            ImPlot::SetupAxes("Number of Workers p", "Efficiency",
                              ImPlotAxisFlags_AutoFit, ImPlotAxisFlags_None);
            ImPlot::SetupAxisLimits(ImAxis_Y1, 0.0, 1.3, ImGuiCond_Always);
            ImPlotSpec sIdeal2;
            sIdeal2.LineColor = {0.5f,0.5f,0.5f,0.7f};
            sIdeal2.LineWeight = 1.0f;
            std::vector<double> ideal(workers.size(), 1.0);
            ImPlot::PlotLine("Ideal", workers.data(), ideal.data(), (int)workers.size(), sIdeal2);
            ImPlotSpec sMeas2;
            sMeas2.LineColor = {0.4f,1.0f,0.5f,1.0f};
            sMeas2.LineWeight = 2.5f;
            sMeas2.Marker = ImPlotMarker_Diamond;
            sMeas2.MarkerSize = 7.0f;
            ImPlot::PlotLine("Measured", workers.data(), efficiencies.data(), (int)workers.size(), sMeas2);
            ImPlot::EndPlot();
        }

        // Backprop section
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::TextColored({0.4f, 0.8f, 1.0f, 1.0f}, "Backpropagation – Loss over 100 Training Steps");
        if (bp_done_ && !bp_result_.loss_history.empty()) {
            std::vector<double> steps(bp_result_.loss_history.size());
            std::iota(steps.begin(), steps.end(), 0.0);
            if (ImPlot::BeginPlot("Loss J vs. Training Step", {vpW - 10, panelH*0.35f})) {
                ImPlot::SetupAxes("Step", "Loss J = 0.5(ŷ-y)²",
                                  ImPlotAxisFlags_AutoFit, ImPlotAxisFlags_AutoFit);
                ImPlotSpec sLoss;
                sLoss.LineColor = {1.0f,0.5f,0.2f,1.0f};
                sLoss.LineWeight = 2.0f;
                ImPlot::PlotLine("Loss", steps.data(),
                                 bp_result_.loss_history.data(),
                                 (int)steps.size(),
                                 sLoss);
                ImPlot::EndPlot();
            }
            // Print gradient table
            ImGui::TextColored({0.4f,0.8f,1.0f,1.0f}, "Initial Gradients & FD Verification (x=1, y=2)");
            ImGui::Columns(4, "##bp_grad");
            ImGui::Text("Param"); ImGui::NextColumn();
            ImGui::Text("Analytical"); ImGui::NextColumn();
            ImGui::Text("FD (ε=1e-6)"); ImGui::NextColumn();
            ImGui::Text("Abs Error"); ImGui::NextColumn();
            ImGui::Separator();
            for (auto& chk : bp_result_.fd_checks) {
                ImGui::Text("%s", chk.name.c_str()); ImGui::NextColumn();
                ImGui::Text("%.8f", chk.analytical); ImGui::NextColumn();
                ImGui::Text("%.8f", chk.fd);         ImGui::NextColumn();
                if (chk.passed)
                    ImGui::TextColored({0.3f,0.9f,0.3f,1.0f}, "%.2e ✓", chk.abs_error);
                else
                    ImGui::TextColored({1.0f,0.4f,0.4f,1.0f}, "%.2e ✗", chk.abs_error);
                ImGui::NextColumn();
            }
            ImGui::Columns(1);
        }
    } else {
        ImGui::SetCursorPos({vpW*0.3f, panelH*0.4f});
        ImGui::TextColored({0.5f,0.6f,0.8f,1.0f}, "Press 'Run Benchmark' to start.");
    }

    ImGui::EndChild();
}

// ─── hardware view ────────────────────────────────────────────────────────────

void App::renderHardwareView() {
    float panelH = ImGui::GetContentRegionAvail().y - 28.0f;

    ImGui::BeginChild("##hw_ctrl", {280.0f, panelH}, true);
    ImGui::TextColored({0.4f, 0.8f, 1.0f, 1.0f}, "Embedded Workbench [SIMULATED]");
    ImGui::Separator();
    ImGui::TextWrapped("All hardware behavior is simulated.\nNo real devices are connected.");
    ImGui::Spacing();

    if (ImGui::Button("Connect All Devices", {-1, 0})) {
        hw_log_ = hw_mgr_.connectAll();
        hw_connected_ = true;
        statusBar("Hardware connection sequence complete [SIMULATED].");
    }
    if (hw_connected_ && ImGui::Button("Disconnect All", {-1, 0})) {
        hw_mgr_.mcu().disconnect();
        hw_mgr_.fpga().disconnect();
        hw_mgr_.jtag().disconnect();
        hw_connected_ = false;
        hw_log_ += "[SIMULATED] All devices disconnected.\n";
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::TextColored({0.4f, 0.8f, 1.0f, 1.0f}, "Device Status");

    auto drawDevRow = [](const hw::MockDevice& dev) {
        auto st = dev.state();
        ImVec4 col = (st == hw::ConnectState::Connected) ? ImVec4{0.3f,0.9f,0.3f,1.f}
                   : (st == hw::ConnectState::Error)     ? ImVec4{1.0f,0.4f,0.4f,1.f}
                   :                                        ImVec4{0.7f,0.7f,0.7f,1.f};
        ImGui::TextColored(col, "[%s]", hw::connectStateName(st));
        ImGui::SameLine();
        ImGui::TextDisabled("%s", hw::deviceTypeName(dev.type()));
    };

    drawDevRow(hw_mgr_.mcu());
    drawDevRow(hw_mgr_.fpga());
    drawDevRow(hw_mgr_.jtag());

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::TextColored({0.4f, 0.8f, 1.0f, 1.0f}, "Connection Log");
    ImGui::InputTextMultiline("##hw_log",
                              hw_log_.data(), hw_log_.size()+1,
                              {-1, 120},
                              ImGuiInputTextFlags_ReadOnly);

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::TextColored({0.4f, 0.8f, 1.0f, 1.0f}, "Live Telemetry [SIMULATED]");
    if (hw_connected_) {
        auto mcuT = hw_mgr_.mcu().readTelemetry();
        auto fpgaT= hw_mgr_.fpga().readTelemetry();
        auto jtagT= hw_mgr_.jtag().readTelemetry();
        if (mcuT)  ImGui::TextDisabled("MCU  Temp: %.2f %s  | %s", mcuT->value,  mcuT->unit.c_str(),  mcuT->timestamp.c_str());
        if (fpgaT) ImGui::TextDisabled("FPGA VCC:  %.3f %s | %s", fpgaT->value, fpgaT->unit.c_str(), fpgaT->timestamp.c_str());
        if (jtagT) ImGui::TextDisabled("JTAG TAP:  %.0f %s | %s",  jtagT->value, jtagT->unit.c_str(), jtagT->timestamp.c_str());
    } else {
        ImGui::TextDisabled("(connect to see live telemetry)");
    }
    ImGui::EndChild();

    ImGui::SameLine();

    float vpW = ImGui::GetContentRegionAvail().x;
    ImGui::BeginChild("##hw_info", {vpW, panelH}, false);

    ImGui::TextColored({0.4f, 0.8f, 1.0f, 1.0f}, "System Block Diagram (ASCII)");
    ImGui::Separator();

    const char* diagram =
R"(
  ┌──────────────────────────────────────────────────────────────────────────┐
  │                    HOST PC (Windows 11 x64)                             │
  │                  THERMATRIX Application (C++17)                         │
  │         USB ↔ MCU          USB ↔ FPGA          USB ↔ J-Link            │
  └────────┬──────────────────────────────┬──────────────────┬──────────────┘
           │  USB Full-Speed (CDC)         │  USB 3.0 (FT601) │  USB 2.0 HID
           ▼                              ▼                   ▼
  ┌────────────────┐          ┌──────────────────┐  ┌────────────────────┐
  │  MCU [SIMULATED]│          │ FPGA [SIMULATED]  │  │ J-Link [SIMULATED] │
  │  STM32F407VGT6  │          │ Xilinx Artix-7    │  │ SEGGER J-Link BASE │
  │  ARM Cortex-M4  │          │ XC7A35T-1CPG236C  │  │ JTAG + SWD        │
  │  168 MHz         │          │ 33K LUTs / 90 DSPs│  │ IEEE 1149.1       │
  │  1 MB Flash      │          │ 1.8 Kb BRAM       │  │                    │
  │  192 KB SRAM     │          │                   │  │  TCK ──► MCU/FPGA  │
  │                  │          │  FPGA ←→ MCU:     │  │  TMS ──► MCU/FPGA  │
  │  3.3V core       │  SPI/    │  SPI @ 3.3V       │  │  TDI ──► MCU/FPGA  │
  │  5V USB          │  UART    │  1.0V core        │  │  TDO ◄── MCU/FPGA  │
  │                  │◄────────►│  1.8V IO          │  │  TRST►  MCU/FPGA  │
  │  JTAG pins:      │          │  3.3V USB bridge  │  │                    │
  │  TDI,TDO,TCK     │          │                   │  │  Level shift:      │
  │  TMS,TRST,GND    │          │  JTAG TAP on-chip │  │  1.2-5V adaptive   │
  └────────────────┘          └──────────────────┘  └────────────────────┘

  Power domains: MCU=3.3V, FPGA_core=1.0V, FPGA_IO=1.8V, J-Link=5V(USB)
  Level shifting: J-Link has on-board level shifters; no external circuit needed.
  JTAG ownership: J-Link adapter drives TCK/TMS/TDI; MCU/FPGA provide TDO.
  JTAG ≠ compute: JTAG (IEEE 1149.1) is a debug/boundary-scan interface only.
  It cannot substitute for USB/SPI/UART as a data transport.

  [ALL HARDWARE SIMULATED – no real devices are connected]
)";
    ImGui::InputTextMultiline("##hw_diag",
                              const_cast<char*>(diagram), strlen(diagram)+1,
                              {-1, 380},
                              ImGuiInputTextFlags_ReadOnly);

    ImGui::Spacing();
    ImGui::TextColored({0.4f, 0.8f, 1.0f, 1.0f}, "Capability Table");

    auto showCaps = [&](const hw::MockDevice& dev) {
        if (ImGui::TreeNode(hw::deviceTypeName(dev.type()))) {
            auto caps = dev.capabilities();
            ImGui::Columns(2, "##caps");
            for (auto& cap : caps) {
                ImGui::TextDisabled("%s:", cap.name.c_str()); ImGui::NextColumn();
                ImGui::TextWrapped("%s", cap.value.c_str()); ImGui::NextColumn();
            }
            ImGui::Columns(1);
            ImGui::TreePop();
        }
    };
    showCaps(hw_mgr_.mcu());
    showCaps(hw_mgr_.fpga());
    showCaps(hw_mgr_.jtag());

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::TextColored({0.8f,0.8f,0.4f,1.0f}, "Safe Integration Plan & Limitations");
    ImGui::TextWrapped(
        "1. Before physical integration: prototype each interface (USB-CDC, SPI) "
        "separately on a development board.\n"
        "2. Use current-limiting resistors on JTAG signals to protect against "
        "voltage mismatch.\n"
        "3. Verify power-on sequencing: FPGA core (1.0V) before IO (1.8V).\n"
        "4. Isolate FPGA and MCU grounds at a single star point to prevent "
        "ground loops.\n"
        "5. Physical wiring, PCB fabrication, firmware deployment, and real "
        "FPGA programming are NOT covered here.\n"
        "LIMITATION: This implementation is entirely simulated; no real hardware "
        "was tested or verified."
    );

    ImGui::EndChild();
}

// ─── background workers ───────────────────────────────────────────────────────

void App::startFemWork() {
    if (fem_running_) return;
    fem_running_ = true;
    if (fem_thread_.joinable()) fem_thread_.join();

    fem_thread_ = std::thread([this]() {
        auto results = femConvergence(fem_params_, {10, 20, 40, 80});
        fem_results_ = std::move(results);
        fem_running_ = false;
        statusBar("FEM convergence study complete.");

        // Auto-export to results/
        fs::create_directories("results");
        femExportCsv(fem_results_, "results/fem_results.csv");
    });
}

void App::startBenchWork() {
    if (bench_running_) return;
    bench_running_ = true;
    bench_done_    = false;
    if (bench_thread_.joinable()) bench_thread_.join();

    bench_thread_ = std::thread([this]() {
        auto r = runBenchmark(bench_cfg_);
        {
            std::lock_guard<std::mutex> lk(bench_mutex_);
            bench_result_ = std::move(r);
            bench_done_   = true;
        }
        bench_running_ = false;
        statusBar("Benchmark complete.");

        // Auto-export
        fs::create_directories("results");
        benchmarkExportCsv(bench_result_, "results/benchmark_results.csv");
    });
}

void App::startT3DWork() {
    if (t3d_running_) return;
    t3d_running_ = true;
    if (t3d_thread_.joinable()) t3d_thread_.join();

    t3d_thread_ = std::thread([this]() {
        t3d::ConvCase cc = (t3d_case_choice_ == 0) ? t3d::ConvCase::A : t3d::ConvCase::B;
        t3d::MeshResolution mr = t3d::MeshResolution::Coarse;
        if (t3d_res_choice_ == 1) mr = t3d::MeshResolution::Medium;
        else if (t3d_res_choice_ == 2) mr = t3d::MeshResolution::Fine;

        std::vector<t3d::SolverResult> res_list;
        if (t3d_solver_choice_ == 0 || t3d_solver_choice_ == 3) {
            t3d::FemSolver3D fem(t3d_prob_);
            res_list.push_back(fem.solve(cc, mr));
        }
        if (t3d_solver_choice_ == 1 || t3d_solver_choice_ == 3) {
            t3d::FdmSolver3D fdm(t3d_prob_);
            res_list.push_back(fdm.solve(cc, mr));
        }
        if (t3d_solver_choice_ == 2 || t3d_solver_choice_ == 3) {
            t3d::FvmSolver3D fvm(t3d_prob_);
            res_list.push_back(fvm.solve(cc, mr));
        }

        t3d_results_ = std::move(res_list);
        t3d_running_ = false;
        statusBar("3D Heat Sink simulation finished.");
    });
}

} // namespace thermatrix
