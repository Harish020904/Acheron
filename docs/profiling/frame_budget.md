# frame_budget.md

# Project Acheron — Frame Budget Architecture

## 1. Purpose

The frame budget defines the hard performance constraints for the runtime simulation and rendering pipeline.

Acheron is fundamentally:

```text
memory-bandwidth bound
```

not:

```text
compute bound
```

The frame budget therefore exists to:

* prevent frame instability
* enforce deterministic scheduling
* maintain simulation throughput
* reduce cache thrashing
* control GPU submission pressure
* prevent runaway system scaling

Every subsystem must operate within explicit timing limits.

---

# 2. Target Frame Rates

Supported runtime targets:

| Mode              | Target   |
| ----------------- | -------- |
| Cinematic         | 30 FPS   |
| Standard          | 60 FPS   |
| High Refresh      | 120 FPS  |
| Simulation Stress | variable |

Primary engineering target:

```text
60 FPS stable
```

Frame time budget:

genui{"math_block_widget_always_prefetch_v2":{"content":"t = \frac{1000}{60}"}}

Target:

```text
16.67ms/frame
```

---

# 3. Core Budget Allocation

## 60 FPS Budget

| System         | Budget |
| -------------- | ------ |
| Simulation     | 4.0ms  |
| Rendering      | 5.0ms  |
| GPU Submission | 1.0ms  |
| Streaming      | 1.0ms  |
| Physics        | 2.0ms  |
| Audio          | 0.5ms  |
| UI             | 0.5ms  |
| Safety Margin  | 2.67ms |

The safety margin is mandatory.

Never allocate 100% of frame time.

---

# 4. Simulation Philosophy

The simulation layer must remain:

* cache-coherent
* SIMD-friendly
* deterministic
* multithreaded

Simulation cost scales with:

* population density
* traffic graph complexity
* economic propagation depth
* ECS entity count

Simulation must degrade gracefully under stress.

---

# 5. ECS Budget

Target:

| Metric              | Budget    |
| ------------------- | --------- |
| ECS iteration       | < 1.5ms   |
| Archetype migration | < 0.2ms   |
| Query execution     | < 0.5ms   |
| Entity destruction  | amortized |

ECS traversal must operate on:

```text
contiguous chunk memory
```

Avoid:

* pointer chasing
* virtual dispatch
* sparse heap allocations

---

# 6. Renderer Budget

Renderer target:

| Stage               | Budget  |
| ------------------- | ------- |
| Visibility culling  | 0.5ms   |
| Draw submission     | 1.0ms   |
| Lighting            | 1.5ms   |
| Post-processing     | 1.0ms   |
| UI rendering        | 0.5ms   |
| GPU synchronization | < 0.5ms |

Renderer bottlenecks:

```text
draw calls
state changes
VRAM bandwidth
GPU stalls
```

---

# 7. Draw Call Budget

Target:

| Platform     | Draw Calls |
| ------------ | ---------- |
| Mid-range PC | < 5000     |
| High-end PC  | < 10000    |
| Web          | < 2000     |

The renderer must aggressively use:

* instancing
* batching
* texture atlases
* bindless resources
* indirect draws

---

# 8. GPU Budget

GPU target:

| Resolution | Target   |
| ---------- | -------- |
| 1080p      | 60 FPS   |
| 1440p      | 60 FPS   |
| 4K         | optional |

GPU pressure sources:

* overdraw
* transparency
* shadow maps
* volumetrics
* post-processing

Avoid excessive fullscreen passes.

---

# 9. Physics Budget

Physics should remain lightweight.

Target:

```text
< 2ms/frame
```

Physics scope:

* broadphase collision
* lightweight constraints
* traffic interactions
* environmental overlap checks

Avoid:

* high-frequency rigid-body simulation
* unnecessary contact resolution
* expensive solver iterations

---

# 10. Traffic Simulation Budget

Traffic simulation target:

```text
< 1.5ms/frame
```

Traffic systems should use:

* density fields
* flow maps
* aggregated lane simulation

Avoid per-vehicle expensive pathfinding.

---

# 11. Economy Simulation Budget

Economy systems update at lower frequencies.

Target:

```text
< 0.5ms average
```

Recommended update rate:

```text
1–2Hz
```

