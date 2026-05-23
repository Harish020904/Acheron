# ai_behaviors.md

## Purpose

This document defines the complete AI architecture for Acheron.

The AI system is NOT:

* scripted NPC logic
* Unity-style MonoBehaviour AI
* per-agent behavior trees attached to objects

The AI system IS:

* a data-oriented simulation layer
* a massively parallel decision engine
* a deterministic ECS-driven behavior framework
* a multi-resolution urban population simulator

The architecture must support:

```text
100,000–1,000,000 active agents
```

while maintaining:

* stable frame pacing
* deterministic updates
* SIMD-friendly traversal
* minimal branch divergence
* cache-coherent memory access

---

# Core AI Philosophy

The simulation does NOT attempt:

* cinematic realism
* handcrafted personalities
* fully simulated cognition

Instead, it prioritizes:

```text
Believable emergent macro behavior
```

The illusion of intelligence is created through:

* layered systems
* flow-field navigation
* utility scoring
* local reactions
* schedule simulation
* traffic pressure
* economic pressure
* environmental stimuli

---

# AI System Hierarchy

The AI stack operates in 5 layers:

```text
Strategic Layer
    ↓
District Layer
    ↓
Crowd Layer
    ↓
Agent Layer
    ↓
Reaction Layer
```

---

# 1. Strategic Layer

Update Frequency:

```text
0.2–1 Hz
```

Purpose:

* city-wide simulation decisions
* macro population balancing
* district pressure propagation
* economy-driven migration
* congestion prediction

Examples:

* population migration
* work district overload
* transport saturation
* emergency rerouting
* police/medical redistribution

This layer NEVER updates individual NPCs.

It manipulates:

* statistical populations
* district metrics
* route pressure maps

---

# 2. District Layer

Update Frequency:

```text
1–5 Hz
```

Purpose:

* district behavior synthesis
* local crowd pressure
* danger propagation
* event activation
* density balancing

Each district maintains:

```cpp
struct DistrictState
{
    float population_density;
    float crime_level;
    float traffic_pressure;
    float economic_strength;
    float police_presence;
    float noise_level;
};
```

District systems generate:

* local crowd targets
* ambient NPC density
* traffic demand
* dynamic events

---

# 3. Crowd Layer

Update Frequency:

```text
5–15 Hz
```

Purpose:

* crowd flow
* lane following
* pedestrian density
* local movement fields
* traffic synchronization

This layer controls:

* thousands of agents simultaneously
* group behavior
* directional propagation

This is NOT per-agent pathfinding.

Crowd movement uses:

* flow fields
* density grids
* directional pressure maps

---

# 4. Agent Layer

Update Frequency:

```text
10–30 Hz
```

Purpose:

* local decision making
* utility evaluation
* schedule execution
* nearby interaction

Agents evaluate:

* destination
* danger
* congestion
* needs
* work schedules
* sleep cycles
* shopping routines

Agents are statistical entities first.

NOT cinematic actors.

---

# 5. Reaction Layer

Update Frequency:

```text
30–60 Hz
```

Purpose:

* immediate reactions
* collision avoidance
* animation switching
* local steering
* obstacle response

This layer must remain extremely lightweight.

---

# ECS AI Components

AI is entirely ECS-driven.

No inheritance.

No virtual dispatch.

---

# Core AI Components

```cpp
struct AgentStateComponent
{
    uint16_t current_state;
    uint16_t previous_state;
};

struct UtilityComponent
{
    float hunger;
    float fatigue;
    float stress;
    float fear;
};

struct NavigationComponent
{
    uint32_t flow_field_id;
    uint32_t current_cell;
};

struct ScheduleComponent
{
    uint32_t current_task;
    float task_timer;
};

struct PerceptionComponent
{
    float vision_range;
    float hearing_range;
};

struct CrowdComponent
{
    uint16_t crowd_group;
    uint16_t crowd_role;
};
```

All components MUST remain:

* trivially copyable
* SIMD-compatible
* fixed-size

---

# AI Update Model

The AI system NEVER updates all agents every frame.

Instead:

```text
Agents are partitioned into update buckets
```

Example:

```text
Frame 1 → Bucket A
Frame 2 → Bucket B
Frame 3 → Bucket C
```

This amortizes:

* CPU load
* cache pressure
* scheduler spikes

---

# AI State Machine Design

Acheron uses:

```text
Data-driven finite state machines
```

NOT:

* behavior trees
* GOAP
* utility-AI-only systems

Reason:
Behavior trees become:

* branch-heavy
* cache-unfriendly
* difficult to vectorize

---

# Agent States

Core states:

```text
Idle
Walking
Working
Shopping
Resting
Eating
Driving
Panicking
Fleeing
Investigating
Waiting
Commuting
Sleeping
Socializing
```

States stored as:

```cpp
enum class AgentState : uint16_t
{
    Idle,
    Walking,
    Working,
    Shopping,
    Resting
};
```

Fixed-width enums improve:

* cache density
* SIMD comparisons
* predictable branching

---

# Utility Scoring System

Agents use lightweight utility scoring.

Example:

```cpp
float EvaluateFoodNeed()
{
    return hunger * 0.8f
         + stress * 0.2f;
}
```

The highest utility wins.

Advantages:

* deterministic
* SIMD-friendly
* no recursive tree traversal
* easy to parallelize

---

# Navigation Architecture

Acheron NEVER uses:

