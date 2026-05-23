# Project Acheron — AI Workflow Strategy & Token Optimization

This strategy outlines the resource-efficient distribution of coding and integration tasks for Project Acheron. Its primary purpose is to **minimize token burn**, **avoid long regression loops**, and **optimize future development phases** using targeted AI models.

---

## 1. Model Matrix & Task Assignment

Different models have varying strengths. We split tasks strategically to maximize output quality while staying well within periodic refresh quotas:

| Model Tier / Class | Strengths | Recommended Tasks in Acheron | What to Avoid |
| :--- | :--- | :--- | :--- |
| **Claude (3.5 Sonnet / Opus / 4.6)** | High-reasoning, deep architecture integration, state sync, ECS queries | * Structural integration<br>* Multi-threading coordination<br>* Game loop routing<br>* Complex sub-renderer structure | * Slicing large image folders<br>* Writing boilerplate structs<br>* Heavy script generation |
| **Gemini (3.5 Flash)** | Ultra-fast execution, vast context window, file system/asset operations | * Asset organization and extraction<br>* Generating SVG vectors<br>* Multi-file refactoring scripts<br>* Initial workspace preparation | * Heavy compiler error resolution<br>* Deep architectural refactoring |
| **Lightweight / Local OSS (GPT-OSS / DeepSeek-Coder)** | Fast single-file utilities, helper methods, simple math calculations | * Vector calculations<br>* Simple particles update loops<br>* Sound integration boilerplate<br>* Color mixer utilities | * Complex multi-file systems integration |

---

## 2. Token Optimization Guidelines

### Principle 1: Prevent In-Place Monolithic Rewrites
* **Do NOT** ask the AI to rewrite whole directories at once.
* Instead of: *"Implement the entire rendering stack and rewrite main.cpp now."*
* Request: *"Create the single header `tile_renderer.h` specifying the interface for drawing 16x16 tiles."*

### Principle 2: Maintain a Modular File System
Keep renderer files small and highly specialized:
* `tile_renderer.cpp` does ONLY tiles.
* `ui_renderer.cpp` does ONLY dashboard text and panel outlines.
* `vehicle_renderer.cpp` does ONLY vehicle array interpolation.
This ensures edits can use targeted file replacement tools rather than expensive whole-file rewrites.

### Principle 3: Avoid Large Binary-to-Text Conversions
* Do not feed raw image data, base64 strings, or giant atlas text maps into the AI prompts.
* Let local C++ routines load textures programmatically. The AI only needs to write the loader loop.

---

## 3. Recommended Phased Prompts

To proceed safely without consuming excessive quota, future prompts should be structured as follows:

```
[Target: Phase 1 — Asset Extraction Script]
"Using Python or PowerShell, write a small script that extracts the static assets from ZIPs in C:\Users\Harish\Downloads\ and copies ONLY the approved PNGs and TTF files into their matching destination directories under Acheron/assets/..."
```

```
[Target: Phase 2 — AssetManager Base]
"Write a simple, high-performance C++ class `AssetManager` that loads textures and fonts on startup using Raylib's LoadTexture/LoadFontEx, and returns them by string hash lookups..."
```

```
[Target: Phase 3 — Component Renderers]
"Create `src/engine/renderer/tile_renderer.h` and `.cpp` to draw the ECS grid districts as tiles. Accept the world and district pointers, and select tiles based on wealth levels..."
```
This incremental approach ensures each segment is compiled, tested, and validated before moving forward, eliminating large-scale codebase regression.
