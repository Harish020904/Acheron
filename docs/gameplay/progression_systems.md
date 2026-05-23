# progression_systems.md

# Project Acheron — Progression Systems Architecture

## 1. Purpose

The progression system in Acheron is not designed as a traditional RPG “level-up” mechanic.

It exists to:

* regulate simulation complexity
* unlock systemic capabilities
* gate economic and infrastructural scale
* create long-term simulation pressure
* provide deterministic growth vectors
* expose increasing computational stress on the engine

The progression layer is therefore treated as a data-driven simulation system rather than a gameplay-only feature.

---

# 2. Core Progression Philosophy

The progression architecture follows four principles:

1. Horizontal expansion over linear stat inflation
2. Simulation complexity as progression
3. Infrastructure unlocks over arbitrary XP rewards
4. Deterministic and data-oriented state transitions

The player does not simply become “stronger.”

The city becomes:

* denser
* more unstable
* more computationally expensive
* harder to optimize
* economically interconnected
* traffic constrained
* resource dependent

Progression therefore acts as a controlled escalation of systemic complexity.

---

# 3. High-Level Progression Layers

The progression system is divided into independent layers.

```text
District Progression
Infrastructure Progression
Logistics Progression
Economic Progression
Technology Progression
Population Progression
Simulation Difficulty Progression
```

Each layer modifies simulation state independently.

---

# 4. Progression Architecture

## Core Rule

Progression state must be:

* serializable
* deterministic
* SIMD-friendly
* event-driven
* data-oriented
* replay-safe

No progression logic should depend on:

* frame timing
* UI state
* rendering order
* random transient runtime values

---

# 5. Data Ownership

Progression state is owned by:

```text
simulation/progression/
```

The renderer never owns progression state.

UI only consumes read-only snapshots.

---

# 6. Primary Runtime Structures

## Global Progression State

```cpp
struct ProgressionState
{
    uint64_t total_population;
    uint64_t total_income;
    uint64_t total_trade_volume;

    uint32_t district_level;
    uint32_t logistics_level;
    uint32_t infrastructure_level;
    uint32_t technology_level;

    float city_stability;
    float resource_pressure;
};
```

The structure must remain POD-compatible.

No virtual methods.

No heap allocations.

---

# 7. Progression Categories

## 7.1 District Progression

District progression unlocks:

* higher density zoning
* new building archetypes
* larger traffic capacity
* more complex simulation layers
* district specialization

District progression is based on:

```text
population density
service coverage
economic stability
traffic efficiency
pollution thresholds
```

---

## 7.2 Logistics Progression

Unlocks:

* cargo hubs
* train infrastructure
* drone networks
* automated routing
* high-speed freight systems

This layer modifies:

```text
pathfinding graphs
resource throughput
vehicle density
traffic pressure
```

---

## 7.3 Technology Progression

Technology progression unlocks:

* simulation optimizations
* economic modifiers
* infrastructure efficiency
* automation systems
* predictive routing

Technology progression modifies:

```text
production coefficients
traffic efficiency
resource consumption
pollution reduction
```

---

## 7.4 Economic Progression

Economic progression determines:

* market complexity
* trade route depth
* industrial chains
* production dependencies
* taxation models

This progression directly impacts:

```text
resource propagation depth
trade graph complexity
simulation load
```

---

# 8. Experience Model

Acheron does not use traditional XP.

Instead:

```text
System Metrics = Progression Inputs
```

Examples:

| Metric             | Affects                    |
| ------------------ | -------------------------- |
| Traffic throughput | logistics progression      |
| Tax revenue        | infrastructure progression |
| Population growth  | district progression       |
| Resource stability | economic progression       |
| Service uptime     | technology progression     |

---

# 9. Progression Evaluation Frequency

Progression systems should not update every frame.

Correct update frequency:

| System            | Frequency    |
| ----------------- | ------------ |
| Traffic           | 10–30Hz      |
| Economy           | 1–2Hz        |
| Population        | 0.2–1Hz      |
| Technology        | event-driven |
| District upgrades | event-driven |

This avoids unnecessary CPU pressure.

---

# 10. Event-Driven Unlock Architecture

Unlocks are event-driven.

Never poll continuously.

## Example

```cpp
struct ProgressionEvent
{
    ProgressionEventType type;

    uint32_t district_id;

    uint64_t value;
};
```

Example events:

```text
PopulationThresholdReached
TradeVolumeExceeded
TrafficEfficiencyAchieved
PollutionLimitExceeded
TechnologyUnlocked
```

---

# 11. Unlock Definition Format

Unlocks are data-driven.

Stored in:

```text
configs/gameplay/progression_curves.json
```

Example structure:

```json
{
  "district_level_2": {
    "population_required": 5000,
    "stability_required": 0.75,
    "unlock": "high_density_residential"
  }
}
```

No unlock logic should be hardcoded.

---

# 12. Dependency Graph

Progression systems form a directed dependency graph.

```text
Population
    ↓
Economy
    ↓
Infrastructure
    ↓
Logistics
    ↓
District Expansion
```

The graph must remain acyclic.

Circular dependencies create unstable simulation loops.

---

# 13. Scaling Curves

All progression scaling uses curves.

Never use:

```text
hardcoded linear scaling
```

Supported curve types:

* exponential
* logarithmic
* sigmoid
* polynomial
* piecewise

---

