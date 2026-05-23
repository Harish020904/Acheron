# save_format.md

# Acheron Save Format

## Deterministic Serialization Architecture for Massive-Scale Simulation

---

# 1. Save System Philosophy

The save system is not:

* a convenience feature
* a JSON dump
* an object serializer

It is:

# a deterministic world reconstruction system

The save architecture exists to solve:

```text id="b98q6t"
world persistence
simulation continuity
replay determinism
streaming recovery
network synchronization
incremental state restoration
```

At Acheron scale:

* millions of entities
* streamed districts
* asynchronous simulation
* procedural world generation
* ECS chunk architecture

traditional save systems collapse.

---

# 2. Core Design Goals

The save system optimizes for:

```text id="7mdckh"
determinism
fast streaming
binary compactness
cache-friendly loading
version resilience
incremental serialization
stable entity reconstruction
```

NOT:

```text id="t5rsm1"
human readability
```

Human-readable formats become:

* too large
* too slow
* too fragmented

---

# 3. Save System Architecture

The save system is divided into:

```text id="rjlwm1"
Global World Save
    ↓
Region Save Chunks
    ↓
Entity Component Streams
    ↓
Streaming Delta Saves
    ↓
Replay/Event Logs
```

---

# 4. Fundamental Serialization Rule

Never serialize:

```text id="jlwm2"
raw pointers
virtual tables
heap graphs
runtime memory addresses
```

Only serialize:

```text id="jlwm3"
stable IDs
flat arrays
offsets
component streams
deterministic values
```

---

# 5. Save File Layout

Primary binary layout:

```text id="jlwm4"
File Header
    ↓
Version Table
    ↓
Chunk Directory
    ↓
World Metadata
    ↓
Entity Tables
    ↓
Component Streams
    ↓
Spatial Data
    ↓
Simulation State
    ↓
Checksums
```

---

# 6. File Header Structure

The file begins with:

```cpp id="jlwm5"
struct SaveHeader
{
    uint32_t magic;

    uint32_t version;

    uint64_t timestamp;

    uint64_t worldSeed;

    uint64_t chunkTableOffset;
};
```

Purpose:

* validation
* compatibility
* corruption detection

---

# 7. Magic Number

Every save begins with:

```cpp id="jlwm6"
constexpr uint32_t SAVE_MAGIC =
    0x41434852;
```

("ACHR")

This detects:

* corrupted files
* invalid formats
* partial writes

---

# 8. Save Versioning

Critical requirement:

```text id="jlwm7"
Save formats MUST evolve safely
```

Every component stream carries:

* version ID
* serialization schema hash

This allows:

* migration
* backward compatibility
* live patch support

---

# 9. Chunk-Based World Saves

The world is divided into:

# streaming districts

Each district serializes independently.

Example:

```text id="jlwm8"
district_12.save
district_13.save
district_14.save
```

Benefits:

* partial loading
* streaming recovery
* incremental autosaves
* parallel IO

---

# 10. Region Save Layout

Each region contains:

```text id="jlwm9"
terrain state
traffic state
economy state
entities
navigation fields
simulation metadata
```

Only loaded regions remain resident.

---

# 11. ECS Serialization Philosophy

The ECS is serialized as:

# component streams

NOT:

```text id="jlwm10"
object trees
```

Correct approach:

```text id="jlwm11"
Entity IDs
    ↓
Position Stream
    ↓
Velocity Stream
    ↓
Traffic Stream
    ↓
Economic Stream
```

This preserves:

* cache locality
* streaming efficiency
* SIMD-friendly loading

---

# 12. Entity Table Format

Entity table:

```cpp id="jlwm12"
struct SerializedEntity
{
    uint32_t entityIndex;

    uint32_t generation;

    uint64_t archetypeHash;
};
```

No pointers stored.

---

# 13. Archetype Serialization

Archetypes serialized independently.

Example:

```text id="jlwm13"
Position + Velocity + Traffic
```

stored separately from:

```text id="jlwm14"
Position + EconomicAgent
```

