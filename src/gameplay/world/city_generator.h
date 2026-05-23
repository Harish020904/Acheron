#pragma once

#include "../../ecs/world.h"

namespace acheron::gameplay::world
{
    class CityGenerator final
    {
    public:
        static void generate_procedural_city(ecs::World& world, std::uint32_t width, std::uint32_t height);
    };
}
