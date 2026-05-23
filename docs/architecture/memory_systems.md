# memory_systems.md

# Acheron Memory Systems

## Deterministic Allocation, Cache Locality, and High-Throughput Data Ownership

---

# 1. Memory Philosophy

Memory is the primary performance constraint.

NOT:

* FLOPS
* thread count
* polygon count

Modern simulation engines are limited primarily by:

```text id="f5wj7t"
cache misses
memory bandwidth
allocation stalls
pointer chasing
TLB pressure
NUMA migration
```

Acheron therefore treats memory as:

# the central architectural concern

Every system is designed around:

* contiguous access
* deterministic ownership
* stable lifetimes
* allocation-free hot paths
* cache locality
* SIMD traversal

---

# 2. Fundamental Rules

The engine follows these hard constraints:

```text id="c9cx3u"
No allocations in hot loops
No general heap usage during simulation
No shared mutable ownership
No pointer-heavy object graphs
No recursive runtime allocation
No STL containers inside ECS hot paths
```

Instead:

* arenas
* pools
* chunk allocators
* ring buffers
* SoA layouts
* frame allocators

---

# 3. Memory Architecture Overview

The memory system is divided into:

```text id="1p8gwe"
Persistent Memory
    ↓
Level Memory
    ↓
Frame Memory
    ↓
Scratch Memory
    ↓
GPU Memory
```

Each tier has:

* distinct ownership
* distinct lifetime
* distinct allocator strategy

---

# 4. Persistent Memory

Persistent memory survives entire application runtime.

Examples:

```text id="wjlwm1"
renderer state
shader cache
global asset metadata
simulation registries
thread systems
```

Allocator:

# virtual memory arena

---

# 5. Level Memory

Level memory survives:

* district load
* map load
* streaming region

Destroyed when region unloads.

Examples:

```text id="jlwm2"
traffic graphs
district geometry
navigation grids
streamed entities
```

Allocator:

# region arena allocator

---

# 6. Frame Memory

Frame memory survives:

# one frame only

Examples:

```text id="jlwm3"
temporary render packets
visibility lists
sorting buffers
job metadata
transient physics contacts
```

Allocator:

# linear frame allocator

Reset every frame.

---

# 7. Scratch Memory

Scratch memory survives:

# one task execution

Examples:

```text id="jlwm4"
temporary SIMD buffers
sorting scratch
pathfinding queues
compression buffers
```

Allocator:

# thread-local scratch allocator

---

# 8. GPU Memory

GPU memory separated into:

```text id="jlwm5"
persistent VRAM
streaming VRAM
upload heaps
frame transient resources
descriptor arenas
```

Managed independently from CPU memory.

---

# 9. Why the General Heap Fails

Default heap allocators cause:

```text id="jlwm6"
fragmentation
allocator contention
unpredictable latency
cache scattering
heap locks
```

At simulation scale:

```text id="jlwm7"
malloc/new become catastrophic
```

---

# 10. Core Allocation Strategy

Acheron uses:

| Memory Type      | Allocator             |
| ---------------- | --------------------- |
| ECS chunks       | fixed block allocator |
| frame data       | linear allocator      |
| streaming assets | region allocator      |
| temporary jobs   | scratch allocator     |
| GPU uploads      | ring allocator        |
| descriptors      | descriptor arena      |

---

# 11. Virtual Memory Arena

The engine reserves large virtual memory ranges upfront.

Example:

```cpp id="jlwm8"
ReserveVirtualMemory(8_GB);
```

Then commits pages lazily.

Benefits:

* stable address ranges
* fewer OS allocations
* reduced fragmentation

---

# 12. Linear Allocator

Primary frame allocator.

Structure:

```cpp id="jlwm9"
struct LinearAllocator
{
    uint8_t* memory;

    uint64_t offset;

    uint64_t capacity;
};
```

Allocation:

```cpp id="jlwm10"
void* Allocate(size_t size, size_t align);
```

Reset:

```cpp id="jlwm11"
allocator.Reset();
```

No frees.

---

# 13. Why Linear Allocation Wins

Linear allocation provides:

```text id="jlwm12"
O(1) allocation
no fragmentation
perfect locality
minimal bookkeeping
cache-friendly traversal
```

This is ideal for:

* transient frame memory
* ECS iteration scratch
* render packet generation

---

# 14. ECS Chunk Allocation

ECS memory organized into:

# fixed-size archetype chunks

Chunk size:

