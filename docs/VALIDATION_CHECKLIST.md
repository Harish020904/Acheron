# VALIDATION_CHECKLIST.md

# Project Acheron — System Validation Checklist

## 1. Purpose

This document defines the mandatory engineering validation process for every subsystem in Acheron.

No subsystem is considered:

* complete
* mergeable
* stable
* production-ready

until it passes all required validation stages.

This document exists to prevent:

```text
architectural drift
performance regression
memory instability
thread contention
non-deterministic behavior
AI-generated code entropy
```

The validation pipeline is treated as:

```text
production infrastructure
```

NOT:

```text
optional cleanup work
```

---

# 2. Core Validation Philosophy

Every system must satisfy:

```text
correctness
→ determinism
→ memory stability
→ scalability
→ performance
→ maintainability
```

Never reverse this order.

---

# 3. Mandatory Validation Categories

Every subsystem must pass:

| Category                 | Required  |
| ------------------------ | --------- |
| Build Validation         | mandatory |
| Memory Validation        | mandatory |
| Thread Validation        | mandatory |
| Determinism Validation   | mandatory |
| Serialization Validation | mandatory |
| Profiling Validation     | mandatory |
| Architectural Validation | mandatory |

No exceptions.

---

# 4. Build Validation

## Requirements

Subsystem must:

* compile cleanly
* support Debug and Release
* support Clang and MSVC
* compile warning-free
* avoid undefined behavior

---

## Forbidden

```text
compiler warnings
undefined behavior
platform-specific hacks
incomplete includes
missing forward declarations
```

---

## Validation Checklist

| Validation                | Status   |
| ------------------------- | -------- |
| builds in Debug           | required |
| builds in Release         | required |
| builds with optimizations | required |
| no compiler warnings      | required |
| no linker errors          | required |

---

# 5. Memory Validation

## Core Rule

Hot-path systems must not allocate dynamically during runtime execution.

---

## Forbidden

```text
malloc/new inside frame loops
hidden STL allocations
heap churn
unbounded temporary vectors
runtime fragmentation
```

---

## Required

```text
arenas
chunk allocators
linear allocators
fixed pools
frame allocators
```

---

## Validation Tests

| Validation                  | Required |
| --------------------------- | -------- |
| no frame allocations        | yes      |
| allocator ownership tracked | yes      |
| no memory leaks             | yes      |
| stable memory lifetime      | yes      |
| fragmentation controlled    | yes      |

---

# 6. Cache Validation

## Objective

Ensure contiguous traversal and cache coherency.

---

## Required

```text
Struct-of-Arrays
aligned data
contiguous iteration
minimal pointer chasing
SIMD-friendly layouts
```

---

## Forbidden

```text
linked lists
deep object graphs
virtual traversal
scattered allocations
AoS hot loops
```

---

## Validation Metrics

| Metric               | Target    |
| -------------------- | --------- |
| L1 miss rate         | minimized |
| L2 miss rate         | minimized |
| branch misses        | minimized |
| contiguous traversal | required  |

---

# 7. Thread Safety Validation

## Core Rule

No unsafe shared mutable state.

---

## Required

```text
phase barriers
job ownership
thread-local scratch memory
lock-free queues where appropriate
explicit synchronization
```

---

## Forbidden

```text
race conditions
shared temporary allocators
global mutable state
unbounded mutex contention
blocking hot systems
```

---

## Validation Checklist

| Validation              | Required |
| ----------------------- | -------- |
| thread-safe traversal   | yes      |
| deterministic ordering  | yes      |
| no data races           | yes      |
| no deadlocks            | yes      |
| synchronization bounded | yes      |

---

# 8. Determinism Validation

## Core Rule

Same input must produce same simulation result.

---

## Forbidden

```text
frame-dependent simulation
unordered iteration
system clock logic
nondeterministic RNG
floating scheduling order
```

---

## Required

```text
fixed timestep
stable update order
deterministic event queues
seeded RNG
phase synchronization
```

---

## Validation Procedure

```text
Run identical simulation twice
→ hash world state
→ compare results
```

Mismatch = failure.

---

# 9. Serialization Validation

## Core Rule

All runtime state must serialize safely.

---

## Forbidden

```text
raw pointer serialization
virtual object serialization
platform-dependent layouts
unordered save structures
```

---

## Required

```text
stable IDs
versioned schemas
flat binary structures
deterministic ordering
```

---

## Validation Checklist

| Validation            | Required |
| --------------------- | -------- |
| save/load consistency | yes      |
| replay stability      | yes      |
| version compatibility | yes      |
| corruption handling   | yes      |

---

# 10. Profiling Validation

## Every subsystem must be measurable.

If a system cannot be profiled:

* it cannot be optimized
* it cannot be trusted

---

## Required Metrics

```text
execution time
memory usage
allocation count
cache pressure
job contention
stall frequency
```

---

## Required Tools

| Tool       | Purpose                  |
| ---------- | ------------------------ |
| Tracy      | frame profiling          |
| RenderDoc  | GPU debugging            |
| VTune      | CPU/cache analysis       |
| Sanitizers | memory/thread validation |

---

# 11. Performance Budget Validation

Subsystems must remain inside frame budgets.

