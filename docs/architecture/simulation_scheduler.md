# simulation_scheduler.md

# Acheron Simulation Scheduler

## Deterministic Massive-Scale Concurrency Architecture

---

# 1. Scheduler Philosophy

The scheduler is not:

* a thread pool
* an async utility
* a background worker manager

It is:

# the execution backbone of the simulation.

The scheduler determines:

* determinism
* scalability
* cache coherency
* synchronization cost
* frame stability
* simulation correctness

At Acheron scale:

* millions of entities
* thousands of jobs
* heterogeneous update rates
* streaming systems
* visibility systems
* economy propagation
* traffic simulation

the scheduler becomes:

```text id="llz1nh"
the engine itself
```

---

# 2. Primary Design Goals

The scheduler optimizes for:

```text id="yod6z0"
deterministic execution
minimal synchronization
high CPU occupancy
cache locality
low contention
predictable frame pacing
NUMA-aware execution
```

NOT:

```text id="7m2v9x"
maximum parallelism at any cost
```

Uncontrolled parallelism creates:

* false sharing
* synchronization storms
* cache invalidation
* scheduler thrashing

---

# 3. Core Execution Model

Acheron uses:

# Fiber-Based Job Scheduling

NOT:

```text id="m51vdo"
one thread per task
```

NOT:

```text id="v3h0tb"
std::async everywhere
```

The system architecture:

```text id="1e3ps3"
Simulation Frame
    ↓
Task Graph Generation
    ↓
Job Decomposition
    ↓
Fiber Scheduling
    ↓
Worker Execution
    ↓
Phase Barrier Synchronization
    ↓
Frame Commit
```

---

# 4. Why Fibers Matter

Threads are:

* expensive
* kernel-scheduled
* blocking

Fibers are:

* user-space scheduled
* lightweight
* cooperative

Fibers allow:

```text id="p4z8qv"
suspend without blocking worker threads
```

Critical for:

* pathfinding
* streaming waits
* dependency stalls
* asynchronous GPU events

---

# 5. Scheduler Architecture

The scheduler consists of:

```text id="h15k98"
Worker Threads
Fiber Pool
Task Graph
Job Queues
Dependency Counters
Phase Barriers
Frame Allocators
```

---

# 6. Worker Thread Model

Worker count:

```cpp id="gix0wn"
const uint32_t workerCount =
    std::thread::hardware_concurrency() - 1;
```

Reserve:

* one thread for render submission
* one thread for OS/message pump

Workers remain pinned for cache stability.

---

# 7. Thread Affinity

Workers should be pinned:

```text id="92qfvi"
Worker 0 → Core 0
Worker 1 → Core 1
```

Why?

Because migration causes:

* cache invalidation
* L1/L2 eviction
* NUMA penalties

---

# 8. NUMA Awareness

Future large-scale support:

```text id="fr6r0e"
NUMA-local allocators
NUMA-local queues
NUMA-aware chunk ownership
```

Critical for:

* Threadripper
* EPYC
* Xeon-scale CPUs

---

# 9. Task Graph Architecture

The simulation frame becomes:

# a directed acyclic graph (DAG)

Example:

```text id="fdgxsd"
Traffic Simulation
    ↓
Logistics Propagation
    ↓
Economy Update
    ↓
Civilian Behavior
    ↓
Render Snapshot
```

Dependencies are explicit.

---

# 10. Task Definition

Tasks are lightweight units:

```cpp id="2wjrms"
struct Task
{
    TaskFunction entry;

    void* data;

    std::atomic<uint32_t> unfinishedDependencies;

    Task* dependents;
};
```

Tasks NEVER own memory.

Tasks operate on:

* preallocated simulation chunks
* ECS ranges
* frame arenas

---

# 11. Job Granularity

Critical rule:

```text id="m2uhxq"
Never schedule per-entity jobs
```

Correct:

```text id="ec7y4g"
1 task = 2k–16k entities
```

Reason:
scheduler overhead must remain amortized.

---

# 12. ECS Chunk Scheduling

Jobs operate on:

# archetype chunks

Example:

```cpp id="tnd2yv"
ScheduleChunkJob(
    chunkBegin,
    chunkCount
);
```

This preserves:

* cache locality
* sequential traversal
* SIMD batching