```cpp id="jlwm13"
constexpr uint32_t CHUNK_SIZE = 16 * 1024;
```

Reasons:

* aligns well with CPU cache hierarchy
* amortizes metadata overhead
* preserves sequential traversal

---

# 15. Archetype Chunk Layout

Chunks use:

# Struct-of-Arrays (SoA)

NOT:

```text id="jlwm14"
Array-of-Structs
```

Correct:

```cpp id="jlwm15"
struct PositionSoA
{
    float x[4096];
    float y[4096];
    float z[4096];
};
```

---

# 16. Why SoA Matters

SoA enables:

```text id="jlwm16"
SIMD vectorization
hardware prefetching
sequential memory access
cache line utilization
```

AoS wastes:

* cache bandwidth
* SIMD lanes
* memory locality

---

# 17. Cache Line Awareness

Modern CPUs operate on:

# cache lines

Typically:

```text id="jlwm17"
64 bytes
```

Memory layouts must align to cache boundaries.

---

# 18. Alignment Rules

Critical structures use:

```cpp id="jlwm18"
alignas(64)
```

Example:

```cpp id="jlwm19"
alignas(64)
struct WorkerQueue
{
    std::atomic<uint64_t> head;
};
```

Prevents:

* false sharing
* cache invalidation storms

---

# 19. False Sharing

False sharing occurs when:
multiple cores modify data in the same cache line.

Result:

```text id="jlwm20"
massive coherence traffic
cache invalidation
performance collapse
```

The engine aggressively isolates:

* worker counters
* queue indices
* synchronization primitives

---

# 20. Pointer Ownership Rules

Pointers are restricted.

Allowed:

* stable arena pointers
* immutable asset references
* transient scratch pointers

Forbidden:

* shared mutable graphs
* deep ownership chains
* arbitrary heap references

---

# 21. Entity Ownership

Entities are referenced by:

# generational handles

Example:

```cpp id="jlwm21"
struct EntityID
{
    uint32_t index;
    uint32_t generation;
};
```

Never raw pointers.

---

# 22. Generational Safety

Deletion increments generation:

```cpp id="jlwm22"
generation++;
```

Stale handles invalidate automatically.

Benefits:

* deterministic ownership
* safe reuse
* no dangling pointers

---

# 23. Object Pools

Certain systems use:

# fixed object pools

Examples:

```text id="jlwm23"
particles
audio voices
network packets
path requests
```

Pool allocation:

* deterministic
* contiguous
* lock-free

---

# 24. Pool Structure

Example:

```cpp id="jlwm24"
template<typename T>
struct ObjectPool
{
    T* objects;

    uint32_t* freeList;
};
```

---

# 25. Ring Buffers

Used for:

* render uploads
* event queues
* audio streaming
* network packets

Structure:

```cpp id="jlwm25"
template<typename T>
struct RingBuffer
{
    T* buffer;

    uint64_t head;

    uint64_t tail;
};
```

---

# 26. Double and Triple Buffering

Critical for:

```text id="jlwm26"
simulation/render decoupling
```

Renderer consumes:

```text id="jlwm27"
immutable snapshots
```

Simulation writes:

```text id="jlwm28"
next frame state
```

Avoids:

* synchronization stalls
* race conditions

---

# 27. Thread-Local Allocators

Each worker thread owns:

```text id="jlwm29"
ThreadLocalScratchAllocator
```

Benefits:

* zero contention
* deterministic locality
* reduced cache bouncing

---

# 28. NUMA Awareness

Future scaling requires:

# NUMA-local allocation

Rule:

```text id="jlwm30"
Allocate where data executes
```

Cross-NUMA access causes:

* latency spikes
* memory bandwidth collapse

---

# 29. Streaming Memory

Streaming systems require:

* asynchronous allocation
* transient residency
* eviction systems

Streaming assets never allocate directly into persistent memory.

---

# 30. Asset Residency

Assets transition between:

```text id="jlwm31"
Unloaded
Resident
GPU Uploaded
Visible
Eviction Candidate
```

The residency manager controls:

* VRAM pressure
* RAM usage
* streaming priorities

---

# 31. Texture Streaming

Textures use:

# mip residency

Only required mip levels remain resident.

Benefits:

```text id="jlwm32"
lower VRAM usage
faster streaming
better frame stability
```

---

# 32. Mesh Streaming

Meshes loaded in:

# chunked geometry pages

Far geometry uses:

* low LOD
* compressed vertex streams

Near geometry:

* full precision

---

# 33. Compression Strategy

Compressed runtime formats:

| Asset     | Format                   |
| --------- | ------------------------ |
| textures  | BC7 / ASTC               |
| meshes    | quantized vertex streams |
| audio     | OGG/Opus                 |
| animation | compressed transforms    |

Compression reduces:

* IO bandwidth
* memory residency
* cache pressure

---

# 34. STL Usage Rules

Allowed:

```text id="jlwm33"
std::array
std::span
std::bitset
```

Restricted:

```text id="jlwm34"
std::vector
std::unordered_map
std::string
```

inside hot systems.

Why?

Because:

* hidden allocations
* allocator unpredictability
* pointer-heavy layouts

---

# 35. Preferred Runtime Containers

Preferred:

* flat arrays
* packed vectors
* sparse sets
* SoA blocks
* custom hash tables

---

# 36. Hash Table Strategy

Simulation hash tables use:

# robin hood hashing

Reasons:

* cache-friendly
* predictable probe lengths
* dense storage

---

# 37. Memory Lifetime Ownership

Every allocation must answer:

```text id="jlwm35"
Who owns this?
How long does it live?
Which allocator created it?
Which thread accesses it?
```

Undefined ownership causes:

```text id="jlwm36"
memory leaks
lifetime bugs
race conditions
```

---

# 38. Serialization Memory

Serialization uses:

# contiguous binary blobs

Avoid:

```text id="jlwm37"
pointer serialization
deep object trees
```

Instead:

* flat ECS snapshots
* stable IDs
* offset-based references

---

# 39. Save Memory Layout

Preferred save structure:

```text id="jlwm38"
Header
Chunk Table
Entity Arrays
Component Streams
String Table
Checksums
```

---

# 40. GPU Upload Memory

GPU upload buffers use:

# persistent mapped ring allocators

Example:

```cpp id="jlwm39"
struct UploadRing
{
    uint8_t* mappedMemory;

    uint64_t writeOffset;
};
```

Avoid:

* per-upload allocations
* driver stalls

---

# 41. Descriptor Allocation

Descriptors allocated from:

# descriptor arenas

NOT:

```text id="jlwm40"
per-frame descriptor heaps
```

Descriptor fragmentation can destroy:

* GPU performance
* resource binding efficiency

---

# 42. Memory Debugging

Required tooling:

```text id="jlwm41"
allocation trackers
arena visualizers
fragmentation graphs
cache miss profilers
leak detectors
```

Without memory telemetry:

```text id="jlwm42"
optimization becomes guesswork
```

---

# 43. Cache Optimization Philosophy

CPU performance is fundamentally:

```text id="jlwm43"
data movement optimization
```

NOT:

```text id="jlwm44"
algorithmic arithmetic optimization
```

The fastest code:

# avoids memory movement entirely

---

# 44. Hardware-Conscious Design

Acheron memory layouts optimize for:

| Hardware Layer | Goal                 |
| -------------- | -------------------- |
| L1 cache       | hot loop locality    |
| L2 cache       | chunk traversal      |
| L3 cache       | worker coherence     |
| RAM            | sequential bandwidth |
| SSD            | streaming throughput |
| VRAM           | residency efficiency |

---

# 45. Failure Modes

The memory system must defend against:

```text id="jlwm45"
heap fragmentation
allocator contention
false sharing
NUMA migration
cache thrashing
runaway residency
descriptor exhaustion
VRAM overflow
```

---

# 46. Long-Term Memory Evolution

Future upgrades:

```text id="jlwm46"
custom virtual memory manager
NUMA partitioning
GPU-driven memory residency
compressed ECS chunks
memory defragmentation
automatic residency prediction
```

---

# 47. Memory Budgets

Target budgets:

| System             | Budget    |
| ------------------ | --------- |
| ECS world          | 1–2 GB    |
| textures           | 2–4 GB    |
| geometry           | 1–2 GB    |
| streaming cache    | 512 MB    |
| frame allocators   | 64–256 MB |
| scratch allocators | 32–128 MB |

---

# 48. Final Memory Philosophy

Acheron is fundamentally:

# a data movement engine

Every subsystem therefore optimizes for:

```text id="jlwm47"
contiguous traversal
stable ownership
minimal allocation
cache coherency
predictable lifetimes
bandwidth efficiency
```

The memory architecture exists to ensure:

```text id="jlwm48"
the CPU spends time computing
instead of waiting for memory
```
