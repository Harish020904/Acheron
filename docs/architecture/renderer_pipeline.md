# renderer_pipeline.md

# Acheron Renderer Pipeline

## GPU-Driven Urban Simulation Rendering Architecture

---

# 1. Renderer Philosophy

The renderer is not a “graphics layer.”

It is:

* a GPU scheduling system
* a memory residency manager
* a bandwidth optimization pipeline
* a visibility reduction architecture

The renderer exists to solve:

```text
CPU → GPU synchronization
VRAM residency
draw call reduction
visibility determination
streaming throughput
frame stability
```

Acheron specifically targets:

* massive visible populations
* dense urban geometry
* atmospheric rendering
* traffic-heavy scenes
* simulation-scale rendering

The renderer therefore prioritizes:

```text
GPU instancing
bindless resources
indirect draws
persistent mapped buffers
clustered lighting
asynchronous streaming
render graph scheduling
```

NOT:

* forward object-by-object rendering
* scene graph traversal
* recursive render trees
* CPU-side visibility systems

---

# 2. Renderer Architecture Overview

The renderer is divided into 7 layers:

```text
Application Layer
    ↓
Simulation Snapshot Layer
    ↓
Render Scene Extraction
    ↓
Visibility & Culling
    ↓
Render Graph Scheduling
    ↓
GPU Submission
    ↓
Post Processing & Presentation
```

---

# 3. High-Level Frame Pipeline

Each frame executes in this order:

```text
Simulation Update
    ↓
Snapshot Extraction
    ↓
GPU Buffer Upload
    ↓
Visibility Pass
    ↓
Shadow Pass
    ↓
Geometry Pass
    ↓
Lighting Pass
    ↓
Transparency Pass
    ↓
Post Processing
    ↓
UI Pass
    ↓
Swapchain Present
```

The renderer NEVER directly reads ECS simulation memory.

The simulation thread produces immutable render snapshots.

---

# 4. Render Thread Separation

Acheron uses:

* simulation thread
* render thread
* async streaming workers

The render thread only consumes:

```cpp
struct RenderSnapshot
{
    TransformSoA transforms;
    MeshHandleSoA meshes;
    MaterialHandleSoA materials;
    VisibilityFlags visibility;
};
```

This prevents:

* race conditions
* cache invalidation
* ECS lock contention
* renderer stalls

---

# 5. Render Graph System

The renderer is built around a render graph.

A render graph is:

* a dependency scheduler
* a resource lifetime tracker
* a GPU barrier optimizer

Each pass declares:

```cpp
struct RenderPassDesc
{
    std::string_view name;

    ResourceHandle inputs[16];
    ResourceHandle outputs[16];

    PipelineState pipeline;
};
```

The graph determines:

* execution order
* resource transitions
* synchronization barriers
* transient resource reuse

---

# 6. Why Render Graphs Matter

Without a render graph:

```text
manual GPU barriers
resource hazards
VRAM waste
synchronization bugs
```

With a render graph:

```text
automatic scheduling
resource aliasing
optimized transitions
stable frame execution
```

This becomes mandatory at scale.

---

# 7. Frame Resource Lifetime

Every frame uses transient GPU resources:

```text
shadow maps
gbuffers
lighting buffers
post-process textures
```

These should NOT persist globally.

Instead:

```cpp
FrameArenaAllocator
```

allocates temporary frame resources.

Resources are destroyed automatically at frame completion.

This minimizes:

* heap fragmentation
* VRAM residency pressure
* descriptor churn

---

# 8. GPU Buffer Strategy

The renderer uses:

```text
persistent mapped buffers
ring-buffer uploads
triple-buffered frame data
```

NOT:

```text
glBufferData-style reallocations
```

---

# 9. Persistent Mapped Upload Buffers

Upload path:

```text
CPU writes directly into mapped memory
    ↓
GPU consumes asynchronously
```

Example:

```cpp
struct UploadBuffer
{
    uint8_t* mappedMemory;

    uint64_t writeOffset;
    uint64_t capacity;
};
```

Benefits:

* fewer driver calls
* fewer stalls
* lower CPU overhead

---

# 10. ECS → GPU Flow

The ECS never stores GPU resources directly.

Instead:

```cpp
struct RenderMeshComponent
{
    MeshHandle mesh;
    MaterialHandle material;
};
```

The renderer resolves:

* GPU buffers
* descriptor sets
* shader bindings

This decouples:

* simulation
* rendering
* asset management

---

# 11. Scene Extraction

At frame begin:

```text
ECS query
    ↓
Extract visible render components
    ↓
Write contiguous render packet arrays
    ↓
Submit to renderer
```

Result:

```cpp
struct RenderPacket
{
    uint32_t meshID;
    uint32_t materialID;
    uint32_t transformIndex;
};
```

Packets are sorted by:

* material
* mesh
* shader pipeline

to reduce:

* pipeline switches
* descriptor rebinding
* cache invalidation

---

# 12. Visibility Pipeline

The renderer uses:

```text
Hierarchical visibility culling
```

Stages:

```text
Frustum culling
    ↓
Occlusion culling
    ↓
LOD selection
    ↓
GPU instance compaction
```

---

# 13. Frustum Culling

CPU performs broadphase frustum rejection.

Data layout:

```cpp
struct BoundingSphereSoA
{
    float x[N];
    float y[N];
    float z[N];
    float radius[N];
};
```

SIMD evaluates:

* 8–16 bounds simultaneously

---

# 14. GPU Occlusion Culling

The GPU performs:

* hierarchical Z-buffer tests
* instance rejection
* visibility compaction

Result:
only visible instances reach the geometry pass.

This massively reduces:

* overdraw
* vertex cost
* draw call waste

---

# 15. GPU-Driven Rendering

The renderer uses:

```text
Indirect draw commands
```

NOT:

```text
CPU draw loops
```

Example:

```cpp
DrawIndexedIndirectCommand
```

Generated on GPU.

Pipeline:

```text
Visibility Compute Pass
    ↓
Build Indirect Draw Buffer
    ↓
Execute MultiDrawIndirect
```

This enables:

* millions of visible objects
* minimal CPU overhead

---

# 16. Instancing Strategy

Urban rendering depends heavily on instancing.

Example:

```text
1 building mesh
rendered 20,000 times
```

Per-instance data:

```cpp
struct InstanceData
{
    mat4 transform;
    uint32_t materialIndex;
};
```

GPU fetches instance data directly.

---

# 17. Material System

The material system is:

* bindless
* descriptor-indexed
* texture-atlas aware

Materials reference:

```cpp
struct MaterialGPU
{
    uint32_t albedoIndex;
    uint32_t normalIndex;
    uint32_t roughnessIndex;
    uint32_t metallicIndex;
};
```

NOT:

* unique shader per material
* unique pipeline per object

---

# 18. Bindless Texture Architecture

Textures live in:

```text
global descriptor arrays
```

Shaders index textures dynamically:

```glsl
texture(samplerArray[material.albedoIndex], uv)
```

Benefits:

* fewer descriptor updates
* fewer pipeline switches
* better batching

---

# 19. Deferred Rendering Pipeline

Acheron uses:

# clustered deferred rendering

Reason:

* thousands of lights
* dense urban scenes
* emissive signage
* headlights
* neon reflections

---

# 20. Geometry Pass

The geometry pass writes:

```text
Albedo
Normals
Depth
Material Params
Motion Vectors
```

into the GBuffer.

---

# 21. Lighting Pass

Clustered lighting divides view space into:

```text
3D light clusters
```

Lights are assigned per cluster.

Benefits:

* avoids full-screen light evaluation
* scales to thousands of lights

---

# 22. Atmospheric Rendering

Atmosphere layers:

```text
Fog
Rain
Volumetric lighting
Distance haze
Screen-space reflections
Wet surface reflections
```

These are separate passes.

---

# 23. Post Processing Stack

Order:

```text
HDR Resolve
    ↓
Bloom
    ↓
Volumetric Blend
    ↓
Tone Mapping
    ↓
Color Grading
    ↓
FXAA/TAA
    ↓
UI Composite
```

