# visual_guide.md

# Project Acheron — Visual Direction & Rendering Identity Guide

## 1. Purpose

This document defines the visual identity of Acheron.

It exists to:

* maintain visual consistency
* guide procedural generation
* constrain asset production
* support renderer scalability
* define lighting language
* establish material taxonomy
* standardize environmental storytelling

The visual guide is treated as:

```text
technical rendering specification
```

Not:

```text
concept art inspiration board
```

---

# 2. Core Visual Philosophy

Acheron is visually defined by:

```text
industrial scale
systemic density
urban pressure
procedural repetition
controlled visual noise
```

The world should feel:

* operational
* crowded
* infrastructural
* mechanically alive
* economically stressed
* rain-soaked
* electrically saturated

The environment should communicate:

```text
"a city optimized beyond human comfort"
```

---

# 3. Primary Artistic Pillars

## Pillar 1 — Functional Architecture

Buildings exist for:

* logistics
* habitation
* manufacturing
* transportation
* utility routing

Avoid decorative fantasy architecture.

Every structure should imply:

* purpose
* maintenance
* scalability
* infrastructure dependency

---

## Pillar 2 — Modular Repetition

The city must visually support:

* procedural assembly
* GPU instancing
* mesh batching
* atlas reuse

Therefore:

* repeated window patterns
* repeated wall modules
* repeated signage systems
* repeated infrastructure units

Visual repetition is intentional.

---

## Pillar 3 — Layered Density

The environment should contain:

* overlapping infrastructure
* stacked transit systems
* suspended utility lines
* ventilation systems
* signage clusters
* dense road layering

The player should visually perceive:

```text
continuous expansion pressure
```

---

# 4. World Tone

Primary emotional tone:

```text
controlled urban collapse
```

Secondary tone:

```text
efficient systemic brutality
```

The world should feel:

* optimized
* overstressed
* industrialized
* monetized
* automated

Avoid:

* whimsical visuals
* clean utopian aesthetics
* fantasy ornamentation
* excessive organic forms

---

# 5. Environmental Style

## Architecture Style

Primary influences:

* brutalism
* industrial megastructures
* modular transit infrastructure
* logistics hubs
* high-density vertical zoning

Geometry language:

* hard edges
* rectangular silhouettes
* exposed structural beams
* stacked modules
* visible maintenance systems

---

# 6. Shape Language

## Dominant Shapes

| Shape             | Purpose                 |
| ----------------- | ----------------------- |
| Rectangles        | buildings and roads     |
| Cylinders         | pipes and utilities     |
| Hex grids         | infrastructure overlays |
| Vertical towers   | density communication   |
| Layered platforms | transportation systems  |

Avoid highly curved architecture.

---

# 7. Visual Hierarchy

Scene readability priority:

```text
roads
→ vehicles
→ signage
→ buildings
→ distant skyline
```

The player should immediately identify:

* traffic direction
* active districts
* infrastructure stress
* transportation flow

---

# 8. Color Palette

## Primary Palette

| Category            | Color Direction       |
| ------------------- | --------------------- |
| Concrete            | cold gray             |
| Steel               | dark desaturated blue |
| Asphalt             | wet charcoal          |
| Utility Lighting    | cyan                  |
| Hazard Lighting     | amber                 |
| Commercial Lighting | magenta               |
| Transit Signals     | neon blue             |

---

# 9. Lighting Philosophy

Lighting exists primarily to:

* define depth
* expose density
* communicate activity
* emphasize infrastructure

The world should contain:

* strong contrast
* localized emissive lighting
* atmospheric fog
* volumetric rain diffusion

Avoid globally even lighting.

---

# 10. Time-of-Day Strategy

Primary visual target:

```text
night + rain
```

Reasons:

* emphasizes emissive materials
* improves atmosphere density
* reduces distant visual complexity
* supports reflective surfaces
* increases mood coherence

Secondary supported times:

* overcast dawn
* polluted sunset
* industrial fog mornings

---

# 11. Weather Identity

Weather should reinforce:

```text
industrial discomfort
```

Primary weather systems:

* rain
* smog
* fog
* steam vents
* particulate haze

Weather affects:

* lighting diffusion
* traffic visibility
* reflections
* atmosphere depth

---

# 12. Surface Material Rules

All materials should imply:

* wear
* maintenance cycles
* environmental stress
* moisture exposure
* industrial usage

Avoid perfectly clean surfaces.

---

# 13. Material Categories

## Primary Materials

| Material              | Visual Purpose         |
| --------------------- | ---------------------- |
| Wet asphalt           | traffic readability    |
| Reinforced concrete   | structural mass        |
| Rusted steel          | industrial age         |
| Brushed metal         | utility infrastructure |
| Neon glass            | commercial density     |
| Painted hazard panels | navigation readability |

---

# 14. Texture Rules

Textures must support:

* tiling
* atlas packing
* procedural variation
* low memory usage
* mip streaming

Avoid:

* unique 4K textures per object
* baked directional lighting
* noisy detail maps

---

# 15. Texture Density Standards