Reference:

* frame_budget.md 
* memory_budget.md 

---

## Validation Categories

| Category        | Target    |
| --------------- | --------- |
| CPU time        | bounded   |
| memory          | bounded   |
| VRAM            | bounded   |
| allocations     | minimized |
| synchronization | bounded   |

---

# 12. ECS Validation

Reference:

* ecs_architecture.md 

---

## Required

```text
contiguous chunks
stable archetypes
SoA traversal
SIMD-safe layouts
deterministic migration
```

---

## Forbidden

```text
entity inheritance
virtual components
pointer-heavy systems
heap-owned entities
```

---

## ECS Checklist

| Validation                      | Required |
| ------------------------------- | -------- |
| archetype traversal contiguous  | yes      |
| no heap allocation in iteration | yes      |
| SIMD-compatible layouts         | yes      |
| stable entity handles           | yes      |

---

# 13. Scheduler Validation

Reference:

* simulation_scheduler.md 

---

## Required

```text
bounded synchronization
work stealing
phase barriers
deterministic scheduling
fiber-safe execution
```

---

## Forbidden

```text
unbounded mutex waits
recursive jobs
blocking worker threads
random execution order
```

---

## Scheduler Checklist

| Validation                   | Required |
| ---------------------------- | -------- |
| worker utilization stable    | yes      |
| no deadlocks                 | yes      |
| no starvation                | yes      |
| phase ordering deterministic | yes      |

---

# 14. Renderer Validation

Reference:

* renderer_pipeline.md 

---

## Required

```text
GPU-driven rendering
minimal draw calls
persistent mapped buffers
streaming-safe uploads
render graph scheduling
```

---

## Forbidden

```text
CPU-side scene graph traversal
per-object rendering
synchronous GPU stalls
runtime shader compilation
```

---

## Renderer Checklist

| Validation             | Required |
| ---------------------- | -------- |
| draw calls minimized   | yes      |
| GPU stalls minimized   | yes      |
| VRAM residency bounded | yes      |
| render graph valid     | yes      |

---

# 15. Memory System Validation

Reference:

* memory_systems.md 

---

## Required

```text
explicit ownership
allocator tracking
stable lifetime rules
frame reset systems
```

---

## Forbidden

```text
hidden ownership
allocator ambiguity
runtime fragmentation
mixed lifetimes
```

---

# 16. Asset Pipeline Validation

Reference:

* asset_pipeline.md 

---

## Required

```text
deterministic asset builds
compressed runtime assets
streaming metadata
version tracking
```

---

## Forbidden

```text
runtime raw asset parsing
unversioned exports
manual asset copying
```

---

# 17. Save System Validation

Reference:

* save_format.md 

---

## Required

```text
stable binary layouts
deterministic replay
version-safe saves
partial streaming recovery
```

---

## Forbidden

```text
pointer persistence
unordered serialization
save-time heap traversal
```

---

# 18. AI System Validation

Reference:

* ai_behaviors.md 

---

## Required

```text
multi-resolution updates
crowd-based logic
flow-field traversal
statistical simulation
```

---

## Forbidden

```text
per-agent expensive pathfinding
deep behavior trees everywhere
frame-rate-dependent AI
```

---

# 19. Gameplay Validation

References:

* combat_rules.md 
* progression_systems.md 

---

## Required

```text
deterministic combat
event-driven progression
stable state machines
simulation-safe gameplay
```

---

## Forbidden

```text
animation-driven gameplay authority
UI-owned gameplay state
nondeterministic progression
```

---

# 20. AI-Generated Code Validation

All AI-generated code must undergo:

```text
human architectural review
```

before merge.

---

## Mandatory Review Questions

### Memory

* does this allocate?
* who owns memory?
* is lifetime deterministic?

### Concurrency

* thread-safe?
* lock contention?
* deterministic ordering?

### ECS

* contiguous?
* SIMD-safe?
* archetype-friendly?

### Renderer

* async-safe?
* GPU-stall-safe?
* streaming-safe?

### Serialization

* replay-safe?
* version-safe?
* stable ordering?

---

# 21. Merge Gate Requirements

No subsystem merges unless:

| Validation                      | Required |
| ------------------------------- | -------- |
| builds                          | yes      |
| tests pass                      | yes      |
| profiling completed             | yes      |
| architecture review completed   | yes      |
| deterministic validation passes | yes      |

---

# 22. Regression Validation

Every major change requires:

```text
before/after profiling
```

Track:

* frame time
* allocations
* cache misses
* synchronization stalls
* VRAM pressure

Optimization without measurement is forbidden.

---

# 23. Failure Policy

If validation fails:

```text
STOP INTEGRATION
```

Do not:

* “fix later”
* “optimize later”
* “refactor later”

Architectural debt compounds exponentially.

---

# 24. Final Engineering Principle

Validation exists to guarantee:

```text
architectural integrity at scale
```

Acheron is not small enough to survive:

* loose engineering
* inconsistent ownership
* ad-hoc systems
* unconstrained AI generation

Every subsystem must remain:

```text
deterministic
cache-coherent
memory-stable
thread-safe
profilable
scalable
```

Or the architecture will collapse under scale.