---

# 24. Temporal Anti-Aliasing

TAA uses:

* motion vectors
* previous frame history
* jittered projection matrices

This stabilizes:

* subpixel geometry
* distant buildings
* traffic motion

---

# 25. Streaming System

The renderer streams:

* textures
* meshes
* animation data
* district chunks

Streaming workers operate asynchronously.

The render thread NEVER blocks waiting for disk IO.

---

# 26. Texture Streaming

Textures are:

* partially resident
* mip-streamed
* residency-tracked

Near objects:

```text
high mip
```

Far objects:

```text
low mip
```

This dramatically reduces:

* VRAM usage
* upload bandwidth

---

# 27. Mesh LOD Pipeline

Every mesh contains:

```text
LOD0
LOD1
LOD2
LOD3
```

Selection depends on:

* distance
* screen coverage
* importance score

Urban scenes REQUIRE aggressive LOD.

---

# 28. Meshlet Architecture (Future Upgrade)

Future renderer path:

```text
mesh shaders + meshlets
```

Benefits:

* GPU-native culling
* improved vertex locality
* lower index bandwidth

---

# 29. Shader Pipeline

Shaders compiled offline:

```text
GLSL/HLSL
    ↓
SPIR-V
    ↓
Shader Cache
```

Runtime compilation should be avoided.

---

# 30. Shader Hot Reloading

Development builds support:

* live shader recompilation
* render graph reload
* pipeline recreation

Without restarting the engine.

---

# 31. Descriptor Management

Descriptors allocated from:

```cpp
DescriptorArena
```

Avoid:

* per-frame descriptor allocation
* heap churn
* driver fragmentation

---

# 32. GPU Synchronization

The renderer minimizes:

```text
CPU ↔ GPU sync points
```

Avoid:

* GPU readbacks
* glFinish-style waits
* forced synchronization

---

# 33. Double/Triple Buffering

Frame resources are triple-buffered:

```text
Frame 0 → GPU
Frame 1 → GPU
Frame 2 → CPU Writing
```

This prevents:

* GPU starvation
* CPU stalls

---

# 34. UI Rendering

UI is rendered:

* after post processing
* in screen space
* via batched quads

UI should NEVER:

* break render batching
* flush GPU pipelines excessively

---

# 35. Debug Rendering

Dedicated debug passes:

* physics bounds
* visibility clusters
* ECS overlays
* profiler graphs
* navigation fields

Debug rendering must be isolated from shipping pipelines.

---

# 36. Renderer Thread Model

Final threading model:

```text
Main Thread
    ↓
Simulation Thread
    ↓
Render Submission Thread
    ↓
GPU Queue
```

Streaming workers run independently.

---

# 37. Failure Conditions

The renderer must defend against:

```text
VRAM exhaustion
descriptor exhaustion
pipeline explosion
shader permutation explosion
draw call explosion
synchronization stalls
```

---

# 38. Hard Performance Constraints

Target:

| Metric            | Budget         |
| ----------------- | -------------- |
| CPU frame         | < 8ms          |
| GPU frame         | < 8ms          |
| Draw calls        | < 5k           |
| Visible instances | 1M+            |
| VRAM budget       | 4–8GB          |
| Texture switches  | minimized      |
| Material count    | heavily reused |

---

# 39. Renderer Design Goals

The renderer exists to achieve:

```text
Stable frame pacing
Minimal synchronization
Maximum batching
GPU-driven visibility
Scalable streaming
Low CPU overhead
```

NOT:

```text
maximum visual complexity at any cost
```

---

# 40. Final Renderer Philosophy

Acheron’s renderer is fundamentally:

```text
A visibility reduction architecture
```

The renderer’s primary job is NOT:

```text
drawing objects
```

Its primary job is:

```text
preventing unnecessary work
```

Every system therefore optimizes for:

```text
contiguous memory
GPU batching
visibility reduction
resource reuse
asynchronous execution
minimal synchronization
predictable frame pacing
```
