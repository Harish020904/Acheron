# ecs_architecture.md

# Project Acheron — ECS Architecture

## Million-Agent Simulation Kernel

### C++20/23 Archetype ECS Specification

---

# 1. Design Philosophy

The ECS (Entity Component System) in Acheron is not a gameplay abstraction.
It is a memory-oriented simulation database designed to maximize:

* cache locality
* SIMD traversal
* deterministic execution
* multithreaded scalability
* memory bandwidth efficiency

The ECS exists to support:

```text
1,000,000+ agents
100,000+ visible entities
massive concurrent updates
continuous streaming
GPU-driven rendering
```

Traditional OOP hierarchies fail at this scale because:

* virtual dispatch destroys branch predictability
* heap allocations fragment memory
* pointer chasing destroys cache locality
* inheritance prevents contiguous traversal

The Acheron ECS therefore prioritizes:

```text
Data Layout > Abstraction
Memory Throughput > API Convenience
Deterministic Scheduling > Runtime Flexibility
SIMD Traversal > Object Encapsulation
```

---

# 2. Core Architectural Principles

## 2.1 Archetype-Based Storage

Entities with identical component signatures are stored together.

Example archetype:

```text
Position + Velocity + TrafficAgent + EconomicState
```

This allows systems to iterate contiguous arrays without filtering.

Benefits:

* contiguous memory access
* fewer cache misses
* predictable SIMD batching
* reduced branch divergence
* efficient multithreading

---

## 2.2 Struct-of-Arrays (SoA)

The ECS uses Struct-of-Arrays storage.

BAD:

```cpp
struct Agent
{
    Position position;
    Velocity velocity;
    Traffic traffic;
};
```

This creates:

* poor cache utilization
* wasted cache lines
* non-vectorizable traversal

CORRECT:

```cpp
struct PositionArray
{
    alignas(64) float x[CHUNK_ENTITY_CAP];
    alignas(64) float y[CHUNK_ENTITY_CAP];
};

struct VelocityArray
{
    alignas(64) float vx[CHUNK_ENTITY_CAP];
    alignas(64) float vy[CHUNK_ENTITY_CAP];
};
```

This enables:

* AVX2/AVX-512 traversal
* hardware prefetching
* branch coherence
* sequential streaming

---

## 2.3 Fixed-Size Archetype Chunks

All archetypes allocate fixed-size memory chunks.

Recommended chunk size:

```cpp
constexpr size_t CHUNK_SIZE = 16 * 1024;
```

Why 16KB?

Because:

* fits efficiently into L1/L2 traversal windows
* minimizes allocator overhead
* simplifies chunk migration
* improves prefetch predictability

---

# 3. ECS Memory Layout

## 3.1 Chunk Structure

```cpp
struct alignas(64) ArchetypeChunk
{
    std::byte* componentBuffers[MAX_COMPONENTS];

    uint32_t entityCount;
    uint32_t capacity;

    EntityID entities[CHUNK_ENTITY_CAP];

    ArchetypeChunk* next;
};
```

---

## 3.2 Component Columns

Each component type owns its own contiguous column.

```cpp
struct ComponentColumn
{
    std::byte* buffer;
    size_t stride;
    size_t alignment;
};
```

Memory allocation:

```cpp
buffer = static_cast<std::byte*>(
    std::aligned_alloc(alignment, stride * capacity)
);
```

---

## 3.3 Alignment Strategy

All hot data is aligned to:

```cpp
alignas(64)
```

Reason:

64-byte alignment matches modern CPU cache line sizes.

Benefits:

* avoids split cache-line loads
* improves SIMD memory access
* reduces false sharing
* improves hardware prefetch behavior

---

# 4. Entity Lifecycle

# 4.1 Entity IDs

Raw pointers are forbidden.

Entities are represented as:

```cpp
struct EntityID
{
    uint32_t index;
    uint32_t generation;
};
```

---

# 4.2 Entity Records

```cpp
struct EntityRecord
{
    ArchetypeChunk* chunk;
    uint32_t row;
    uint32_t generation;
};
```

Global lookup:

```cpp
std::vector<EntityRecord> entityTable;
```

---

# 4.3 Deletion Process

When deleting:

Step 1:
Increment generation.

```cpp
record.generation++;
```

Step 2:
Return index to freelist.

```cpp
freeIndices.push(recordIndex);
```

Step 3:
Invalidate stale references automatically.

---

# 4.4 Validation

```cpp
bool IsAlive(EntityID id)
{
    return entityTable[id.index].generation == id.generation;
}
```

This prevents:

* dangling pointers
* stale references
* invalid component access

---

# 5. Component System

# 5.1 Component Requirements

All components must satisfy:

```cpp
template<typename T>
concept Component =
    std::is_trivially_copyable_v<T> &&
    std::is_standard_layout_v<T>;
```

This guarantees:

* memcpy-safe migration
* deterministic layout
* SIMD-safe memory traversal
* zero virtual dispatch

---

# 5.2 Hot vs Cold Components

Components are separated into:

## Hot Components

Updated every frame.

Examples:

* Position
* Velocity
* Transform
* TrafficAgent
* AnimationState

Stored in:

```text
L1/L2 optimized archetype chunks
```

---

## Cold Components

Rarely updated.

Examples:

* DialogueState
* QuestData
* CosmeticMetadata
* SaveFlags

Stored separately to avoid polluting hot traversal.

---

# 5.3 Component Registration

```cpp
template<Component T>
ComponentType RegisterComponent();
```

Each component receives:

* stable runtime ID
* memory metadata
* stride/alignment data
* archetype signature bit index

