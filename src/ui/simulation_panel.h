#pragma once

#include "raylib.h"
#include "../engine/renderer/texture_cache.h"
#include "../engine/renderer/visual_palette.h"
#include "../simulation/traffic/traffic_system.h"
#include "../simulation/economy/economy_system.h"
#include "../gameplay/progression/progression_system.h"
#include "../ecs/world.h"

namespace acheron::ui
{
    class SimulationPanel final
    {
    public:
        void render(const renderer::TextureCache& cache, int screen_width, const ecs::World& world, const simulation::traffic::TrafficSystem& traffic, const simulation::economy::EconomySystem& economy, const gameplay::progression::ProgressionSystem& progression, int sim_speed);
    };
}
