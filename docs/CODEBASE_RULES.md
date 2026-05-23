# Project Acheron — Codebase Rules

## 1. Purpose

These rules keep the project deterministic, explainable, and suitable for a university showcase.

## 2. Mandatory rules

- Keep hot systems allocation-light.
- Keep simulation deterministic.
- Prefer contiguous data and explicit ownership.
- Avoid runtime polymorphism in hot paths.
- Keep renderer state read-only with respect to simulation.

## 3. Memory rules

- Use allocators for owned lifetime boundaries.
- Keep transient data in frame or scratch storage.
- Keep hot data out of general-purpose heap churn.

## 4. Threading rules

- Use the job scheduler for ordered parallel work.
- Avoid shared mutable state without ownership.
- Prefer lock-free or phase-based synchronization.

## 5. Simulation rules

- Use fixed timestep updates.
- Avoid frame-rate dependence.
- Preserve replay consistency.

## 6. Output rules

- Generated binaries, objects, and temporary files belong under `build/`.
- Do not place generated files in source directories or the repository root.
