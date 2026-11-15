@echo off
setlocal enabledelayedexpansion

echo ==============================================================================
echo  THERMATRIX 3D Thermal Solvers Multi-Process Benchmark Runner
echo  Isolated per-process execution for true high-water mark peak memory and timing
echo ==============================================================================

:: Find executable
set EXE=build\bin\run_t3d_benchmark.exe
if not exist "%EXE%" (
    set EXE=bin\run_t3d_benchmark.exe
)
if not exist "%EXE%" (
    echo [ERROR] Cannot find run_t3d_benchmark.exe in build\bin\ or bin\
    exit /b 1
)

:: Prepare output directories
if not exist "results" mkdir results
if not exist "THERMATRIX_ASSIGNMENT_2\Results" mkdir THERMATRIX_ASSIGNMENT_2\Results

set MAIN_CSV=results\benchmark_results.csv
set LIN_CSV=results\linear_solver_comparison.csv

if exist "%MAIN_CSV%" del /f /q "%MAIN_CSV%"
if exist "%LIN_CSV%" del /f /q "%LIN_CSV%"

echo.
echo [STAGE 1] Running Main Benchmark (FEM, FDM, FVM x Coarse, Medium, Fine x Case A, Case B)
echo Linear Solver: PCG-IC0, Tolerance: 1e-10, Repeats: 5 (+ 1 warmup)
echo.

for %%c in (A B) do (
    for %%l in (1 2 3) do (
        for %%s in (fem fdm fvm) do (
            echo Launching isolated process: %%s level %%l case %%c...
            "%EXE%" --solver %%s --level %%l --case %%c --repeats 5 --warmup 1 --linear_solver pcg-ic0 --out "%MAIN_CSV%"
            if errorlevel 1 (
                echo [ERROR] Run failed for %%s level %%l case %%c
                exit /b 1
            )
        )
    )
)

echo.
echo [STAGE 2] Running Linear Solvers Comparison on Medium Level (Case A)
echo Comparing PCG-IC0 vs PCG-Jacobi vs SSOR across FEM, FDM, FVM
echo.

for %%s in (fem fdm fvm) do (
    for %%m in (pcg-ic0 pcg-jacobi ssor) do (
        echo Launching isolated process: %%s level 2 case A linear_solver %%m...
        "%EXE%" --solver %%s --level 2 --case A --repeats 5 --warmup 1 --linear_solver %%m --out "%LIN_CSV%"
        if errorlevel 1 (
            echo [ERROR] Run failed for %%s level 2 case A %%m
            exit /b 1
        )
    )
)

echo.
echo [STAGE 3] Syncing CSV deliverables to THERMATRIX_ASSIGNMENT_2\Results\
copy /Y "%MAIN_CSV%" "THERMATRIX_ASSIGNMENT_2\Results\benchmark_results.csv" >nul
copy /Y "%LIN_CSV%" "THERMATRIX_ASSIGNMENT_2\Results\linear_solver_comparison.csv" >nul

echo.
echo ==============================================================================
echo  Benchmark Complete!
echo  Main benchmark results: %MAIN_CSV%
echo  Linear solver comparison: %LIN_CSV%
echo ==============================================================================
