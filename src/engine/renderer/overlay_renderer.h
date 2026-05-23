#pragma once

#include "../../ecs/world.h"
#include "texture_cache.h"
#include "raylib.h"
#include "tile_renderer.h"

namespace acheron::renderer
{
    class OverlayRenderer final
    {
    public:
        void render(const ecs::World& world, float cell_px, OverlayMode mode);
    };
}