```text
A* per NPC
```

Impossible at scale.

Instead:

* district flow fields
* local steering
* lane grids
* directional maps

---

# Flow Field Structure

```cpp
struct FlowFieldCell
{
    uint16_t integration_cost;
    uint8_t direction;
    uint8_t flags;
};
```

Each district owns:

* pedestrian fields
* vehicle fields
* emergency reroute fields

---

# Crowd Simulation

Crowds behave as:

```text
density systems
```

NOT:

```text
independent intelligent humans
```

Each crowd maintains:

* directional momentum
* local density
* panic propagation
* congestion pressure

---

# Panic Propagation

Panic spreads using:

```text
neighbor influence propagation
```

Example:

```cpp
stress += nearby_panic * 0.35f;
```

This creates:

* emergent stampedes
* believable crowd reactions
* scalable disaster behavior

without expensive scripting.

---

# Perception System

Perception uses:

* spatial partitions
* broadphase queries
* perception masks

Agents DO NOT raycast continuously.

---

# Perception Query

```cpp
struct PerceptionEvent
{
    uint32_t stimulus_type;
    float intensity;
    Vector2 position;
};
```

Stimuli include:

* explosions
* traffic accidents
* gunshots
* alarms
* collisions
* crowd panic

---

# AI Scheduling System

Each citizen operates on:

```text
daily schedules
```

Example:

```text
06:00 Wake
07:00 Commute
08:00 Work
18:00 Return Home
20:00 Leisure
23:00 Sleep
```

Schedules generate:

* believable traffic waves
* district transitions
* economic demand cycles

---

# Time Compression

Far-away agents are simulated statistically.

Close agents become:

```text
high-fidelity agents
```

This is critical.

---

# Simulation Levels

| Level | Detail                 |
| ----- | ---------------------- |
| L0    | Full local simulation  |
| L1    | Simplified steering    |
| L2    | Crowd aggregation      |
| L3    | Statistical simulation |

---

# AI LOD System

AI fidelity scales based on:

* camera distance
* gameplay relevance
* district activity
* visibility

This reduces:

* CPU cost
* scheduler pressure
* memory bandwidth

---

# Traffic AI

Vehicle AI uses:

* lane graphs
* traffic pressure
* density fields
* predictive braking

NOT:

* rigid body driving simulation

---

# Vehicle Components

```cpp
struct VehicleAIComponent
{
    uint32_t lane_id;
    float desired_speed;
    float braking_factor;
};
```

---

# Combat AI

Combat AI exists only for:

* police
* security
* hostile factions

Civilian AI avoids combat complexity.

Combat logic:

* utility-driven
* cover evaluation
* threat scoring
* suppression response

---

# Threat Evaluation

```cpp
float threat =
    visible_enemies * 0.5f
  + recent_damage * 0.3f
  + nearby_explosions * 0.2f;
```

---

# Police AI

Police systems operate hierarchically.

Command Layer:

* district dispatch

Unit Layer:

* squad movement

Agent Layer:

* local engagement

---

# AI Memory Constraints

Per-agent memory budget target:

```text
64–128 bytes
```

Maximum:

```text
256 bytes
```

This is critical.

1 million agents at:

```text
1 KB each
```

equals:

```text
~1 GB RAM
```

Unacceptable.

---

# AI Parallelization

AI updates execute through:

* worker jobs
* chunk iteration
* SIMD traversal

Chunk size target:

```text
1024–4096 agents
```

per task.

---

# SIMD Optimization Rules

Avoid:

* virtual calls
* deep branching
* pointer chasing
* dynamic allocation

Prefer:

* SoA layouts
* packed arrays
* contiguous traversal
* fixed-size state blocks

---

# Agent Query Example

GOOD:

```cpp
for (size_t i = 0; i < count; ++i)
{
    stress[i] += danger[i];
}
```

BAD:

```cpp
agent->brain->evaluate();
```

---

# Determinism Requirements

Simulation must remain deterministic.

Required for:

* replay systems
* debugging
* multiplayer synchronization
* simulation validation

Avoid:

* unordered floating operations
* nondeterministic threading
* unstable iteration order

---

# Debug Visualization

AI debug overlays must support:

```text
Flow fields
District pressure
Crowd density
Stress maps
Traffic routing
Pathing heatmaps
Perception events
```

These are essential.

Without them:
AI debugging becomes impossible.

---

# Performance Targets

| System             | Budget  |
| ------------------ | ------- |
| Crowd AI           | 2–4 ms  |
| Navigation         | 1–2 ms  |
| Utility Evaluation | 1 ms    |
| Traffic AI         | 2–5 ms  |
| Total AI           | < 10 ms |

Target hardware:

```text
Mid-range desktop CPU
8-core modern processor
```

---

# Future Extensions

Potential advanced systems:

* social graph simulation
* criminal networks
* faction influence
* propaganda spread
* disease simulation
* dynamic migration
* adaptive economies
* machine-learned traffic prediction

These are OPTIONAL.

Core architecture stability comes first.

---

# Final Principle

The AI system exists to create:

```text
Emergent Urban Believability
```

NOT:

```text
Perfect Human Intelligence
```

The player should perceive:

* systemic pressure
* crowd momentum
* city-scale behavior
* believable reactions
* living infrastructure

while the engine internally optimizes for:

```text
cache locality
SIMD traversal
minimal synchronization
predictable branching
deterministic execution
```
