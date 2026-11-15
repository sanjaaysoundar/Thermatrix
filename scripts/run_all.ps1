# scripts/run_all.ps1
# Multi-Process Benchmark Runner for THERMATRIX 3D Thermal Solvers

$ErrorActionPreference = "Stop"

Write-Host "=============================================================================="
Write-Host " THERMATRIX 3D Thermal Solvers Multi-Process Benchmark Runner"
Write-Host " Isolated per-process execution for true high-water mark peak memory and timing"
Write-Host "=============================================================================="

$exe = "build/bin/run_t3d_benchmark.exe"
if (-not (Test-Path $exe)) {
    $exe = "bin/run_t3d_benchmark.exe"
}
if (-not (Test-Path $exe)) {
    Write-Error "Cannot find run_t3d_benchmark.exe in build/bin or bin"
    exit 1
}

New-Item -ItemType Directory -Force -Path "results" | Out-Null
New-Item -ItemType Directory -Force -Path "THERMATRIX_ASSIGNMENT_2/Results" | Out-Null

$mainCsv = "results/benchmark_results.csv"
$linCsv = "results/linear_solver_comparison.csv"

if (Test-Path $mainCsv) { Remove-Item $mainCsv }
if (Test-Path $linCsv) { Remove-Item $linCsv }

Write-Host "`n[STAGE 1] Running Main Benchmark (FEM, FDM, FVM x Coarse, Medium, Fine x Case A, Case B)"
Write-Host "Linear Solver: PCG-IC0, Tolerance: 1e-10, Repeats: 5 (+ 1 warmup)`n"

$cases = @("A", "B")
$levels = @("1", "2", "3")
$solvers = @("fem", "fdm", "fvm")

foreach ($c in $cases) {
    foreach ($l in $levels) {
        foreach ($s in $solvers) {
            Write-Host "Launching isolated process: $s level $l case $c..."
            & $exe --solver $s --level $l --case $c --repeats 5 --warmup 1 --linear_solver pcg-ic0 --out $mainCsv
            if ($LASTEXITCODE -ne 0) {
                Write-Error "Benchmark run failed for $s level $l case $c"
                exit 1
            }
        }
    }
}

Write-Host "`n[STAGE 2] Running Linear Solvers Comparison on Medium Level (Case A)"
Write-Host "Comparing PCG-IC0 vs PCG-Jacobi vs SSOR across FEM, FDM, FVM`n"

$methods = @("pcg-ic0", "pcg-jacobi", "ssor")

foreach ($s in $solvers) {
    foreach ($m in $methods) {
        Write-Host "Launching isolated process: $s level 2 case A linear_solver $m..."
        & $exe --solver $s --level 2 --case A --repeats 5 --warmup 1 --linear_solver $m --out $linCsv
        if ($LASTEXITCODE -ne 0) {
            Write-Error "Linear solver benchmark run failed for $s level 2 case A $m"
            exit 1
        }
    }
}

Write-Host "`n[STAGE 3] Syncing CSV deliverables to THERMATRIX_ASSIGNMENT_2/Results/"
Copy-Item -Force $mainCsv "THERMATRIX_ASSIGNMENT_2/Results/benchmark_results.csv"
Copy-Item -Force $linCsv "THERMATRIX_ASSIGNMENT_2/Results/linear_solver_comparison.csv"

Write-Host "`n=============================================================================="
Write-Host " Benchmark Complete!"
Write-Host " Main benchmark results: $mainCsv"
Write-Host " Linear solver comparison: $linCsv"
Write-Host "=============================================================================="