This matches runtime ECS layout.

---

# 14. Component Stream Format

Each component stream:

```cpp id="jlwm15"
struct ComponentStreamHeader
{
    uint64_t componentID;

    uint32_t componentVersion;

    uint64_t entityCount;

    uint64_t dataOffset;
};
```

---

# 15. Struct-of-Arrays Save Layout

Components serialize in:

# SoA order

Example:

```cpp id="jlwm16"
float posX[N];
float posY[N];
float posZ[N];
```

NOT:

```cpp id="jlwm17"
struct Position
{
    float x;
    float y;
    float z;
};
```

Why?

Because SoA:

* streams faster
* compresses better
* loads cache-efficiently

---

# 16. Compression Strategy

Compression pipeline:

```text id="jlwm18"
Serialize
    ↓
Chunk Compress
    ↓
Write To Disk
```

Compression formats:

| Asset          | Compression       |
| -------------- | ----------------- |
| save chunks    | zstd              |
| terrain        | delta compression |
| traffic fields | RLE               |
| navigation     | quantized grids   |

---

# 17. Incremental Saves

The engine supports:

# delta serialization

Only modified chunks serialize.

Benefits:

```text id="jlwm19"
faster autosaves
lower IO bandwidth
minimal frame interruption
```

---

# 18. Dirty Tracking

Chunks contain dirty flags:

```cpp id="jlwm20"
struct ChunkState
{
    bool dirty;
};
```

Only dirty chunks serialize.

---

# 19. Autosave System

Autosaves occur asynchronously.

Pipeline:

```text id="jlwm21"
Simulation Snapshot
    ↓
Serialization Queue
    ↓
Background Compression
    ↓
Async Disk Write
```

Simulation never blocks on disk IO.

---

# 20. Snapshot Isolation

Save serialization operates on:

# immutable snapshots

Never serialize live ECS memory directly.

Otherwise:

```text id="jlwm22"
race conditions
corrupted saves
partial writes
```

---

# 21. Binary Serialization Rules

Serialization rules:

```text id="jlwm23"
No runtime pointers
No STL containers
No polymorphic layouts
No platform-dependent padding
```

Everything uses:

* fixed-width types
* deterministic layouts
* explicit alignment

---

# 22. Endianness Handling

All saves use:

# little-endian canonical layout

Cross-platform loading requires:

* endian conversion layer
* validation checks

---

# 23. Deterministic Floating Point

Critical simulation values use:

* deterministic math
* quantized storage where possible

Otherwise:

```text id="jlwm24"
simulation divergence
replay mismatch
network instability
```

---

# 24. Quantization Strategy

Examples:

| Data            | Storage           |
| --------------- | ----------------- |
| positions       | quantized float16 |
| rotations       | packed quaternion |
| traffic density | uint16            |
| economy weights | uint16            |

Reduces:

* disk size
* bandwidth
* cache pressure

---

# 25. Replay System

Replay architecture:

# event log + deterministic simulation

Replay file contains:

```text id="jlwm25"
player input
random seeds
simulation events
timestamps
```

NOT full world snapshots every frame.

---

# 26. Event Log Structure

Example:

```cpp id="jlwm26"
struct SimulationEvent
{
    uint32_t frame;

    uint16_t type;

    uint16_t payloadSize;
};
```

---

# 27. Save Validation

Each chunk stores:

# checksums

Example:

```cpp id="jlwm27"
uint64_t crc64;
```

Detects:

* corruption
* incomplete writes
* disk failures

---

# 28. Save Recovery

Recovery pipeline:

```text id="jlwm28"
Validate Header
    ↓
Validate Chunk Table
    ↓
Validate Checksums
    ↓
Recover Valid Regions
    ↓
Regenerate Missing Chunks
```

Procedural generation helps recover damaged regions.

---

# 29. Procedural World Reconstruction

The world seed is always serialized.

This allows:

* terrain regeneration
* district reconstruction
* procedural traffic regeneration

Only delta changes require storage.

---

# 30. Save Streaming

Large saves stream incrementally.

