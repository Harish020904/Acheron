# combat_rules.md

# Acheron Combat Rules

## Deterministic Combat Architecture for Massive-Scale Simulation

---

# 1. Combat Philosophy

Combat is not:

* animation spam
* hitpoint subtraction
* visual effects orchestration

Combat is:

# a deterministic state transition system

The combat architecture exists to solve:

```text id="pjlwm1"
large-scale conflict simulation
deterministic damage resolution
crowd-scale engagement
animation synchronization
network-safe state changes
combat readability at scale
```

Acheron combat must support:

* thousands of simultaneous agents
* simulation-scale traffic conflicts
* riot propagation
* police suppression systems
* district instability
* civilian panic behavior
* infrastructure destruction

The combat system therefore prioritizes:

```text id="pjlwm2"
determinism
scalability
state clarity
cache locality
predictable timing
minimal branching
```

NOT:

```text id="pjlwm3"
cinematic complexity per entity
```

---

# 2. Core Combat Architecture

Combat is divided into:

```text id="pjlwm4"
Intent Phase
    ↓
Target Resolution
    ↓
Hit Validation
    ↓
Damage Calculation
    ↓
State Application
    ↓
Animation/Event Dispatch
    ↓
Reaction Propagation
```

Each phase executes deterministically.

---

# 3. Combat Data Philosophy

Combat data is:

# data-oriented

NOT:

```text id="pjlwm5"
deep inheritance hierarchies
```

Combat operates on:

* contiguous ECS chunks
* SIMD-friendly arrays
* deterministic state buffers

---

# 4. Combat Entity Structure

Combat-capable entities contain:

```cpp id="pjlwm6"
struct CombatComponent
{
    uint16_t health;
    uint16_t stamina;

    uint16_t attackPower;
    uint16_t defense;

    uint16_t stateFlags;
};
```

No virtual methods.
No polymorphic dispatch.

---

# 5. Combat State Machine

Combat uses:

# explicit finite state machines

States:

```text id="pjlwm7"
Idle
Moving
Attacking
Blocking
Stunned
Dodging
Dead
Panicking
Suppressed
```

No implicit animation-driven logic.

---

# 6. Deterministic State Rules

Every combat state must define:

```text id="pjlwm8"
entry conditions
exit conditions
allowed transitions
timing windows
interruptibility
```

Without deterministic transitions:

```text id="pjlwm9"
network desync
replay mismatch
unpredictable AI
```

---

# 7. Simulation Tick Rate

Combat updates at:

```text id="pjlwm10"
30Hz fixed timestep
```

Reason:

* stable hit resolution
* deterministic timing
* predictable rollback/replay

---

# 8. Frame vs Simulation Separation

Visual animation may run at:

```text id="pjlwm11"
60–144Hz
```

Combat logic remains:

```text id="pjlwm12"
fixed simulation frequency
```

Visual interpolation is separate from gameplay logic.

---

# 9. Combat Timing Windows

Every attack contains:

```text id="pjlwm13"
startup frames
active frames
recovery frames
cancel windows
```

Example:

| Phase    | Duration |
| -------- | -------- |
| startup  | 200ms    |
| active   | 100ms    |
| recovery | 300ms    |

---

# 10. Attack Definitions

Combat attacks are pure data:

```cpp id="pjlwm14"
struct AttackDefinition
{
    uint16_t damage;

    uint16_t staminaCost;

    uint16_t startupTicks;

    uint16_t activeTicks;

    uint16_t recoveryTicks;

    uint16_t range;
};
```

This allows:

* data-driven balancing
* SIMD evaluation
* deterministic serialization

---

# 11. Hit Detection Philosophy

Acheron uses:

# broadphase + narrowphase combat checks

NOT:

```text id="pjlwm15"
animation-event-only collision
```

Combat resolution must remain simulation-authoritative.

---

# 12. Broadphase Combat Detection

Broadphase uses:

* spatial grids
* chunk queries
* bounding spheres

Example:

```cpp id="pjlwm16"
struct CombatBounds
{
    float x;
    float y;
    float radius;
};
```

---

# 13. Narrowphase Validation

Narrowphase evaluates:

```text id="pjlwm17"
distance
angle
line-of-sight
facing direction
attack timing
```

---

# 14. Attack Cone Logic

Example melee validation:

```text id="pjlwm18"
distance < range
AND
dot(forward, targetDirection) > threshold
```

Avoid:

```text id="pjlwm19"
per-bone collision checks
```

at simulation scale.

---

# 15. Damage Resolution Pipeline

Damage pipeline:

```text id="pjlwm20"
Attack Event
    ↓
Armor Mitigation
    ↓
Resistance Calculation
    ↓
Critical Evaluation
    ↓
Status Effects
    ↓
Final Damage
```

---

# 16. Damage Formula

Base combat equation:

D = (A - R) \times C

Where:

* D = final damage
* A = attack power
* R = resistance
* C = critical modifier

---

# 17. Stamina System

Combat consumes:

# stamina budget

Purpose:

* pacing
* combat rhythm
* crowd stabilization

Without stamina:

```text id="pjlwm21"
combat degenerates into spam
```

---

# 18. Crowd-Scale Combat

At city scale:
combat becomes:

# behavioral propagation

Example:

```text id="pjlwm22"
gunshot
    ↓
panic spread
    ↓
civilian fleeing
    ↓
traffic congestion
    ↓
district instability
```

Combat affects:

* economy
* traffic
* AI routing
* police response

---

# 19. Suppression System

Heavy combat generates:

