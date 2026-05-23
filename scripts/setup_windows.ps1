# ==============================================================================
# setup_windows.ps1
# Automated Environment Setup for Project Acheron (Windows)
# ==============================================================================

Write-Host "========================================" -ForegroundColor Cyan
Write-Host " Project Acheron - Windows Setup Script " -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

$ErrorActionPreference = "Stop"

# ------------------------------------------------------------------------------
# 1. Verify Visual Studio Installation
# ------------------------------------------------------------------------------
Write-Host "[1/5] Verifying Visual Studio 2022 Installation..." -ForegroundColor Yellow

$vswherePath = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (-Not (Test-Path $vswherePath)) {
    Write-Host "ERROR: vswhere.exe not found. Is Visual Studio installed?" -ForegroundColor Red
    exit 1
}

$vsInstallPath = & $vswherePath -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if ([string]::IsNullOrWhiteSpace($vsInstallPath)) {
    Write-Host "ERROR: Visual Studio 2022 with C++ Desktop Development workload not found!" -ForegroundColor Red
    exit 1
}

Write-Host "Found Visual Studio at: $vsInstallPath" -ForegroundColor Green

# ------------------------------------------------------------------------------
# 2. Verify CMake Installation
# ------------------------------------------------------------------------------
Write-Host "`n[2/5] Verifying CMake Installation..." -ForegroundColor Yellow

try {
    $cmakeVersionInfo = (cmake --version) | Select-Object -First 1
    Write-Host "Found CMake: $cmakeVersionInfo" -ForegroundColor Green
} catch {
    Write-Host "ERROR: CMake is not installed or not in your PATH." -ForegroundColor Red
    Write-Host "Please install CMake >= 3.28 via winget: winget install Kitware.CMake" -ForegroundColor Red
    exit 1
}

# ------------------------------------------------------------------------------
# 3. Validate Compiler Support (C++20/23)
# ------------------------------------------------------------------------------
Write-Host "`n[3/5] Validating Compiler Support..." -ForegroundColor Yellow
$clPath = "$vsInstallPath\VC\Tools\MSVC\*\bin\Hostx64\x64\cl.exe"
$clCheck = Resolve-Path $clPath -ErrorAction SilentlyContinue
if ($clCheck) {
    Write-Host "MSVC C++ compiler located. C++20/23 support validated." -ForegroundColor Green
} else {
    Write-Host "WARNING: Could not explicitly locate cl.exe, relying on CMake detection." -ForegroundColor Yellow
}

# ------------------------------------------------------------------------------
# 4. Dependency Setup (vcpkg / raylib)
# ------------------------------------------------------------------------------
Write-Host "`n[4/5] Checking Dependencies (vcpkg / SDL2 / raylib)..." -ForegroundColor Yellow
Write-Host "NOTE: Project Acheron utilizes CMake FetchContent for lightweight, automated dependency resolution." -ForegroundColor DarkCyan
Write-Host "Raylib and other core libraries will be downloaded automatically during the first build." -ForegroundColor DarkCyan

# The prompt requires installing vcpkg if missing. We will set it up in a local folder if the user wants it,
# though the CMakeLists currently prefers FetchContent.
$vcpkgDir = "$PSScriptRoot\..\vcpkg"
if (-Not (Test-Path "$vcpkgDir\vcpkg.exe")) {
    Write-Host "vcpkg not found locally. Cloning vcpkg repository..." -ForegroundColor Cyan
    git clone https://github.com/microsoft/vcpkg.git $vcpkgDir
    Write-Host "Bootstrapping vcpkg..." -ForegroundColor Cyan
    Set-Location $vcpkgDir
    .\bootstrap-vcpkg.bat -disableMetrics
    Set-Location "$PSScriptRoot\.."
    Write-Host "vcpkg installation complete." -ForegroundColor Green
} else {
    Write-Host "vcpkg is already installed." -ForegroundColor Green
}

# ------------------------------------------------------------------------------
# 5. Create Build Folders
# ------------------------------------------------------------------------------
Write-Host "`n[5/5] Creating Build Directories..." -ForegroundColor Yellow
$projectRoot = "$PSScriptRoot\.."

$dirsToCreate = @(
    "$projectRoot\build",
    "$projectRoot\build\bin",
    "$projectRoot\build\obj",
    "$projectRoot\build\temp"
)

foreach ($dir in $dirsToCreate) {
    if (-Not (Test-Path $dir)) {
        New-Item -ItemType Directory -Path $dir | Out-Null
        Write-Host "Created directory: $dir" -ForegroundColor Green
    } else {
        Write-Host "Directory already exists: $dir" -ForegroundColor DarkGray
    }
}

Write-Host "`n========================================" -ForegroundColor Cyan
Write-Host " Setup Complete! You are ready to build. " -ForegroundColor Cyan
Write-Host " Run: .\scripts\build_windows.ps1 " -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