---

# 13. Scheduler Queues

Each worker owns:

```text id="2oym2k"
local work-stealing deque
```

NOT:

```text id="zsk7yz"
global mutex queue
```

Global queues collapse under contention.

---

# 14. Work-Stealing Deque

Queue structure:

```cpp id="5jlwmv"
struct WorkStealingDeque
{
    alignas(64) std::atomic<uint64_t> top;

    alignas(64) std::atomic<uint64_t> bottom;

    Task* tasks[MAX_TASKS];
};
```

Separate cache lines reduce:

* false sharing
* MESI traffic

---

# 15. Queue Scheduling Logic

Owner worker:

```text id="gjjlwm"
pop bottom
```

Stealing worker:

```text id="c4x5ae"
steal top
```

This minimizes contention.

---

# 16. Fiber Pool

Fibers are preallocated.

Example:

```cpp id="7y4gvl"
Fiber fibers[MAX_FIBERS];
```

No runtime heap allocations.

Fibers contain:

* stack
* execution context
* scheduler metadata

---

# 17. Fiber Stack Size

Recommended:

| Fiber Type          | Stack Size |
| ------------------- | ---------- |
| Standard simulation | 64KB       |
| Pathfinding         | 128KB      |
| Streaming           | 256KB      |

Avoid:

```text id="7sbjlwm"
megabyte-scale stacks
```

---

# 18. Fiber Suspension

Tasks suspend when waiting:

```text id="w1yaz2"
resource dependency
GPU fence
streaming completion
phase barrier
```

Instead of:

```text id="78fyyn"
blocking OS threads
```

---

# 19. Simulation Phases

The frame is divided into deterministic phases.

Example:

```text id="2a0l0n"
Input
    ↓
Traffic
    ↓
Economy
    ↓
Civilian AI
    ↓
Physics
    ↓
Visibility
    ↓
Snapshot Extraction
```

No phase overlap allowed unless proven safe.

---

# 20. Why Deterministic Phases Matter

Without deterministic phases:

```text id="1f5rtk"
race conditions
nondeterministic saves
network divergence
replay instability
```

Deterministic scheduling is mandatory for:

* debugging
* replay systems
* lockstep networking
* save validation

---

# 21. Phase Barrier System

Barriers synchronize phase completion.

Example:

```cpp id="6klrfv"
struct PhaseBarrier
{
    std::atomic<uint32_t> completed;

    uint32_t target;
};
```

Workers increment completion count.

Next phase unlocks only after:

```text id="0njlwm"
all tasks complete
```

---

# 22. Barrier Stall Prevention

Barrier stalls are catastrophic.

Therefore:

* task counts balanced
* chunk sizes normalized
* work stealing aggressive

The scheduler must avoid:

```text id="m1vjlwm"
long-tail tasks
```

---

# 23. Frame Timeline

Target frame structure:

```text id="e2wjlwm"
2ms → traffic
2ms → economy
1ms → AI
1ms → physics
1ms → visibility
1ms → extraction
```

Simulation budget:

```text id="mjlwm7"
~8ms total
```

---

# 24. Update Frequency Decoupling

Not all systems update equally.

Example:

| System    | Frequency |
| --------- | --------- |
| Traffic   | 30Hz      |
| Economy   | 5Hz       |
| AI        | 10Hz      |
| Rendering | 60–144Hz  |
| Audio     | realtime  |

This massively reduces:

* CPU load
* synchronization overhead

---

# 25. Temporal Bucketing

Low-frequency systems use:

# temporal buckets

Example:

```text id="jlwm89"
Economy Sector A → frame 0
Economy Sector B → frame 1
Economy Sector C → frame 2
```

This smooths CPU usage.

---

# 26. Memory Ownership Rules

Tasks NEVER allocate from global heap.

Allowed:

```text id="sljlwm"
frame allocators
arena allocators
thread-local scratch allocators
```

Forbidden:

```text id="djlwm1"
malloc/new inside hot loops
```

---

# 27. Frame Allocator Strategy

Each worker owns:

```cpp id="x7jlwm"
ThreadLocalFrameArena
```

Reset at frame completion.

Benefits:

* zero fragmentation
* no allocator contention
* deterministic lifetime

---

# 28. Cache Locality Rules

