# Project Acheron — Implementation Order

## 1. Build order

1. Core runtime
2. Memory allocators
3. Threading and scheduler
4. ECS
5. Simplified renderer
6. Traffic simulation
7. Economy simulation
8. Progression systems
9. Profiler and demo polish

## 2. Rule

Each layer must be understandable on its own before the next layer is added.