# suppression fields

Suppression affects:

* civilian morale
* movement speed
* AI aggression
* crowd density

---

# 20. Status Effects

Effects are:

# compact bitfield-driven modifiers

Example:

```cpp id="pjlwm23"
enum StatusFlags : uint32_t
{
    Burning   = 1 << 0,
    Poisoned  = 1 << 1,
    Stunned   = 1 << 2,
    Suppressed= 1 << 3
};
```

Avoid:

```text id="pjlwm24"
heap-allocated effect objects
```

---

# 21. Combat Events

Combat uses:

# event-driven processing

Example events:

```text id="pjlwm25"
attack_started
attack_hit
damage_applied
entity_stunned
entity_dead
panic_triggered
```

Events stored in:

# lock-free ring buffers

---

# 22. Death Handling

Death pipeline:

```text id="pjlwm26"
health <= 0
    ↓
death state entered
    ↓
physics disabled
    ↓
AI disabled
    ↓
render transition
    ↓
despawn scheduling
```

Never delete entities immediately during iteration.

---

# 23. Combat AI Layers

Combat AI divided into:

```text id="pjlwm27"
Perception
    ↓
Threat Evaluation
    ↓
Decision Selection
    ↓
Action Execution
```

Each layer updates independently.

---

# 24. Threat Evaluation

Threat score example:

T = W_d D + W_h H + W_p P

Where:

* D = distance
* H = hostility
* P = perceived power

---

# 25. Crowd Conflict Scaling

Large riots require:

# aggregate combat simulation

Far-away conflicts may collapse into:

* density models
* probabilistic damage
* statistical suppression

instead of:

* per-agent combat

---

# 26. Animation Integration

Animations are:

# visual consequences

NOT:

# gameplay authorities

Gameplay state drives:

* animation transitions
* hit reactions
* locomotion blends

Never inverse ownership.

---

# 27. Hit Reactions

Reactions include:

```text id="pjlwm28"
stagger
knockback
panic
ragdoll
suppression
```

All deterministic.

---

# 28. Knockback Logic

Knockback formula:

F = \frac{P}{M}

Where:

* F = impulse force
* P = impact power
* M = target mass

---

# 29. Friendly Fire Rules

Combat factions define:

```text id="pjlwm29"
hostile
neutral
civilian
law_enforcement
military
gang
```

Faction matrices determine:

* hostility
* suppression
* response escalation

---

# 30. Weapon Categories

Weapon classes:

| Type         | Characteristics            |
| ------------ | -------------------------- |
| melee        | low range, low suppression |
| ballistic    | high suppression           |
| explosive    | area panic                 |
| energy       | armor penetration          |
| riot_control | morale disruption          |

---

# 31. Projectile Simulation

Projectiles use:

# batched ballistic simulation

NOT:

```text id="pjlwm30"
one heap object per bullet
```

Projectile arrays update via:

* SIMD
* SoA traversal
* chunk simulation

---

# 32. Explosion System

Explosions generate:

```text id="pjlwm31"
damage field
impulse field
suppression field
panic propagation
visibility disruption
```

All radius-based.

---

# 33. Panic Propagation

Panic spreads probabilistically.

Propagation factors:

* proximity
* visibility
* crowd density
* suppression intensity

This creates:

# emergent city behavior

---

# 34. Combat Networking

Future networking requires:

```text id="pjlwm32"
deterministic lockstep
rollback-safe state
stable tick ordering
```

Combat therefore avoids:

* floating nondeterminism
* unstable iteration
* random execution order

---

# 35. Replay Compatibility

Combat events serialize into:

# replay logs

Replays store:

* inputs
* RNG seeds
* combat events

NOT:

```text id="pjlwm33"
full world snapshots every frame
```

---

# 36. Performance Constraints

Combat budget:

| Metric            | Target    |
| ----------------- | --------- |
| combat update     | < 2ms     |
| active combatants | 100k+     |
| projectile count  | 50k+      |
| suppression zones | thousands |
| event processing  | lock-free |

---

# 37. SIMD Combat Evaluation

Combat systems process:

# chunked SoA arrays

Example:

```cpp id="pjlwm34"
float health[4096];
float stamina[4096];
```

Allows:

* AVX vectorization
* branch minimization
* cache-efficient traversal

---

# 38. Memory Constraints

Combat systems avoid:

```text id="pjlwm35"
heap allocations
deep object graphs
pointer-heavy structures
```

Combat memory must remain:

* contiguous
* cache-local
* deterministic

---

# 39. Failure Modes

Combat must defend against:

```text id="pjlwm36"
desynchronization
state corruption
event flooding
infinite stun loops
AI oscillation
projectile explosion
panic runaway
```

---

# 40. Balancing Philosophy

Combat should optimize for:

```text id="pjlwm37"
clarity
predictability
emergent behavior
system interaction
```

NOT:

```text id="pjlwm38"
raw animation spectacle
```

---

# 41. Long-Term Combat Evolution

Future upgrades:

```text id="pjlwm39"
destructible infrastructure
crowd-scale suppression modeling
district morale systems
police escalation simulation
vehicle combat
terrain damage propagation
```

---

# 42. Final Combat Philosophy

Acheron combat is fundamentally:

# a distributed behavioral simulation

NOT:

# isolated duel mechanics

Every combat interaction should influence:

```text id="pjlwm40"
crowd flow
traffic
district stability
economy
AI routing
suppression propagation
```

The combat system therefore exists to create:

```text id="pjlwm41"
emergent systemic instability
```

across the simulated city.
