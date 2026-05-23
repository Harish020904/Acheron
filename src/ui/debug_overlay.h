#pragma once

#include "raylib.h"
#include "../engine/renderer/texture_cache.h"
#include "../engine/renderer/visual_palette.h"

namespace acheron::ui
{
    class DebugOverlay final
    {
    public:
        void render(const renderer::TextureCache& cache, int fps, int entities);
    };
}
