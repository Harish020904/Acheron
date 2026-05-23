# ==============================================================================
# build_windows.ps1
# Automated Build Script for Project Acheron (Windows)
# ==============================================================================

Write-Host "========================================" -ForegroundColor Cyan
Write-Host " Project Acheron - Windows Build Script " -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

$ErrorActionPreference = "Stop"

$projectRoot = "$PSScriptRoot\.."
Set-Location $projectRoot

# ------------------------------------------------------------------------------
# 1. CMake Configure
# ------------------------------------------------------------------------------
Write-Host "Configuring CMake project..." -ForegroundColor Yellow

# Use the windows-release preset defined in CMakePresets.json
try {
    cmake --preset windows-release
} catch {
    Write-Host "ERROR: CMake configuration failed." -ForegroundColor Red
    Write-Host "Please ensure you have run setup_windows.ps1 first." -ForegroundColor Red
    exit 1
}

# ------------------------------------------------------------------------------
# 2. CMake Build
# ------------------------------------------------------------------------------
Write-Host "`nBuilding project..." -ForegroundColor Yellow

try {
    cmake --build build --config Release
} catch {
    Write-Host "ERROR: Build failed." -ForegroundColor Red
    exit 1
}

# ------------------------------------------------------------------------------
# 3. Final Validation
# ------------------------------------------------------------------------------
$exePath = "$projectRoot\build\bin\acheron.exe"
if (Test-Path $exePath) {
    Write-Host "`n========================================" -ForegroundColor Green
    Write-Host " Build Successful! " -ForegroundColor Green
    Write-Host " Executable located at: $exePath " -ForegroundColor Green
    Write-Host " Run it with: .\scripts\run_windows.ps1 " -ForegroundColor Cyan
    Write-Host "========================================" -ForegroundColor Green
} else {
    Write-Host "`nERROR: Build completed but executable was not found at expected path: $exePath" -ForegroundColor Red
    exit 1
}
