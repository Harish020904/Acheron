# Phase 10: Polish & Showcase Preparation

## Visual Polish & Demo Scenarios
Project Acheron's ultimate goal is to act as a highly impressive university showcase.
By utilizing the **`M`** key, the user toggles **Demo Mode**. 
In Demo Mode:
- The camera smoothly pans across the procedurally generated city at a cinematic, diagonal angle.
- The active heatmap overlay (`Traffic`, `Economy`, `Instability`, `Progression`, `Zoning`) automatically rotates every 10 seconds.
- It provides a completely hands-free method to showcase the project's technical depth without relying on manual player input.

## Audio Atmosphere
An `AudioManager` system using miniaudio/Raylib manages the soundscape. 
- A subtle low-frequency background hum anchors the simulation.
- High-tech blip/alert UI sounds play immediately upon interacting with overlays, regenerating the city, or saving/loading states.
- Audio is intentionally kept minimal to ensure the presenter's voice is not drowned out during a showcase.

## Debug Tools
Presenters have access to full runtime toggles:
- **`SPACE`**: Instantly pause simulation while retaining visual rendering/camera control.
- **`TAB`**: Hide/Show the Profiler overlay for clean video capture.
- **`1-4`**: Instantly snap to specific ECS metrics overlays to demonstrate data pipelines.

## Video Friendly Camera
The `SimpleRenderer` supports smooth zoom targeting the mouse cursor location, allowing the presenter to fluidly dive deep into an active intersection or zoom out to view the entire macroeconomic grid. Middle-mouse dragging allows for precise frame composition.
