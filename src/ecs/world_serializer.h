#pragma once

#include "world.h"
#include <string>

namespace acheron::ecs
{
    class WorldSerializer final
    {
    public:
        static bool save_to_file(const World& world, const std::string& filepath);
        static bool load_from_file(World& world, const std::string& filepath);
    };
}