Economy propagation must operate on:

* contiguous arrays
* graph batches
* SIMD-compatible structures

---

# 12. Streaming Budget

Streaming target:

```text
< 1ms/frame average
```

Streaming tasks:

* texture residency
* mesh loading
* district streaming
* audio streaming

Streaming must remain asynchronous.

Never block the render thread.

---

# 13. Asset Loading Rules

No runtime stalls.

All runtime loading must:

* use async IO
* prefetch assets
* stage uploads
* avoid blocking GPU synchronization

Target:

```text
0 frame hitches during streaming
```

---

# 14. Memory Access Budget

Primary performance bottleneck:

```text
cache misses
```

Not FLOPS.

All systems must optimize for:

* sequential traversal
* contiguous memory
* branch predictability
* SIMD alignment

---

# 15. Cache Targets

Recommended targets:

| Cache             | Goal  |
| ----------------- | ----- |
| L1 hit rate       | > 90% |
| L2 hit rate       | > 80% |
| Branch prediction | > 95% |

Avoid:

* fragmented heaps
* random memory access
* linked-list traversal

---

# 16. Job System Budget

Task scheduler target:

```text
< 0.3ms overhead
```

Scheduler requirements:

* lock-free queues
* work stealing
* fixed worker threads
* minimal synchronization

Avoid:

* thread creation during runtime
* blocking mutexes
* global task queues

---

# 17. Synchronization Constraints

Synchronization must remain minimal.

Allowed:

* atomic counters
* phase barriers
* double buffering
* lock-free queues

Avoid:

* coarse locks
* long critical sections
* render-thread stalls

---

# 18. Frame Pacing

Stable pacing is more important than peak FPS.

Target:

```text
consistent frametimes
```

Avoid:

* microstutter
* inconsistent simulation delta
* periodic streaming spikes

---

# 19. Frame Timing Strategy

Runtime order:

```text
Input
→ Simulation
→ ECS Updates
→ Physics
→ Rendering
→ GPU Submission
→ Audio
→ Present
```

Each stage is independently profiled.

---

# 20. Profiling Requirements

Every subsystem must expose:

* CPU timings
* allocation counts
* synchronization stalls
* cache pressure
* job execution timing

Profiling is mandatory.

---

# 21. Runtime Telemetry

Track:

* frametime spikes
* memory spikes
* GPU stalls
* streaming latency
* ECS chunk pressure
* pathfinding spikes

Telemetry drives optimization priorities.

---

# 22. Worst-Case Simulation Targets

Stress test target:

| Metric           | Goal       |
| ---------------- | ---------- |
| Entities         | 1,000,000+ |
| Vehicles         | 100,000+   |
| Active districts | 32+        |
| Concurrent jobs  | 10,000+    |

The engine must degrade gracefully.

---

# 23. Graceful Degradation Strategy

Under heavy load:

Reduce:

* simulation frequency
* crowd density
* shadow quality
* traffic update rate
* distant AI processing

Never freeze the simulation.

---

# 24. GPU Synchronization Rules

Never force GPU waits unless unavoidable.

Avoid:

* readbacks
* immediate synchronization
* excessive fences

Prefer:

* async compute
* persistent mapped buffers
* multi-frame buffering

---

# 25. Threading Model

Recommended worker count:

genui{"math_block_widget_always_prefetch_v2":{"content":"workers = cores - 1"}}

One thread reserved for:

```text
render submission
```

Simulation workers should remain CPU-bound.

---

# 26. Frame Capture Requirements

Performance captures must include:

* CPU timings
* GPU timings
* memory snapshots
* ECS archetype pressure
* job execution traces
* cache miss analysis

Use continuous capture during stress testing.

---

# 27. Optimization Priority Order

Correct optimization order:

```text
1. Memory layout
2. Cache coherency
3. Branch reduction
4. SIMD utilization
5. Parallelization
6. Algorithm refinement
```

Premature threading does not solve poor memory layout.

---

# 28. Engineering Principle

Acheron is fundamentally constrained by:

```text
memory movement
```

not raw arithmetic throughput.

The frame budget therefore exists to enforce:

* deterministic scheduling
* stable frametimes
* bounded synchronization
* cache-efficient execution
* scalable simulation throughput

Every subsystem must justify its frame cost.
