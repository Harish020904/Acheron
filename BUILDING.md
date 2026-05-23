# Building Project Acheron — Detailed Guide

## Table of Contents

1. [Windows (MinGW + Ninja)](#windows-mingw--ninja)
2. [Windows (MSVC + Visual Studio)](#windows-msvc--visual-studio)
3. [macOS (Xcode Command Line Tools)](#macos-xcode-command-line-tools)
4. [Dependency Notes](#dependency-notes)
5. [Build Troubleshooting](#build-troubleshooting)
6. [IDE Integration](#ide-integration)

---

## Windows (MinGW + Ninja) ← Recommended for Windows

This is the setup used during development (MinGW-w64 GCC 15 + Ninja).

### Step 1 — Install tools

**Option A: winget (fastest)**
```powershell
winget install Kitware.CMake
winget install Ninja-build.Ninja
winget install MSYS2.MSYS2
```

After MSYS2 installs, open **MSYS2 UCRT64** terminal and run:
```bash
pacman -S mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-ninja
```

Add `C:\msys64\ucrt64\bin` to your Windows PATH.

**Option B: Download directly**
- CMake: https://cmake.org/download/
- Ninja: https://ninja-build.org/ (single `.exe`, put it in PATH)
- MinGW-w64: https://www.mingw-w64.org/downloads/ (MSUCRT variant recommended)

### Step 2 — Clone and build

```powershell
git clone https://github.com/your-repo/acheron.git
cd acheron
cmake --preset windows-release
cmake --build build --config Release
```

First build downloads raylib (~5 MB) automatically. Subsequent builds are fast (incremental).

### Step 3 — Run

```powershell
.\build\bin\acheron.exe
```

---

## Windows (MSVC + Visual Studio)

### Step 1 — Install Visual Studio 2022

Install **Visual Studio 2022** (Community edition is free) with:
- ✅ "Desktop development with C++" workload
- ✅ CMake tools for Visual Studio (included in the C++ workload)

Also install **CMake** separately if `cmake` isn't in your PATH.

### Step 2 — Open Developer Command Prompt

Use the `x64 Native Tools Command Prompt for VS 2022` from Start menu.

```bat
cd acheron
cmake --preset windows-release
cmake --build build --config Release
```

**Or using Visual Studio's built-in CMake support:**

1. Open Visual Studio → File → Open → Folder → select the `acheron/` directory
2. Visual Studio detects `CMakeLists.txt` automatically
3. Select preset `windows-release` from the toolbar
4. Build → Build All

### Step 3 — Run

```bat
build\bin\acheron.exe
```

---

## macOS (Xcode Command Line Tools)

### Step 1 — Install Xcode Command Line Tools

```sh
xcode-select --install
```

This installs AppleClang, `make`, and other required tools. No full Xcode needed.

### Step 2 — Install CMake

**Option A: Homebrew (recommended)**
```sh
brew install cmake ninja
```

**Option B: Official download**
Download from https://cmake.org/download/ and add to PATH:
```sh
export PATH="/Applications/CMake.app/Contents/bin:$PATH"
```

### Step 3 — Clone and build

```sh
git clone https://github.com/your-repo/acheron.git
cd acheron

# With Ninja (faster):
cmake --preset macos-ninja-release
cmake --build build

# With Make (no Ninja needed):
cmake --preset macos-release
cmake --build build
```

### Step 4 — Run

```sh
./build/bin/acheron
```

> **Note:** On macOS, the first time you run the binary, you may see a security warning.
> Go to **System Settings → Privacy & Security** and click "Allow Anyway" if prompted.

---

## Dependency Notes

**All dependencies are automatically downloaded by CMake.**

No manual `apt install`, `brew install`, or `vcpkg install` is needed.

| Dependency | Version | How obtained |
|------------|---------|--------------|
| [raylib](https://github.com/raysan5/raylib) | 5.5 | CMake FetchContent (downloaded on first build) |

raylib provides:
- Cross-platform window management
- OpenGL 3.3 rendering
- Input handling (keyboard, mouse)
- Audio (miniaudio backend)
- 2D drawing primitives

The download requires an internet connection **only on the first build**.
After that, the source is cached in `build/_deps/raylib-src/`.

---

## Build Troubleshooting

### "cmake: command not found" / "cmake is not recognized"
→ CMake is not in PATH. Add the CMake `bin/` directory to your system PATH.

### "ninja: command not found"
→ Either install Ninja, or use a Make-based preset (`macos-release` uses `Unix Makefiles`).

### "No CMAKE_CXX_COMPILER could be found"
→ On Windows: ensure MinGW or MSVC is installed and in PATH.
→ On macOS: run `xcode-select --install`.

### FetchContent download fails (network error)
→ TLS verification is disabled by default to support MinGW environments without
root CA bundles. If you still see download failures, check your firewall/proxy.
To *enforce* TLS verification (recommended on corporate networks with proper CA setup):
```sh
cmake --preset windows-release -DACHERON_ENABLE_TLS_VERIFY=ON
```
Or manually download raylib 5.5.tar.gz and set `FETCHCONTENT_SOURCE_DIR_RAYLIB`.

### Build fails with "Permission denied" on acheron.exe (Windows)
→ The old executable is still running. Close the application window first, then rebuild.

### "raylib not found" / linking errors
→ Delete the `build/` directory and re-run from Step 2 (clean build):
```sh
Remove-Item -Recurse -Force build   # PowerShell
rm -rf build                        # macOS/bash
```

### macOS: "dylib not found" at runtime
→ Acheron links raylib statically, so no `.dylib` files are needed at runtime.
If you see this error, it may be from a system library. Check macOS version (11.0+ required).

### macOS: Gatekeeper blocks execution
→ Run `xattr -d com.apple.quarantine ./build/bin/acheron` in terminal,
or allow it via System Settings → Privacy & Security.

---

## IDE Integration

### Visual Studio Code

1. Install extensions:
   - **CMake Tools** (ms-vscode.cmake-tools)
   - **C/C++** (ms-vscode.cpptools)

2. Open the `acheron/` folder in VS Code

3. Press `Ctrl+Shift+P` → **CMake: Select Configure Preset** → choose your platform preset

4. Press `F7` to build, `F5` to debug

### CLion (JetBrains)

1. Open → select `acheron/` directory
2. CLion detects CMakePresets.json automatically
3. Select preset from the toolbar
4. Build and run normally

### Xcode (macOS only)

```sh
cmake -B build-xcode -G Xcode -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0
open build-xcode/Acheron.xcodeproj
```

---

## Regenerating Melbourne Map Data

The road network is baked into `assets/data/melbourne_map_data.h` (5,734 nodes).
To refresh from OpenStreetMap (requires internet):

```sh
# Fetch fresh data
python tools/save_osm_cache.py

# Regenerate the C++ header
python tools/process_melbourne_osm.py
```

Then rebuild the project — the new header is picked up automatically.

---

## Running Unit Tests

```sh
cmake --preset windows-release   # or macos-release
cmake --build build
cd build
ctest --output-on-failure
```
