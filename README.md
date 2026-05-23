# Acheron

> **A systems programming and deterministic urban simulation showcase built in modern C++23.**

Acheron is a deterministic urban simulation sandbox focused on ECS, memory systems, scheduling, and simulation architecture. It visualizes the flow of traffic, economy, and district activity in real time, serving as a comprehensive technical showcase of modern C++ systems programming.

![Acheron Simulation](docs/screenshot.png)

---

## 1. Project Overview

Acheron is a high-performance, top-down city simulation set in a scaled representation of Melbourne, Australia. 

**Non-Technical Explanation:**  
"A city simulation where traffic, economy, and district growth interact in real time."

**Technical Context:**  
The project exists to demonstrate robust engine architecture without relying on commercial game engines like Unity or Unreal. Everything from memory allocation to entity management, parallel task scheduling, and state determinism is built from scratch in pure modern C++23.

---

## 2. Technical Highlights

- **ECS Architecture:** A strict Entity-Component-System design that enforces data-oriented design (SoA) for optimal cache coherency.
- **Deterministic Simulation:** Fixed-timestep updates ensure reproducible simulation states across runs.
- **Memory Allocators:** Custom linear, pool, and frame allocators bypass standard library overhead to eliminate fragmentation.
- **Job Scheduler:** A lock-free, work-stealing multithreaded task graph ensures all CPU cores are effectively utilized.
- **Traffic Simulation:** A continuum-flow density model handles congestion propagation across road networks.
- **Economy Propagation:** Real-time production, trade routes, and capital flow algorithms between interacting districts.
- **Profiler Systems:** In-engine, real-time profiling overlays to monitor frame times, memory usage, and thread activity.

---

## 3. Quick Install (Bootstrap)

Acheron uses a professional engine bootstrap system. A single command will automatically install the necessary toolchains (CMake, Ninja, Vulkan SDK, vcpkg, Visual Studio Build Tools/Xcode CLT), clone the repository, build the engine, and run it.

### Windows (PowerShell)
Open PowerShell **as Administrator** and run:
```powershell
Set-ExecutionPolicy Bypass -Scope Process -Force
irm https://raw.githubusercontent.com/Harish020904/Acheron/main/bootstrap/install.ps1 | iex
```

### macOS (Terminal)
Open your terminal and run:
```bash
curl -fsSL https://raw.githubusercontent.com/Harish020904/Acheron/main/bootstrap/install.sh | bash
```

> **Note:** The bootstrap scripts will automatically check for minimum system requirements (8GB+ RAM, 10GB+ Free Disk Space) before proceeding.

---

## 7. Controls

| Input | Action |
|-------|--------|
| `SPACE` | Pause / resume simulation |
| `Q` / `E` | Decrease / increase simulation speed |
| `WASD` / Middle Mouse | Pan camera |
| `Scroll Wheel` | Zoom in/out |
| `G` | Enter Ground Exploration mode (character movement) |
| `F1` - `F4` | Toggle overlays (Traffic, Economy, Density, Stability) |
| `F5` / `F9` | Save / Load game state |
| `F6` / `TAB` | Toggle profiler and debug HUD |
| `T` | Trigger city event (congestion/collapse testing) |
| `L` | Locate nearest citizen / spawn point |
| `ESC` | Deselect / cancel |

---

## 8. Project Structure

- `src/` - Core engine and application code
- `src/ecs/` - Entity-Component-System implementation (Archetypes, Queries)
- `src/simulation/` - Traffic, Economy, and Citizen simulation logic
- `src/threading/` - Lock-free queues, Job Scheduler
- `src/engine/renderer/` - 2D Rendering backend and overlay drawing
- `src/utilities/` - Math, JSON, File System, Memory Allocators
- `docs/` - Documentation and screenshots
- `configs/` - JSON configuration files for engine settings
- `build/` - Output directory for object files and executables
- `scripts/` - Automated setup, build, and run scripts

---

## 9. Learning Outcomes

This project was built to demonstrate deep systems programming concepts:
- **ECS Architecture:** Moving away from OOP to achieve massive performance gains through decoupled logic and data.
- **Memory-Oriented Programming:** Eliminating `new`/`delete` in the main loop to prevent heap fragmentation.
- **Multithreading:** Safely scaling a deterministic simulation across multiple cores using a lock-free work-stealing scheduler.
- **Deterministic Simulation:** Separating logic ticks from render frames to ensure reliable simulation state reproduction.
- **Data-Oriented Design:** Organizing data in Struct-of-Arrays (SoA) format for optimal CPU cache utilization.

---

## 10. Performance Notes

- **Fixed Timestep:** The simulation logic ticks at a constant 60Hz, independent of the rendering framerate.
- **Cache-Friendly Layouts:** Component data is stored contiguously in memory arrays.
- **SoA Storage:** Data structures prioritize CPU cache lines, reducing cache misses during system iterations.
- **Deterministic Updates:** Floating-point determinism rules are respected to ensure identical outcomes from identical seeds.
- **Profiling Systems:** The `F6` profiler tracks microsecond-level execution times for every active Job System phase.

---

## 11. Troubleshooting

- **Compiler not found:** 
  - *Windows:* Ensure Visual Studio 2022 Desktop C++ workload is installed.
  - *macOS:* Run `xcode-select --install`.
- **CMake not found:** 
  - Install CMake via `winget install Kitware.CMake` (Windows) or `brew install cmake` (macOS). Ensure it is in your system PATH.
- **SDL2/raylib missing:** 
  - The project uses CMake `FetchContent` to download raylib automatically. Ensure you have an active internet connection on the first build.
- **Executable not generated:** 
  - Check the output of the build script for syntax or linking errors. Run `cmake --build build --clean-first`.
- **Permission issues:** 
  - *macOS:* Ensure scripts are executable (`chmod +x scripts/*.sh`).
- **macOS unsigned executable issue (Gatekeeper):** 
  - If macOS blocks the binary, run `xattr -d com.apple.quarantine ./build/bin/acheron` or allow it via System Settings -> Privacy & Security.

---

## 12. Future Improvements

- **Larger Cities:** Expanding beyond the CBD to simulate the entire Greater Melbourne metropolitan area.
- **GPU Simulation:** Offloading traffic continuum calculations to Compute Shaders (Vulkan/OpenGL).
- **Weather Systems:** Dynamic weather that impacts traffic flow, citizen mood, and district stability.
- **Advanced AI:** More complex state machines for citizens allowing for varied routines and emergency responses.
- **Streaming:** Chunk-based data streaming to support near-infinite map sizes without exhausting RAM.

---

## 13. License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

---

## 14. Final Summary

**Project Acheron is a systems programming and deterministic urban simulation showcase built in modern C++23.** It represents a rigorous exploration of game engine architecture, proving that massive parallel simulations can be achieved cleanly and efficiently through proper memory management and data-oriented design.