Never load:

```text id="jlwm29"
entire world into RAM
```

Instead:

```text id="jlwm30"
stream districts on demand
```

---

# 31. Save Loading Pipeline

Load path:

```text id="jlwm31"
Read Header
    ↓
Read Chunk Directory
    ↓
Determine Required Regions
    ↓
Async Chunk Load
    ↓
Decompress
    ↓
Reconstruct ECS Chunks
    ↓
Restore Simulation State
```

---

# 32. Parallel Deserialization

Chunk loading is:

# parallelized

Workers deserialize:

* ECS streams
* traffic fields
* navigation grids
* economy sectors

simultaneously.

---

# 33. Memory Reconstruction

Deserialization loads directly into:

# archetype chunk memory

Avoid:

```text id="jlwm32"
temporary object construction
```

Direct memory reconstruction preserves:

* cache locality
* SIMD layouts
* allocator efficiency

---

# 34. Save File Directory

The chunk directory stores:

```cpp id="jlwm33"
struct ChunkEntry
{
    uint64_t offset;

    uint64_t compressedSize;

    uint64_t uncompressedSize;

    uint64_t regionID;
};
```

Enables:

* random access
* partial loading
* async streaming

---

# 35. Save Metadata

Global metadata:

```text id="jlwm34"
world seed
playtime
difficulty
simulation tick
build version
mod version
```

Stored separately from simulation chunks.

---

# 36. Mod Compatibility

Future mod support requires:

# schema-based serialization

Components identified by:

* stable hashes
* schema versions

NOT:

```text id="jlwm35"
compile-time type order
```

---

# 37. Save Security

Save files must defend against:

* corruption
* malicious edits
* invalid memory reconstruction

Validation required before:

* allocation
* deserialization
* chunk reconstruction

---

# 38. Streaming Constraints

Target save constraints:

| Metric             | Target  |
| ------------------ | ------- |
| autosave stall     | < 2ms   |
| chunk load         | < 50ms  |
| region load        | < 500ms |
| memory overhead    | minimal |
| replay determinism | exact   |

---

# 39. Large-Scale World Saves

At full scale:

| World Element      | Count                 |
| ------------------ | --------------------- |
| entities           | millions              |
| traffic nodes      | hundreds of thousands |
| economy agents     | millions              |
| streamed districts | thousands             |

Therefore:

```text id="jlwm36"
save architecture must scale horizontally
```

---

# 40. Save Compression Philosophy

Compression must optimize for:

```text id="jlwm37"
streaming speed
decompression speed
cache locality
partial access
```

NOT:

```text id="jlwm38"
maximum compression ratio
```

Fast decompression matters more.

---

# 41. Save Failure Modes

The system must defend against:

```text id="jlwm39"
partial writes
power loss
corruption
version mismatch
desync
stale chunk references
invalid entity handles
```

---

# 42. Save Testing Requirements

Required tests:

```text id="jlwm40"
save/load determinism
cross-version migration
corruption recovery
parallel loading
incremental save correctness
```

Without this:

```text id="jlwm41"
large worlds become unstable
```

---

# 43. Long-Term Save Evolution

Future upgrades:

```text id="jlwm42"
networked persistence
cloud streaming saves
incremental replication
distributed region saves
live world patching
```

---

# 44. Save Performance Philosophy

The save system exists to minimize:

```text id="jlwm43"
memory movement
serialization overhead
disk stalls
runtime reconstruction work
```

The optimal save system:

# reconstructs runtime memory directly

without:

```text id="jlwm44"
expensive object rebuilding
```

---

# 45. Final Save System Philosophy

Acheron’s save architecture is fundamentally:

# a streaming data reconstruction system

NOT:

# an object persistence system

Every subsystem therefore optimizes for:

```text id="jlwm45"
flat data
stable IDs
deterministic layouts
contiguous streams
incremental reconstruction
asynchronous IO
```

The save format exists to ensure:

```text id="jlwm46"
massive simulation worlds
can persist and recover
without disrupting frame stability
```