| Asset Type | Resolution         |
| ---------- | ------------------ |
| Buildings  | 1K–2K              |
| Props      | 512–1K             |
| Roads      | 2K                 |
| Decals     | 256–512            |
| UI         | vector or high DPI |

Visual consistency matters more than raw resolution.

---

# 16. Building Construction Rules

Buildings are assembled from:

* walls
* windows
* roof units
* support beams
* signage mounts
* HVAC modules
* utility connectors

Never author entire unique skyscrapers.

Procedural assembly is mandatory.

---

# 17. Procedural District Identity

Each district requires:

* palette variation
* density variation
* signage style
* road complexity
* utility saturation
* ambient fog intensity

Districts should remain recognizable at distance.

---

# 18. District Categories

## Industrial

Characteristics:

* smoke
* steel
* exposed pipes
* orange hazard lighting
* dense machinery

## Commercial

Characteristics:

* emissive signage
* reflective glass
* crowded roads
* animated billboards

## Residential

Characteristics:

* stacked apartments
* tighter roads
* softer lighting
* visible habitation density

---

# 19. Signage Philosophy

Signage is a major visual identity layer.

Purpose:

* visual noise
* navigation guidance
* district branding
* economic storytelling

Use:

* modular decal atlases
* emissive materials
* procedural variation

---

# 20. Road Design Rules

Roads communicate:

* city scale
* traffic pressure
* transportation hierarchy

Road hierarchy:

| Type             | Width   |
| ---------------- | ------- |
| Mega transit     | extreme |
| Freight lanes    | wide    |
| Standard streets | medium  |
| Service alleys   | narrow  |

Wet roads are critical to atmosphere.

---

# 21. Vehicle Visual Rules

Vehicles should:

* prioritize silhouette readability
* remain modular
* support instancing
* avoid excessive uniqueness

Traffic density matters more than vehicle complexity.

---

# 22. Crowd Visual Rules

NPCs exist to:

* communicate population pressure
* reinforce scale
* increase motion density

NPC detail should remain minimal.

LOD transitions must be aggressive.

---

# 23. Environmental FX

Primary effects:

* rain streaks
* steam vents
* emissive flicker
* fog layers
* traffic light reflections
* electrical sparks

FX should support atmosphere rather than spectacle.

---

# 24. Camera Philosophy

The camera should emphasize:

* vertical scale
* traffic movement
* infrastructure layering
* skyline density

Wide-angle environmental framing preferred.

---

# 25. UI Visual Direction

UI style:

* industrial
* data-driven
* infrastructure-oriented
* minimal ornamentation

Use:

* thin typography
* grid layouts
* system colors
* layered transparency

Avoid fantasy UI themes.

---

# 26. Typography Rules

Typography should communicate:

* system operations
* transit control
* infrastructure analytics

Recommended:

* geometric sans-serif
* condensed industrial fonts
* monospaced debug overlays

---

# 27. Rendering Priorities

Renderer optimization priorities:

```text
GPU instancing
→ batching
→ texture atlases
→ clustered lighting
→ streaming
→ post-processing
```

Atmosphere should come from:

* composition
* lighting
* density

Not excessive shader complexity.

---

# 28. Post-Processing Rules

Allowed:

* bloom
* fog
* tone mapping
* screen-space reflections
* chromatic diffusion

Avoid:

* excessive blur
* aggressive chromatic aberration
* noisy sharpening

---

# 29. Visual Noise Management

Dense scenes require controlled contrast.

Use:

* focal lighting
* color grouping
* district zoning
* emissive isolation

Avoid visual overload.

---

# 30. Asset Naming Standards

Examples:

```text
building_wall_industrial_a
road_asphalt_wet_01
sign_neon_transit_blue
vehicle_freight_medium_b
```

Never use inconsistent naming.

---

# 31. LOD Rules

Every asset requires:

* LOD0
* LOD1
* LOD2
* optional billboard

LOD transitions must prioritize:

* silhouette preservation
* stable lighting
* minimal popping

---

# 32. Streaming Rules

Visual assets must support:

* asynchronous loading
* chunk streaming
* residency tracking
* distance-based eviction

The world should never fully load at once.

---

# 33. Technical Art Constraints

Target constraints:

| System           | Goal           |
| ---------------- | -------------- |
| Draw Calls       | minimized      |
| Materials        | heavily reused |
| Texture Switches | minimized      |
| VRAM Residency   | controlled     |
| Shader Variants  | limited        |

Scalability is more important than visual excess.

---

# 34. Audio-Visual Relationship

Visual atmosphere should synchronize with:

* traffic hum
* rain ambience
* electrical noise
* distant sirens
* industrial machinery

The city should feel continuously operational.

---

# 35. Final Artistic Principle

Acheron should visually communicate:

```text
infrastructure under pressure
```

The city is not:

```text
a handcrafted fantasy world
```

It is:

```text
a procedural industrial organism
```

Every visual system should reinforce:

* scale
* repetition
* pressure
* density
* systemic dependency
* mechanical realism
