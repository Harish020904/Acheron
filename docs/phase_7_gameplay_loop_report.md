# Phase 7: Gameplay Loop & Systemic Pressure

## Gameplay Flow
Acheron revolves around maintaining the delicate balance of an expanding urban center.
1. Generate the initial city layout.
2. Watch as districts dynamically spawn traffic, moving citizens to commercial/industrial zones.
3. Use wealth to upgrade districts (Press `U`), increasing capacity but generating more pressure.
4. Manually drag roads between disconnected districts (Mouse Left-Click Drag) to ease congestion.
5. Cycle zone types (Press `Z`) to balance the RCI (Residential, Commercial, Industrial) demand.

## Progression Logic
The `ProgressionSystem` automatically unlocks global tiers (High Density, Public Transit, Automation) as `total_population` and `total_income` cross thresholds. These upgrades increase global stability caps and allow higher-tier districts.

## Pressure Systems
- **Congestion**: Vehicles traveling on sparse road networks rapidly increase road congestion.
- **Instability**: High traffic blocks economic trade, causing district `instability` to climb.
- **Economy**: Poorly connected commercial zones will fail to generate wealth, causing the overarching treasury to stagnate.

## Interaction Systems
- `InteractionSystem` resolves mouse coordinates to grid cells.
- **Left-Click + Drag**: Connects roads between orthogonal districts.
- **Z**: Cycles district zoning (Residential → Commercial → Industrial).
- **U**: Upgrades district level (costs 500 wealth).
- **D**: Spawns a new district on an empty tile.

## Collapse Mechanics
When a district's `instability` exceeds `0.8f`, the district physically flashes red on the map. Surrounding roads with `congestion > 0.9f` also strobe violently. If ignored, the district will cease producing wealth, cascading into a total economic crash. The player must intervene by bulldozing/rezoning or laying redundant traffic routes.
