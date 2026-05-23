# ==============================================================================
# run_windows.ps1
# Automated Run Script for Project Acheron (Windows)
# ==============================================================================

$ErrorActionPreference = "Stop"
$projectRoot = "$PSScriptRoot\.."
$exePath = "$projectRoot\build\bin\acheron.exe"

if (-Not (Test-Path $exePath)) {
    Write-Host "ERROR: Executable not found at $exePath" -ForegroundColor Red
    Write-Host "Please build the project first using: .\scripts\build_windows.ps1" -ForegroundColor Yellow
    exit 1
}

Write-Host "Starting Acheron Simulation..." -ForegroundColor Cyan

# Set the working directory to the project root so assets can be loaded correctly
Set-Location $projectRoot

# Execute the simulation
& $exePath