# 14. Difficulty Escalation

As progression increases:

```text
simulation complexity increases
```

Examples:

| Progression            | Simulation Impact               |
| ---------------------- | ------------------------------- |
| higher density         | more pathfinding load           |
| advanced logistics     | larger trade graphs             |
| industrial expansion   | increased pollution propagation |
| transportation scaling | higher traffic density          |

The progression system intentionally increases computational pressure.

---

# 15. Population Progression

Population is a primary simulation driver.

Population affects:

* road pressure
* economic throughput
* service demand
* power consumption
* housing requirements
* AI density

Population progression should use chunked simulation.

Never update every citizen individually.

---

# 16. Infrastructure Tiering

Infrastructure exists in tiers.

## Tier Examples

| Tier   | Unlocks                       |
| ------ | ----------------------------- |
| Tier 1 | roads, buses                  |
| Tier 2 | rail systems                  |
| Tier 3 | freight automation            |
| Tier 4 | drone logistics               |
| Tier 5 | predictive autonomous routing |

Tier transitions must trigger:

* renderer updates
* simulation graph rebuilds
* pathfinding regeneration
* economic route recalculation

---

# 17. Resource Pressure System

Progression introduces pressure.

Pressure is represented numerically.

```cpp
struct ResourcePressure
{
    float food;
    float water;
    float energy;
    float housing;
    float transport;
};
```

Pressure modifies:

* citizen satisfaction
* economic stability
* district growth speed
* migration probability

---

# 18. Stability System

City stability acts as a simulation governor.

High instability causes:

* economic collapse
* traffic failure
* service outages
* migration spikes
* crime increases

Stability must be deterministic.

Never tie stability to frame-rate-dependent systems.

---

# 19. Unlock Propagation

Unlocks propagate through simulation systems.

Example:

```text
Unlock Freight Rail
    ↓
Increase cargo throughput
    ↓
Reduce truck density
    ↓
Reduce traffic pressure
    ↓
Increase economic efficiency
```

Progression should create systemic ripple effects.

---

# 20. Serialization Requirements

Progression state must support:

* binary save format
* deterministic replay
* rollback
* multiplayer synchronization

Never serialize pointers.

Only serialize:

* IDs
* values
* deterministic arrays

---

# 21. Save Format Example

```cpp
struct ProgressionSaveData
{
    uint32_t version;

    ProgressionState state;

    uint64_t unlocked_flags;
};
```

---

# 22. Unlock Flags

Use bitsets.

Never store unlocks as strings during runtime.

```cpp
enum UnlockFlags : uint64_t
{
    UnlockRail = 1ULL << 0,
    UnlockDrones = 1ULL << 1,
    UnlockMegaDistricts = 1ULL << 2
};
```

Bitmasks are:

* cache-friendly
* SIMD-friendly
* serialization-safe
* branch-efficient

---

# 23. UI Representation

UI reads immutable snapshots.

UI should never directly modify progression state.

Correct architecture:

```text
Simulation Thread
    ↓
Read-Only Snapshot
    ↓
UI Thread
```

---

# 24. Threading Model

Progression evaluation occurs in simulation phases.

Correct order:

```text
Traffic
→ Economy
→ Resource Pressure
→ Progression Evaluation
→ Unlock Events
→ Snapshot Generation
```

This guarantees deterministic evaluation.

---

# 25. Performance Constraints

Progression systems must:

* avoid heap allocation
* avoid string hashing during runtime
* avoid dynamic polymorphism
* operate on contiguous arrays
* minimize cache misses

Target:

```text
< 0.5ms/frame average progression overhead
```

---

# 26. Telemetry Requirements

Track:

* unlock timing
* economic collapse frequency
* traffic saturation
* player growth curves
* simulation stress spikes

Telemetry feeds balancing.

---

# 27. Balancing Philosophy

The game should trend toward:

```text
controlled instability
```

The city should never become permanently solved.

Late-game progression should introduce:

* larger systemic dependencies
* cascading failures
* throughput bottlenecks
* computational stress

---

# 28. Modding Support

Progression data should be externalized.

Supported moddable files:

```text
progression_curves.json
unlock_tables.json
technology_tree.json
resource_pressure.json
```

Mods should never require recompiling the engine.

---

# 29. Replay Safety

All progression systems must be deterministic.

Never use:

* non-seeded randomness
* system clock values
* floating-point nondeterminism across threads

Required:

```text
same input
→ same progression outcome
```

---

# 30. Long-Term Expansion Hooks

The architecture must support:

* multiplayer economies
* district specialization
* political systems
* climate simulation
* social unrest
* infrastructure sabotage
* AI-controlled corporations

These systems should integrate through event channels.

---

# 31. Future Simulation Layers

Future progression layers may include:

| Layer         | Purpose                      |
| ------------- | ---------------------------- |
| Climate       | environmental stress         |
| Politics      | taxation and governance      |
| Crime         | instability propagation      |
| Automation    | labor replacement            |
| Energy Grid   | distributed power simulation |
| Supply Chains | global logistics             |

The progression system must remain extensible.

---

# 32. Final Engineering Principle

Progression in Acheron is not:

```text
stat inflation
```

It is:

```text
simulation amplification
```

Every unlock increases:

* interconnectedness
* simulation density
* computational pressure
* systemic instability
* optimization requirements

The player progresses by learning to manage increasingly complex systems.
