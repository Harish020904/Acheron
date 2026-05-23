#pragma once

#include "raylib.h"

namespace acheron::renderer
{
    // Acheron Cyberpunk Simulation Palette
    namespace palette
    {
        // Base Environment
        constexpr Color background      = Color{ 15, 20, 32, 255 };    // Deep Navy
        constexpr Color grid_lines      = Color{ 30, 45, 65, 120 };    // Muted tech blue

        // District States
        constexpr Color stable_blue     = Color{ 0, 140, 255, 255 };   // Neon Blue (Stable residential)
        constexpr Color economic_growth = Color{ 0, 230, 118, 255 };   // Bright Green (Commerce)
        constexpr Color industrial_zone = Color{ 142, 36, 170, 255 };  // Deep Violet (Industry)
        
        // Traffic & Congestion
        constexpr Color traffic_flow    = Color{ 0, 242, 254, 255 };   // Teal (Good flow)
        constexpr Color traffic_warn    = Color{ 255, 170, 0, 255 };   // Amber
        constexpr Color traffic_heavy   = Color{ 255, 23, 68, 255 };   // Intense Crimson (Gridlock)
        
        // Pressure & System Health
        constexpr Color upgrade_pressure = Color{ 255, 214, 0, 255 };  // Radiant Amber
        constexpr Color system_failure   = Color{ 255, 0, 85, 255 };   // Neon Red Pulse (Instability)

        // UI Colors
        constexpr Color ui_panel_bg     = Color{ 10, 15, 25, 210 };
        constexpr Color ui_border       = Color{ 0, 189, 255, 120 };
        constexpr Color ui_text_main    = Color{ 243, 243, 243, 255 }; // RAYWHITE
        constexpr Color ui_text_dim     = Color{ 180, 190, 200, 255 };
        constexpr Color ui_accent       = Color{ 0, 255, 102, 255 };   // Lime Green (Profiler)
    }
}
