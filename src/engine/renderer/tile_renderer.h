#pragma once

#include "raylib.h"
#include "../../ecs/world.h"
#include "texture_cache.h"
#include "visual_palette.h"

namespace acheron::renderer
{
    enum class OverlayMode
    {
        None,
        Traffic,
        Economy,
        Instability,
        Progression,
        Zoning
    };

    class TileRenderer final
    {
    public:
        void render_districts(const ecs::World& world, const TextureCache& cache, float cell_px, OverlayMode mode, ecs::EntityID selected_district);
    };
}
