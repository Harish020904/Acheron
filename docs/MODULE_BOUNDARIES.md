# Project Acheron — Module Boundaries

## Runtime layers

`core` → `memory` → `threading` → `scheduler` → `ecs` → `simulation` → `renderer`

## Rules

- ECS owns simulation state.
- Simulation writes deterministic state only.
- Renderer consumes immutable snapshots.
- Scheduler owns execution order, not game logic.
- Memory allocators provide fixed ownership and lifetime boundaries.

## Keep-list modules

- ECS
- memory allocators
- job scheduler
- simplified renderer
- traffic simulation
- economy simulation
- progression systems
- profiler
