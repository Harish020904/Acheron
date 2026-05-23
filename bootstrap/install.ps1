<#
.SYNOPSIS
Project Acheron Windows Bootstrap Installer

.DESCRIPTION
This script serves as the single entry point for installing, building, and running
Project Acheron. It installs all required toolchains (VS Build Tools, CMake, Ninja, Vulkan SDK, vcpkg),
configures the environment, builds the engine, and launches it.

.EXAMPLE
irm https://raw.githubusercontent.com/YOUR_USERNAME/acheron/main/bootstrap/install.ps1 | iex
#>

$ErrorActionPreference = 'Stop'
$WarningPreference = 'Continue'

function Write-Step ($message) {
    Write-Host "`n[ACHERON] $message" -ForegroundColor Cyan
}

function Write-Info ($message) {
    Write-Host "  -> $message" -ForegroundColor Gray
}

function Write-Success ($message) {
    Write-Host "  [OK] $message" -ForegroundColor Green
}

function Write-ErrorAndExit ($message) {
    Write-Host "  [ERROR] $message" -ForegroundColor Red
    exit 1
}

# ==============================================================================
# STEP 1: Verify Admin Privileges
# ==============================================================================
$isAdmin = ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
if (-not $isAdmin) {
    Write-Host "This script requires Administrator privileges to install the toolchain." -ForegroundColor Red
    Write-Host "Relaunching as Administrator..." -ForegroundColor Yellow
    $myPath = $MyInvocation.MyCommand.Path
    if ($myPath) {
        Start-Process powershell.exe -Verb RunAs -ArgumentList "-NoProfile -ExecutionPolicy Bypass -File `"$myPath`""
    } else {
        # If run via iex, we can't reliably self-elevate the same web script without re-downloading
        Write-ErrorAndExit "Please open PowerShell as Administrator and run the command again."
    }
    exit 0
}

Write-Step "Initializing Acheron Bootstrap"

# ==============================================================================
# STEP 2: Validate System Requirements
# ==============================================================================
Write-Step "Validating System Requirements"

# RAM Check (Minimum 8GB, Recommended 16GB)
$computerInfo = Get-CimInstance Win32_OperatingSystem
$totalRAM_GB = [math]::Round($computerInfo.TotalVisibleMemorySize / 1024 / 1024)
Write-Info "Detected RAM: ${totalRAM_GB}GB"
if ($totalRAM_GB -lt 8) {
    Write-ErrorAndExit "Acheron requires at least 8GB of RAM to compile and run."
}

# Disk Space Check (Minimum 10GB free on C:)
$driveC = Get-CimInstance Win32_LogicalDisk -Filter "DeviceID='C:'"
$freeSpace_GB = [math]::Round($driveC.FreeSpace / 1GB)
Write-Info "Free Disk Space (C:): ${freeSpace_GB}GB"
if ($freeSpace_GB -lt 10) {
    Write-ErrorAndExit "Acheron requires at least 10GB of free space on C: for toolchains."
}

# GPU Check
$gpus = Get-CimInstance Win32_VideoController
foreach ($gpu in $gpus) {
    Write-Info "Detected GPU: $($gpu.Name)"
}

Write-Success "System meets minimum requirements."

# ==============================================================================
# STEP 3: Install Toolchain via Winget
# ==============================================================================
Write-Step "Installing Toolchain Dependencies"

$packages = @(
    @{ Id="Git.Git"; Name="Git" },
    @{ Id="Kitware.CMake"; Name="CMake" },
    @{ Id="Ninja-build.Ninja"; Name="Ninja" },
    @{ Id="KhronosGroup.VulkanSDK"; Name="Vulkan SDK" }
)

foreach ($pkg in $packages) {
    Write-Info "Checking $($pkg.Name)..."
    $check = winget list --id $($pkg.Id) --exact 2>$null
    if ($check -match $($pkg.Id)) {
        Write-Success "$($pkg.Name) is already installed."
    } else {
        Write-Info "Installing $($pkg.Name)..."
        winget install --id $($pkg.Id) --exact --accept-package-agreements --accept-source-agreements --silent
        if ($LASTEXITCODE -ne 0 -and $LASTEXITCODE -ne -1978335189) { # Ignore "already installed" exit codes
            Write-ErrorAndExit "Failed to install $($pkg.Name)"
        }
        Write-Success "Installed $($pkg.Name)."
    }
}

# Visual Studio Build Tools (Requires specific workloads)
Write-Info "Checking Visual Studio Build Tools..."
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vsInstalled = $false
if (Test-Path $vswhere) {
    $vsPath = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if (-not [string]::IsNullOrWhiteSpace($vsPath)) {
        $vsInstalled = $true
        Write-Success "Visual Studio Build Tools already installed at: $vsPath"
    }
}

if (-not $vsInstalled) {
    Write-Info "Installing Visual Studio Build Tools (This may take a few minutes)..."
    winget install Microsoft.VisualStudio.2022.BuildTools --override "--wait --quiet --add Microsoft.VisualStudio.Workload.VCTools --includeRecommended" --accept-package-agreements --accept-source-agreements
    Write-Success "Installed Visual Studio Build Tools."
}

# ==============================================================================
# STEP 4: Install vcpkg
# ==============================================================================
Write-Step "Configuring vcpkg"
$vcpkgDir = "C:\vcpkg"

if (-not (Test-Path "$vcpkgDir\vcpkg.exe")) {
    if (Test-Path $vcpkgDir) { Remove-Item -Recurse -Force $vcpkgDir }
    Write-Info "Cloning vcpkg to $vcpkgDir..."
    git clone https://github.com/microsoft/vcpkg.git $vcpkgDir
    
    Write-Info "Bootstrapping vcpkg..."
    Set-Location $vcpkgDir
    .\bootstrap-vcpkg.bat -disableMetrics
    
    Write-Info "Integrating vcpkg..."
    .\vcpkg.exe integrate install
    Write-Success "vcpkg configured."
} else {
    Write-Success "vcpkg already installed at $vcpkgDir."
}

[Environment]::SetEnvironmentVariable("VCPKG_ROOT", $vcpkgDir, "User")
$env:VCPKG_ROOT = $vcpkgDir

# ==============================================================================
# STEP 5: Clone or Update Acheron Repository
# ==============================================================================
Write-Step "Fetching Acheron Source Code"
$targetDir = "$env:USERPROFILE\Acheron"
$repoUrl = "https://github.com/Harish020904/Acheron.git" # Replace with actual if needed

if (Test-Path "$targetDir\.git") {
    Write-Info "Repository exists at $targetDir. Pulling latest changes..."
    Set-Location $targetDir
    git pull origin main
    Write-Success "Repository updated."
} else {
    Write-Info "Cloning Acheron to $targetDir..."
    git clone $repoUrl $targetDir
    Set-Location $targetDir
    Write-Success "Repository cloned."
}

# ==============================================================================
# STEP 6: Configure Developer Environment
# ==============================================================================
Write-Step "Configuring Developer Environment"

$vsPath = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$vcvars = "$vsPath\VC\Auxiliary\Build\vcvars64.bat"

if (-not (Test-Path $vcvars)) {
    Write-ErrorAndExit "Could not locate vcvars64.bat"
}

Write-Info "Loading MSVC environment variables..."
cmd.exe /c " `"$vcvars`" && set " | Foreach-Object {
    if ($_ -match "^(.*?)=(.*)$") {
        Set-Item -Force -Path "Env:\$($matches[1])" -Value $matches[2]
    }
}
Write-Success "Environment configured for MSVC x64."

# ==============================================================================
# STEP 7: Generate Build Files
# ==============================================================================
Write-Step "Generating Build Files"
Set-Location $targetDir

Write-Info "Running CMake generation (Ninja)..."
# We define CMAKE_TOOLCHAIN_FILE to hook vcpkg up, even if currently unused, 
# to support standard engine workflows.
cmake -B build -G Ninja -DCMAKE_TOOLCHAIN_FILE="$vcpkgDir\scripts\buildsystems\vcpkg.cmake" -DCMAKE_BUILD_TYPE=Release
if ($LASTEXITCODE -ne 0) {
    Write-ErrorAndExit "CMake generation failed."
}
Write-Success "Build files generated."

# ==============================================================================
# STEP 8: Build the Engine
# ==============================================================================
Write-Step "Building Acheron Engine"
cmake --build build --config Release
if ($LASTEXITCODE -ne 0) {
    Write-ErrorAndExit "Engine build failed."
}
Write-Success "Engine built successfully."

# ==============================================================================
# STEP 9: Stage Runtime Assets
# ==============================================================================
Write-Step "Staging Runtime Assets"
$binDir = "$targetDir\build\bin"

$assetsToCopy = @("assets", "configs", "shaders", "localization")
foreach ($asset in $assetsToCopy) {
    if (Test-Path "$targetDir\$asset") {
        Write-Info "Staging $asset..."
        Copy-Item -Path "$targetDir\$asset" -Destination "$binDir\$asset" -Recurse -Force
    }
}
Write-Success "Assets staged."

# ==============================================================================
# STEP 10: Run the Engine
# ==============================================================================
Write-Step "Launching Acheron..."
$exePath = "$binDir\acheron.exe"

if (Test-Path $exePath) {
    Set-Location $targetDir # Run from root to ensure relative paths work if not staged
    Start-Process -FilePath $exePath
    Write-Success "Acheron launched successfully!"
} else {
    Write-ErrorAndExit "Executable not found at $exePath."
}

Write-Host "`n========================================================" -ForegroundColor Cyan
Write-Host " Bootstrap Complete! Environment is fully configured." -ForegroundColor Cyan
Write-Host "========================================================`n" -ForegroundColor Cyan
