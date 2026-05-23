# Project Acheron — Visual Font Usage

This document establishes the typographic rules and font usage guidelines for Project Acheron's transition to a stylized, high-performance cyberpunk simulation sandbox.

---

## 1. Font Selection: Orbitron

Orbitron is a futuristic, geometric sans-serif typeface designed by Matt McInerney. It is specifically selected as the primary visual font for Acheron due to its high readability at small sizes, industrial geometric form, and perfect alignment with the "cyberpunk simulation sandbox" aesthetic.

* **Source File**: `C:\Users\Harish\Downloads\Orbitron.zip`
* **Static Assets Staged**:
  * `Orbitron-Regular.ttf` (Primary UI labels, stats, values)
  * `Orbitron-Medium.ttf` (Category headings, standard metrics)
  * `Orbitron-Bold.ttf` (District titles, header bars, critical warnings)
  * `Orbitron-Black.ttf` (Large counters, speed indicators, simulation speed buttons)

---

## 2. Typography Rules by Context

To maintain UI density, extreme screen readability, and aesthetic consistency, the following typographic hierarchy is established:

| Context | Recommended Font Style | Size (px) | Primary Color | Visual Effect | Naming Convention / Tag |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **Main Dashboard Title** | `Orbitron-Bold.ttf` | 24 - 28px | `RAYWHITE` (`#F3F3F3`) | Subtle neon outer glow, upper-case | `UI_TITLE_MAIN` |
| **Sub-panel Headers** | `Orbitron-Medium.ttf` | 16 - 18px | `SKYBLUE` (`#00BDFF`) | Left-border accent bar, upper-case | `UI_HEADER_SUB` |
| **Statistical Labels** | `Orbitron-Regular.ttf` | 14 - 16px | `Fade(RAYWHITE, 0.7f)` | No glow, crisp alignment | `UI_LABEL_STAT` |
| **Statistical Values** | `Orbitron-Medium.ttf` | 14 - 16px | Cyan / Green / Red / Yellow | Color reflects state (e.g. Red for traffic) | `UI_VALUE_STAT` |
| **Profiler Overlay** | `Orbitron-Regular.ttf` | 11 - 12px | `LIME` (`#00FF66`) | Monospaced alignment, semi-transparent panel | `UI_TEXT_PROFILER` |
| **District Level Text** | `Orbitron-Bold.ttf` | 12 - 14px | Dynamic / Dynamic HSL | Rendered on tile background, high contrast | `UI_TEXT_DISTRICT` |
| **Warning / Alerts** | `Orbitron-Bold.ttf` | 12 - 14px | `ORANGE` (`#FF5B00`) or `RED` | Flashing neon pulse (sine wave alpha) | `UI_ALERT_FLASH` |

---

## 3. Implementation Guidelines with Raylib

### Loading Fonts
In raylib, fonts are loaded using `LoadFontEx`. To ensure maximum pixel clarity at multiple scales:
```cpp
// Stored in AssetManager
Font font_regular = LoadFontEx("assets/fonts/Orbitron-Regular.ttf", 16, nullptr, 0);
Font font_medium  = LoadFontEx("assets/fonts/Orbitron-Medium.ttf", 20, nullptr, 0);
Font font_bold    = LoadFontEx("assets/fonts/Orbitron-Bold.ttf", 28, nullptr, 0);
```

### Rendering with Crispness
* **VSync & Integer Coordinates**: To prevent pixel-art filtering artifacts, all text drawing positions MUST be cast to `float` with integer precision (e.g. `floorf(x)`).
* **Text Shadows / Glow**: Simple, high-efficiency glowing outlines should be created by drawing the text multiple times with high transparency and slight offsets, or using shader passes:
  ```cpp
  // Draw subtle glow back-shadow
  DrawTextEx(font, text, Vector2{x - 1, y}, size, spacing, Fade(glow_color, 0.3f));
  DrawTextEx(font, text, Vector2{x + 1, y}, size, spacing, Fade(glow_color, 0.3f));
  DrawTextEx(font, text, Vector2{x, y - 1}, size, spacing, Fade(glow_color, 0.3f));
  DrawTextEx(font, text, Vector2{x, y + 1}, size, spacing, Fade(glow_color, 0.3f));
  // Draw primary text
  DrawTextEx(font, text, Vector2{x, y}, size, spacing, primary_color);
  ```

---

## 4. Profiler & Monospaced Readability

Although Orbitron is a proportional font, it maintains very consistent character widths. For table alignments (e.g., in the profiler or resource breakdowns), we use standard monospaced formatting rules:
1. **Right-align all numbers** with constant padding.
2. **Fixed label widths** to ensure the column alignment never shifts as simulation values fluctuate.
3. **Monospaced fallbacks** (e.g. Raylib default font or `stb_truetype` monospaced packing) will only be used if layout jitter occurs, though standard column-based tabular formatting with `TextFormat` is fully sufficient.
