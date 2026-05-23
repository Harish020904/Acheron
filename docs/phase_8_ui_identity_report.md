# Phase 8: UI, Overlays & Cyberpunk Identity

## Minimalist Cyberpunk Aesthetic
Acheron aggressively avoids cartoon graphics, fantasy UI, and overwhelming menus. It models itself as a high-end urban infrastructure management terminal.

## Main HUD
The `UIRenderer` draws a thin-line terminal UI utilizing the `Orbitron` font.
- Top-Left: FPS, Total Districts, Total Roads, Active Simulation Speed (`1x`, `2x`, etc.).
- Center: Large red `PAUSED` indicator when spacebar is pressed.
- Top-Right: The current active overlay mode (e.g., `OVERLAY: ECONOMY`).

## Cyberpunk Panels
- UI panels use transparent dark-gray backgrounds (`Fade(palette::ui_panel_bg, 0.8f)`) with sharp, 1px bright borders (`palette::ui_border`).
- Neon accents (`palette::ui_accent`) are used exclusively for titles and critical graph lines.

## Overlay System
Players press `1-4` to instantly toggle Data Heatmaps:
- **Traffic (1)**: Brightness represents congestion.
- **Economy (2)**: Brightness represents wealth.
- **Instability (3)**: Brightness represents collapse pressure.
- **Progression (4)**: Brightness represents district level.

## Map Readability & Minimal Effects
We prioritized high-contrast, deterministic readability over AAA graphics:
- **Rain Streaks**: Procedural diagonal lines in `OverlayRenderer` that simulate rain without particle systems.
- **Industrial Smog**: Semi-transparent rectangular "smoke" rises from industrial zones.
- **Background Haze**: A scrolling parallax grid provides a sense of speed and depth behind the city matrix.
