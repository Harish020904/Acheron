# Phase 6: Core City Visualization & World Integration

## ECS/Render Integration
The city is fully visualized by passing read-only snapshots of the ECS `World` to the independent rendering passes (`TileRenderer`, `RoadRenderer`, `OverlayRenderer`). The ECS remains entirely detached from Raylib structs, ensuring strict data immutability during the rendering cycle. 

## Tilemap Structure
The `CityGenerator` procedurally spaces districts on a 2D grid (`CELL_PX = 64.0f`), leaving strategic organic gaps. Districts are drawn as inset padded rectangles, leaving visual corridors for infrastructure without relying on heavy tile atlases.

## District Rendering Logic
Districts evolve based on progression (levels 1-4).
- **Residential**: Cyan (`SKYBLUE`) hue.
- **Commercial**: Neon Blue (`economic_growth`) hue. They possess a pulsing emissive flicker.
- **Industrial**: Orange (`industrial_zone`) hue. Emits procedural rising smoke in the overlay pass.
Higher-level districts physically glow brighter, using a multiplier on Raylib's `Fade` alpha parameter.

## Traffic Visualization Logic
Roads are drawn dynamically between valid `RoadComponent` connections.
- **Flow**: White/Cyan moving dots (vehicles) interpolate from `road.from` to `road.to` using a time-based modulo formula offset by the vehicle index.
- **Congestion**: Base road colors transition from Green (Low) → Yellow (Medium) → Red (Severe) based on `road.congestion`.
- **Reaction**: At `congestion > 0.9f`, the entire road segment flashes bright red rapidly to signal infrastructural collapse.

## Economy Overlay Logic
The `OverlayRenderer` cycles through heatmaps. The economy view pulses cyan over high-wealth districts, while instability pulses red over dissatisfied districts. 

## Renderer Architecture
- **Immediate Mode**: Renderers use Raylib 2D shapes (rectangles, lines, circles) tightly batched.
- **Layers**: 
  1. `SimpleRenderer` (Grid, Camera)
  2. `RoadRenderer` (Lines, Traffic Dots)
  3. `TileRenderer` (District Blocks)
  4. `OverlayRenderer` (Smoke, Rain, Heatmaps)
  5. `UIRenderer` (HUD, Profiler)
