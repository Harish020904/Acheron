#include "debug_overlay.h"

namespace acheron::ui
{
    void DebugOverlay::render(const renderer::TextureCache& cache, int fps, int entities)
    {
        Font font = cache.get_font("orbitron_regular");
        
        Rectangle panel = { 10, 10, 200, 60 };
        DrawRectangleRec(panel, renderer::palette::ui_panel_bg);
        DrawRectangleLinesEx(panel, 1.0f, renderer::palette::ui_border);
        
        DrawTextEx(font, TextFormat("FPS: %d", fps), Vector2{20, 15}, 16, 1.0f, renderer::palette::ui_text_main);
        DrawTextEx(font, TextFormat("ENTITIES: %d", entities), Vector2{20, 35}, 16, 1.0f, renderer::palette::ui_text_dim);
    }
}
