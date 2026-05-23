#pragma once

#include "raylib.h"
#include "../../ecs/world.h"
#include "visual_palette.h"
#include "texture_cache.h"

namespace acheron::renderer
{
    class RoadRenderer final
    {
    public:
        void render_roads(const ecs::World& world, float cell_px, int overlay_mode);
        void render_traffic(const ecs::World& world, const TextureCache& cache, float cell_px);
    private:
        Color mix_color(Color a, Color b, float t) const;
    };
}
