# Project Acheron — Assets Manifest & Migration Plan

This document defines the asset filtering logic, approved/rejected asset mappings, final target structure, and automated extraction strategy for Phase 1.

---

## 1. Source ZIP File Inventory

Six asset ZIP files are staged in the user's Downloads folder (`C:\Users\Harish\Downloads\`):

1. **`Orbitron.zip`** (High-tech geometric sans-serif fonts)
2. **`kenney_ui-pack.zip`** (Sleek UI panels, borders, buttons, arrows)
3. **`kenney_tiny-town.zip`** (16x16 pixel art tilesets, road tiles, and micro vehicles)
4. **`kenney_platformer-art-buildings.zip`** (Modular building panels, roofs, awnings, and decals)
5. **`cyberpunk_city_2_files.zip`** (Neon city background layers, animated neon props, cyberpunk tileset)
6. **`kenney_input-prompts-pixel-1-bit.zip`** (16x16 pixel control layout key icons)

---

## 2. Visual Filtering Plan

Acheron's theme is a **"Minimal cyberpunk urban simulation sandbox."** The visual filter restricts asset ingestion to keep the repository lightweight, performance-focused, and free of irrelevant fantasy clutter.

### Approved Assets (To Be Extracted)
* **Fonts**: `Orbitron-Regular.ttf`, `Orbitron-Medium.ttf`, `Orbitron-Bold.ttf`, `Orbitron-Black.ttf` (from Orbitron.zip).
* **UI Packs**: Grey panels, button outlines, directional indicators, transparent text bars (from kenney_ui-pack/Grey/Double).
* **Environment Tiles**: Neon city tileset.png, concrete walkways, industrial floors (from cyberpunk_city_2_files.zip).
* **Building Components**: Vent shafts, chimneys, green/red awnings, metallic panel units, modular roof boxes (from kenney_platformer-art-buildings.zip).
* **Road Systems**: Grid-based intersection lines, crosswalk indicators, concrete road lane tiles (from kenney_tiny-town.zip).
* **Simulation Icons**: Keyboard prompts (Space, 1, 2, 3, T, E, C, D, H) for controls mapping (from kenney_input-prompts-pixel-1-bit.zip).
* **Micro Vehicles**: Small cars, utility freight trucks, transit cabs (from kenney_tiny-town.zip).

### Rejected Assets (To Be Ignored / Excluded)
* **Characters / Entities**: Cop character sprite animations, player animations, enemy grunt prefabs, loot chests, weapons, and bullet particles.
* **Fantasy Elements**: Town castles, castle flags, dungeon assets, medieval wooden signs, trees, farms, cows, and grass decorative tiles.
* **Music / Large Audio**: Giant MP3/WAV/OGG soundtracks inside the cyberpunk pack (~36MB total) to prevent project bloating.
* **Aseprite Source Files**: `.ase` and `.aseprite` compilation sources (retained ONLY for off-line developer reference, not included in runtime build).
* **OS Metadata**: Mac OS resource fork files (`__MACOSX/` directories).

---

## 3. Final Asset Folder Mapping

The organized assets will reside under the standard `assets/` structure:

```text
assets/
├── fonts/          <-- TTF fonts for interface, profilers, titles
├── ui/             <-- Panels, button borders, panel backgrounds
├── icons/          <-- Pixel keyboard key icons for UI dashboard legend
├── tiles/          <-- Core city tile layouts and ground tileset
├── buildings/      <-- Modular building assets, roof units, HVAC boxes
├── roads/          <-- Concrete intersection tiles
├── vehicles/       <-- Car and cargo truck pixel sprites
├── overlays/       <-- Stored procedural heatmaps (rendered at runtime)
├── particles/      <-- Stored procedural particle configurations (runtime)
├── shaders/        <-- Emissive glow shaders (loaded on demand)
└── generated/
    └── svg/        <-- Lightweight vector symbols and indicator sheets (reference)
```

---

## 4. Migration File Mapping

The following mapping defines the extraction source paths to final destination paths:

| Source ZIP | Source Entry Path | Target Project Destination | Naming Convention |
| :--- | :--- | :--- | :--- |
| **Orbitron.zip** | `static/Orbitron-*.ttf` | `assets/fonts/` | `orbitron_[weight].ttf` |
| **kenney_ui-pack.zip** | `PNG/Grey/Double/*.png` | `assets/ui/` | `ui_grey_[element].png` |
| **kenney_tiny-town.zip** | `Tilemap/tilemap_packed.png` | `assets/tiles/` | `tileset_tiny_town.png` |
| **kenney_tiny-town.zip** | `Tiles/tile_0049.png` to `0055.png` (Cars) | `assets/vehicles/` | `vehicle_[color].png` |
| **kenney_platformer-art-buildings.zip** | `Tiles/awning*.png`, `chimney*.png` | `assets/buildings/` | `building_prop_[name].png` |
| **cyberpunk_city_2_files.zip** | `Environment/tileset.png` | `assets/tiles/` | `tileset_cyberpunk.png` |
| **cyberpunk_city_2_files.zip** | `Environment/props/lights/*.png` | `assets/buildings/` | `building_glow_[name].png` |
| **kenney_input-prompts-pixel-1-bit.zip** | `Tiles (Black)/tile_*.png` (selected keys) | `assets/icons/` | `icon_key_[char].png` |

---

## 5. Migration Execution Strategy (Phase 1)

When entering Phase 1, migration will be completed using a single, efficient, automated PowerShell extraction script. 

### Extraction Script Concept:
1. Create target directories dynamically.
2. Read the ZIP files directly from Downloads using `System.IO.Compression.ZipArchive`.
3. Filter entries based on string-matching approved lists (e.g. ignoring `Music`, `__MACOSX`, `.ase` extensions).
4. Extract files and rename them to conform to the naming conventions.
5. Verify that all target asset files exist and have non-zero file sizes.
