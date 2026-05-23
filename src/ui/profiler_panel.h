#pragma once

#include "raylib.h"
#include "../engine/renderer/texture_cache.h"
#include "../engine/renderer/visual_palette.h"

namespace acheron::ui
{
    class ProfilerPanel final
    {
    public:
        void render(const renderer::TextureCache& cache, int screen_width, int screen_height);
    private:
        float m_history[100]{};
        int m_index = 0;
    };
}
