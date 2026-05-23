#pragma once

#include "../../ecs/world.h"
#include "../../simulation/traffic/traffic_system.h"
#include "../../simulation/economy/economy_system.h"
#include "../../gameplay/progression/progression_system.h"
#include "texture_cache.h"
#include "raylib.h"

namespace acheron::renderer
{
    class UIRenderer final
    {
    public:
        void init();
        void render(
            const TextureCache& cache,
            int screen_width,
            int screen_height,
            const ecs::World& world,
            const simulation::traffic::TrafficSystem& traffic,
            const simulation::economy::EconomySystem& economy,
            const gameplay::progression::ProgressionSystem& progression,
            int overlay_mode,
            int sim_speed,
            ecs::EntityID selected_district,
            bool is_paused,
            bool show_profiler
        );
        
    private:
        float m_ft_history[100]{};
        int m_ft_index = 0;
    };
}
