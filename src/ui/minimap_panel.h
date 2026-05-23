#pragma once

#include "raylib.h"
#include "../ecs/world.h"
#include "../engine/renderer/visual_palette.h"

namespace acheron::ui
{
    class MinimapPanel final
    {
    public:
        void render(int screen_width, const ecs::World& world);
    };
}
