Restructuring summary for Acheron (university showcase)

1) Files/Folders moved to archive (to delete or keep externally)
- tools/asset_pipeline/*
- tools/profiling/*
- src/tools_runtime/*
- src/tools/*
- src/engine/renderer/render_graph.*
- src/engine/renderer/gpu_allocator.*
- src/engine/renderer/shader_manager.*
- assets/shaders/post_process/*
- assets/shaders/compute/*
- tests/performance/*
- docs/pipelines/*
- docs/restructuring/*
- Build artifacts cleaned under build/ (old contents archived or removed)

These were moved to: build\temp\archived_removed

2) Files/Folders to keep (core simulation & showcase)
- CMakeLists.txt, CMakePresets.json, README.md, LICENSE, CHANGELOG.md, version.json, vcpkg.json
- src/core/*
- src/ecs/* and src/ecs/memory/*
- src/simulation/{scheduler,traffic,economy,navigation}/*
- src/gameplay/* (progression, world)
- src/engine/renderer/renderer.* (simplified)
- src/engine/renderer/texture_manager.* and mesh_manager.* (if used)
- src/utilities/memory/*
- src/utilities/profiler.*
- src/utilities/thread_pool.* and threading/*
- assets/prefabs/*, assets/shaders/sprite/*, assets/scenes/*
- configs/gameplay/* and configs/engine/* (trimmed)
- docs/architecture/*, docs/gameplay/*, docs/PLAN.md
- tests/unit/*

3) Simplified final directory structure
/
- CMakeLists.txt, CMakePresets.json, README.md, LICENSE, vcpkg.json
- src/
  - core/
  - ecs/
    - memory/
  - simulation/
    - scheduler/
    - traffic/
    - economy/
    - navigation/
  - engine/
    - renderer/
  - gameplay/
    - progression/
    - world/
  - utilities/
  - threading/
- assets/
  - prefabs/
  - shaders/
    - sprite/
  - scenes/
- configs/
- docs/
  - architecture/
  - gameplay/
  - PLAN.md
- build/
  - bin/
  - obj/
  - temp/

4) Updated CMake/build structure (what changed)
- All target outputs forced into build/bin (runtime) and build/obj (archives/libraries). PDBs and temp files in build/temp.
- New CMake options to disable heavy subsystems by default:
  - BUILD_ASSET_PIPELINE=OFF
  - BUILD_STREAMING=OFF
  - BUILD_GPU_ALLOCATOR=OFF
  - BUILD_RENDER_GRAPH=OFF
- CMakePresets.json updated to configure into build/ and disable heavy subsystems by default.
- Build instructions in README updated to use cmake -S . -B build or cmake --preset default.

5) Why removed systems are unnecessary for a university showcase
- Asset pipeline: production tooling that increases repo complexity; prepack assets instead and keep a minimal importer.
- Profiling/metrics tools: external parsers and heavy tooling add noise; keep an in-engine simplified profiler to demonstrate timing and memory.
- Runtime dev tools (replay, overlays, ECS debugger): useful in development but not required to demonstrate deterministic simulation; removing simplifies code surface.
- Advanced renderer internals (render-graph, GPU allocator, shader reflection): high complexity targeted at production engines; a simple renderer suffices to visualize simulation for a demo.
- Post-process/compute shaders: visual fidelity extras not required to show systems programming, determinism, or urban simulation concepts.
- Streaming, packaging, CI, deployment docs: out of scope for a university project; removing clarifies objectives and reduces maintenance.

Notes
- All removed source files were moved to build\temp\archived_removed for safe rollback. If permanent deletion is desired, remove that archive before final submission.
- After review, run: cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build --config Release