Scheduler must preserve:

```text id="hjlwm2"
spatial locality
temporal locality
sequential traversal
```

Therefore:

* ECS chunks processed contiguously
* systems sorted by archetype
* worker affinity preserved

---

# 29. False Sharing Prevention

Critical rule:

```text id="d8jlwm"
Never allow multiple workers
to modify the same cache line
```

Example:

BAD:

```cpp id="njlwm3"
struct Agent
{
    float x;
    float y;
    bool dirty;
};
```

Multiple threads touching adjacent agents:

* invalidates cache lines continuously

---

# 30. Correct Parallel Layout

GOOD:

```cpp id="mjlwm4"
alignas(64)
struct ChunkSimulationState
{
    float density[4096];
};
```

Workers own:

* independent chunks
* independent cache lines

---

# 31. Synchronization Philosophy

Use synchronization only:

```text id="jlwm55"
at phase boundaries
```

Avoid:

* fine-grained locks
* per-entity mutexes
* atomic-heavy hot paths

---

# 32. Atomic Usage Rules

Atomics allowed only for:

* queue indices
* dependency counters
* completion barriers

Atomics forbidden inside:

* hot simulation loops
* ECS iteration
* pathfinding grids

---

# 33. Streaming Scheduler

Streaming jobs operate separately from simulation.

Streaming tasks:

* IO reads
* decompression
* mesh uploads
* texture uploads

Streaming must NEVER block:

* gameplay
* render submission
* ECS updates

---

# 34. Async IO Pipeline

Streaming path:

```text id="jlwm66"
Disk Read
    ↓
Background Decompression
    ↓
Asset Decode
    ↓
GPU Upload Queue
    ↓
Render Residency
```

All asynchronous.

---

# 35. GPU Synchronization

Simulation should NEVER wait for GPU unless unavoidable.

Avoid:

```text id="qjlwm7"
GPU readbacks
blocking fences
pipeline flushes
```

GPU waits destroy frame pacing.

---

# 36. Snapshot Extraction

Final simulation stage:

```text id="5jlwm8"
Extract immutable render snapshot
```

Renderer consumes snapshots asynchronously.

Simulation continues independently.

---

# 37. Profiling Infrastructure

Scheduler must expose:

* task timings
* worker occupancy
* queue contention
* barrier stalls
* cache misses
* NUMA migration

Without profiling:

```text id="0jlwm9"
scheduler optimization is blind
```

---

# 38. Debug Visualization

Required debug overlays:

```text id="jlwm10"
worker timelines
task graph visualization
barrier waits
chunk ownership
queue depths
```

These become critical at scale.

---

# 39. Failure Modes

The scheduler must defend against:

```text id="jlwm11"
deadlocks
priority inversion
starvation
barrier stalls
memory exhaustion
runaway task spawning
false sharing
cache thrashing
```

---

# 40. Simulation Determinism

Acheron requires:

# deterministic simulation

Therefore:

* fixed timestep
* deterministic phase order
* stable iteration order
* deterministic RNG
* stable task decomposition

Without this:

```text id="jlwm12"
replays desync
networking fails
bugs become impossible to reproduce
```

---

# 41. Target Scalability

The scheduler should scale to:

| Metric            | Target           |
| ----------------- | ---------------- |
| Entities          | 1M+              |
| Tasks/frame       | 10k+             |
| Worker threads    | 8–64             |
| Frame time        | < 8ms simulation |
| Streaming tasks   | thousands        |
| Concurrent fibers | 100k+            |

---

# 42. Long-Term Evolution Path

Future upgrades:

```text id="jlwm13"
GPU simulation scheduling
job priority lanes
NUMA partitioning
distributed simulation
async compute scheduling
fiber-aware IO completion
```

---

# 43. Final Scheduler Philosophy

The scheduler exists to solve:

```text id="jlwm14"
how to move the minimum amount of data
through the CPU
with the fewest synchronization points
while preserving deterministic execution
```

NOT:

```text id="jlwm15"
how to maximize thread count
```

Modern simulation performance is fundamentally limited by:

```text id="jlwm16"
cache coherency
memory bandwidth
synchronization overhead
data movement
```

The scheduler is therefore:

# a memory orchestration system

more than:

# a concurrency system
