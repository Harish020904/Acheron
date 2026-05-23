# Final Architecture Summary

Project Acheron was built entirely from scratch in C++20 as a deterministic city simulation showcase, deliberately avoiding enterprise engines like Unity or Unreal.

## 1. Entity Component System (ECS)
At the core of Acheron is a bespoke Generational ECS.
- **Memory Layout**: Entities are simply 64-bit IDs containing an index and a generation counter. Components (`DistrictComponent`, `RoadComponent`) are stored in tightly packed, pre-allocated Contiguous Arrays (`LinearAllocator`).
- **Data Locality**: This structure guarantees maximum CPU Cache Coherency during simulation iteration, entirely avoiding the overhead of Object-Oriented polymorphism or pointer-chasing.

## 2. Multi-Threaded Scheduler
The simulation loop avoids locking mutexes in favor of a `JobSystem`.
- **Lock-Free Queue**: Jobs are submitted to a lock-free queue and processed by a thread pool initialized to `hardware_concurrency()`.
- **Phase Barriers**: Heavy iteration loops (like `traffic` and `economy` updates) are chunked into batches, dispatched asynchronously, and synchronized via `wait_idle()` phase barriers before rendering occurs.

## 3. Fixed-Timestep Simulation
The `main.cpp` loop employs an accumulator-based fixed timestep (`FIXED_DT = 1.0f / 60.0f`). This guarantees the traffic flow, economic propagation, and instability calculations are 100% deterministic regardless of fluctuating frame rates or rendering bottlenecks.

## 4. Layered Renderer
Acheron uses Raylib for window management and OpenGL abstraction, wrapped in distinct C++ renderer layers:
- `SimpleRenderer`: Handles WASD/Middle-mouse camera panning, Zooming, and screen coordinate mapping.
- `TileRenderer`: Iterates the ECS for district blocks, rendering procedural neon glow effects based on District type.
- `RoadRenderer`: Interpolates dynamic vehicle "dots" along vectors based on ECS congestion metrics.
- `OverlayRenderer`: Handles cinematic ambient effects (scrolling haze, industrial smoke stacks).
- `UIRenderer`: Utilizes the Orbitron font to draw Cyberpunk-styled system dashboards and performance profiler graphs.

## 5. Serialization
The `WorldSerializer` can dump the entire state of the linear ECS arrays into a binary file (`save_state.bin`) instantly, and restore it natively into memory, allowing for perfect simulation snapshots without JSON overhead.
