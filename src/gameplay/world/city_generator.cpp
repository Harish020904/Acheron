#include "city_generator.h"
#include <cstdlib>
#include <cmath>

namespace acheron::gameplay::world
{
    void CityGenerator::generate_procedural_city(ecs::World& world, std::uint32_t width, std::uint32_t height)
    {
        world.clear();
        
        for (std::uint32_t x = 0; x < width; ++x)
        {
            for (std::uint32_t y = 0; y < height; ++y)
            {
                // Core layout rules
                // Center blocks are Commercial. Edges are Industrial. In-between is Residential.
                
                // Leave organic gaps
                if ((x * 13 + y * 7) % 5 == 0) continue; 
                
                // Keep away from absolute borders
                if (x < 1 || x >= width - 1 || y < 1 || y >= height - 1) continue;

                auto ent = world.spawn_district(x, y);
                auto* d = world.district(ent);
                if (!d) continue;

                float cx = static_cast<float>(width) / 2.0f;
                float cy = static_cast<float>(height) / 2.0f;
                float dist_to_center = std::sqrt(std::pow(x - cx, 2) + std::pow(y - cy, 2));

                if (dist_to_center < (width * 0.25f)) 
                {
                    d->type = ecs::DistrictType::Commercial;
                    d->wealth = 600.0f + (rand() % 400);
                    d->level = 3 + (rand() % 2);
                    d->population = 6000.0f;
                }
                else if (dist_to_center > (width * 0.38f)) 
                {
                    d->type = ecs::DistrictType::Industrial;
                    d->wealth = 100.0f + (rand() % 300);
                    d->level = 1 + (rand() % 2);
                    d->population = 1000.0f;
                }
                else 
                {
                    d->type = ecs::DistrictType::Residential;
                    d->wealth = 300.0f + (rand() % 300);
                    d->level = 2;
                    d->population = 3500.0f;
                }
            }
        }

        // Road connections
        for (std::uint32_t i = 0; i < world.district_count(); ++i)
        {
            auto* d1 = world.district_slot(i);
            if (!d1) continue;
            for (std::uint32_t j = i + 1; j < world.district_count(); ++j)
            {
                auto* d2 = world.district_slot(j);
                if (!d2) continue;
                
                int dx = std::abs((int)d1->grid_x - (int)d2->grid_x);
                int dy = std::abs((int)d1->grid_y - (int)d2->grid_y);
                
                if (dx + dy == 1) // Adjacent
                {
                    int chance = 60;
                    // Connect commercial cores heavily
                    if (d1->type == ecs::DistrictType::Commercial || d2->type == ecs::DistrictType::Commercial) chance = 95;
                    // Connect industrial sparsely
                    if (d1->type == ecs::DistrictType::Industrial && d2->type == ecs::DistrictType::Industrial) chance = 40;
                    
                    if (rand() % 100 < chance)
                    {
                        world.connect_districts(d1->entity, d2->entity, 1.0f);
                    }
                }
            }
        }
    }
}
