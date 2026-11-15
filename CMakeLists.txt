cmake_minimum_required(VERSION 3.16)
project(THERMATRIX VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ── Output directory ──────────────────────────────────────────────────────────
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/bin")

# ── Compiler flags ────────────────────────────────────────────────────────────
if(MSVC)
    add_compile_options(/O2 /W3 /openmp)
    # Static runtime for portability
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
else()
    add_compile_options(-O2 -Wall -Wextra -fopenmp)
    add_link_options(-fopenmp)
endif()

# ── External: GLFW (from system or subdirectory) ──────────────────────────────
set(GLFW_BUILD_DOCS     OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

# Try system GLFW first (MSYS2 provides it)
find_package(glfw3 QUIET)
if(NOT glfw3_FOUND)
    if(EXISTS "${CMAKE_SOURCE_DIR}/external/glfw/CMakeLists.txt")
        add_subdirectory(external/glfw)
    else()
        message(FATAL_ERROR "GLFW not found. Install via MSYS2 or place source in external/glfw/")
    endif()
endif()

# ── External: ImGui sources ───────────────────────────────────────────────────
set(IMGUI_DIR "${CMAKE_SOURCE_DIR}/external/imgui")
set(IMGUI_SOURCES
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_demo.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
    ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
)

# ── External: ImPlot sources ──────────────────────────────────────────────────
set(IMPLOT_DIR "${CMAKE_SOURCE_DIR}/external/implot")
set(IMPLOT_SOURCES
    ${IMPLOT_DIR}/implot.cpp
    ${IMPLOT_DIR}/implot_items.cpp
    ${IMPLOT_DIR}/implot_demo.cpp
)

# ── Core backend library ──────────────────────────────────────────────────────
set(CORE_SOURCES
    src/core/StlParser.cpp
    src/core/CsvParser.cpp
    src/core/FemSolver.cpp
    src/core/BackpropSolver.cpp
    src/core/BenchmarkEngine.cpp
    src/core/HardwareMock.cpp
)

add_library(thermatrix_core STATIC ${CORE_SOURCES})
target_include_directories(thermatrix_core PUBLIC
    ${CMAKE_SOURCE_DIR}/src
)
target_compile_features(thermatrix_core PUBLIC cxx_std_17)

if(NOT MSVC)
    target_compile_options(thermatrix_core PRIVATE -fopenmp)
endif()

# ── Thermal3D library (Phase 1+) ──────────────────────────────────────────────
set(T3D_SOURCES
    src/thermal3d/SparseCsr.cpp
    src/thermal3d/LinearSolvers.cpp
    src/thermal3d/SolverResult.cpp
)

add_library(thermatrix_t3d STATIC ${T3D_SOURCES})
target_include_directories(thermatrix_t3d PUBLIC
    ${CMAKE_SOURCE_DIR}/src
)
target_compile_features(thermatrix_t3d PUBLIC cxx_std_17)

if(NOT MSVC)
    target_compile_options(thermatrix_t3d PRIVATE -fopenmp)
endif()

# On Windows link psapi for GetProcessMemoryInfo
if(WIN32)
    target_link_libraries(thermatrix_t3d PUBLIC psapi)
endif()

# ── Main application ──────────────────────────────────────────────────────────
add_executable(THERMATRIX WIN32
    src/main.cpp
    src/app/App.cpp
    ${IMGUI_SOURCES}
    ${IMPLOT_SOURCES}
)

target_include_directories(THERMATRIX PRIVATE
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
    ${IMPLOT_DIR}
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(THERMATRIX PRIVATE
    thermatrix_core
    glfw
    opengl32
)

if(NOT MSVC)
    target_link_libraries(THERMATRIX PRIVATE gomp)
endif()

# ── Tests ─────────────────────────────────────────────────────────────────────
enable_testing()

function(add_thermatrix_test name src)
    add_executable(${name} ${src})
    target_include_directories(${name} PRIVATE ${CMAKE_SOURCE_DIR})
    target_link_libraries(${name} PRIVATE thermatrix_core)
    add_test(NAME ${name} COMMAND ${name})
endfunction()

add_thermatrix_test(test_stl_parser    tests/test_stl_parser.cpp)
add_thermatrix_test(test_csv_parser    tests/test_csv_parser.cpp)
add_thermatrix_test(test_fem_solver    tests/test_fem_solver.cpp)
add_thermatrix_test(test_backprop      tests/test_backprop.cpp)
add_thermatrix_test(test_benchmark     tests/test_benchmark.cpp)

# ── Thermal3D tests ───────────────────────────────────────────────────────────
add_executable(test_t3d_linalg tests/test_t3d_linalg.cpp)
target_include_directories(test_t3d_linalg PRIVATE ${CMAKE_SOURCE_DIR})
target_link_libraries(test_t3d_linalg PRIVATE thermatrix_t3d)
add_test(NAME test_t3d_linalg COMMAND test_t3d_linalg)

# ── Copy data files to binary dir ─────────────────────────────────────────────
add_custom_command(TARGET THERMATRIX POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/data"
        "$<TARGET_FILE_DIR:THERMATRIX>/data"
    COMMENT "Copying data files..."
)

# ── Install ───────────────────────────────────────────────────────────────────
install(TARGETS THERMATRIX RUNTIME DESTINATION bin)
install(DIRECTORY data DESTINATION .)