---

# 6. Archetype Transitions

Adding/removing components migrates entities between archetypes.

Example:

```text
Position + Velocity
↓ add Renderable
Position + Velocity + Renderable
```

Migration process:

1. Allocate slot in destination archetype
2. Copy overlapping component columns
3. Initialize new components
4. Destroy old slot
5. Update entity table

Migration must:

* avoid heap allocation
* use memcpy where possible
* remain deterministic

---

# 7. ECS Query System

Queries iterate archetypes directly.

Example:

```cpp
Query<Position, Velocity, TrafficAgent>()
```

This resolves to:

```text
All archetypes containing:
- Position
- Velocity
- TrafficAgent
```

No runtime filtering.

---

# 7.1 Chunk Iteration

Traversal:

```cpp
for (Chunk* chunk : matchingChunks)
{
    auto* posX = GetColumn<float>(chunk, PositionX);
    auto* posY = GetColumn<float>(chunk, PositionY);

    auto* velX = GetColumn<float>(chunk, VelocityX);

    for (size_t i = 0; i < chunk->entityCount; i += 8)
    {
        // SIMD update
    }
}
```

This enables:

* AVX traversal
* contiguous memory access
* predictable prefetching

---

# 8. SIMD Strategy

The ECS is designed for explicit vectorization.

Primary targets:

```text
AVX2
AVX-512
SIMD float batching
```

Example:

```cpp
x += vx * dt;
y += vy * dt;
```

Updated 8–16 entities simultaneously.

---

# 8.1 SIMD Component Rules

SIMD-friendly components:

* float-heavy
* tightly packed
* no pointers
* no polymorphism
* aligned memory

Avoid:

* std::string
* std::vector
* virtual methods
* scattered references

---

# 9. Multithreading Strategy

The ECS is designed for chunk-level parallelism.

NOT entity-level jobs.

Correct:

```text
1 task = N chunks
```

Incorrect:

```text
1 task = 1 entity
```

---

# 9.1 Scheduler Integration

Simulation phases:

```text
Traffic
→ Logistics
→ Economy
→ Crowd
→ Rendering Snapshot
```

Each phase processes chunk ranges.

Example:

```cpp
Schedule(UpdateTraffic, chunkRange);
```

---

# 9.2 False Sharing Prevention

Worker threads never modify adjacent memory regions.

Chunk ownership remains exclusive during updates.

Benefits:

* reduced cache invalidation
* reduced synchronization
* improved NUMA locality

---

# 10. Memory Allocation Strategy

The ECS never uses general-purpose allocation during frame execution.

All memory comes from:

* chunk allocators
* frame allocators
* monotonic arenas

---

# 10.1 Linear Allocator

```cpp
struct LinearAllocator
{
    std::byte* memory;
    size_t offset;
    size_t capacity;
};
```

Frame allocations:

```cpp
void* Allocate(size_t size, size_t align)
{
    size_t aligned =
        (offset + align - 1) & ~(align - 1);

    void* ptr = memory + aligned;

    offset = aligned + size;

    return ptr;
}
```

Reset once per frame.

No fragmentation.

---

# 11. Serialization Architecture

Entity serialization uses:

* stable entity IDs
* archetype metadata
* binary component snapshots

Serialization format:

```text
Header
→ Archetype Table
→ Chunk Data
→ Entity Records
→ Component Streams
```

---

# 11.1 Save Constraints

The ECS save system must support:

* deterministic replay
* streaming world saves
* incremental chunk serialization
* version migration

---

# 12. Streaming Architecture

The world is divided into streaming districts.

Each district owns:

* ECS chunks
* navigation data
* render metadata
* audio zones

Streaming occurs asynchronously.

Entities migrate between loaded regions.

---

# 12.1 Streaming Rules

Hot gameplay regions:

```text
fully simulated
```

Distant regions:

```text
reduced simulation frequency
```

Very distant regions:

```text
aggregate statistical simulation
```

---

# 13. Renderer Integration

The renderer never traverses gameplay entities directly.

Instead:

```text
ECS
→ Render Snapshot
→ Renderer
```

Render snapshots contain:

* transforms
* mesh handles
* material handles
* animation state

Stored in contiguous GPU-friendly buffers.

---

# 14. Debugging Infrastructure

The ECS must expose:

* archetype visualizer
* chunk occupancy statistics
* component heatmaps
* allocation metrics
* cache miss instrumentation
* SIMD occupancy statistics

---

# 15. Performance Targets

Target simulation budgets:

| System            | Budget |
| ----------------- | ------ |
| ECS traversal     | < 2ms  |
| Traffic           | < 3ms  |
| Economy           | < 2ms  |
| Crowd             | < 2ms  |
| Render extraction | < 1ms  |

Target entity scale:

```text
1,000,000+ lightweight agents
100,000+ visible entities
```

---

# 16. Hardware Consequences

Correct ECS architecture improves:

* cache locality
* branch prediction
* SIMD occupancy
* memory throughput
* thread scalability

Incorrect architecture causes:

* L1/L2 thrashing
* allocator contention
* branch divergence
* TLB misses
* synchronization stalls

---

# 17. Final Architectural Principle

The ECS is not:

```text
an object system
```

It is:

```text
a memory streaming system
```

The primary bottleneck is not FLOPS.

It is:

* cache misses
* memory bandwidth
* synchronization overhead
* branch misprediction
* NUMA penalties

Every ECS design decision must therefore optimize for:

```text
Contiguous traversal
Deterministic scheduling
SIMD batching
Minimal synchronization
Predictable memory access
```
