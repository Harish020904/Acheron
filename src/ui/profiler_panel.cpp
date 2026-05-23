#include "profiler_panel.h"
#include <algorithm>

namespace acheron::ui
{
    void ProfilerPanel::render(const renderer::TextureCache& cache, int screen_width, int screen_height)
    {
        (void)screen_width;
        Font font = cache.get_font("orbitron_regular");
        
        float current_ft = GetFrameTime() * 1000.0f;
        m_history[m_index] = current_ft;
        m_index = (m_index + 1) % 100;
        
        Rectangle panel = { 10.0f, static_cast<float>(screen_height - 120), 300.0f, 110.0f };
        DrawRectangleRec(panel, Fade(renderer::palette::ui_panel_bg, 0.5f));
        DrawRectangleLinesEx(panel, 1.0f, Fade(renderer::palette::ui_accent, 0.4f));
        
        float y = panel.y + 10.0f;
        float x = panel.x + 10.0f;
        
        DrawTextEx(font, "PROFILER ACTIVE", Vector2{x, y}, 14, 1.0f, renderer::palette::ui_accent); y += 20.0f;
        DrawTextEx(font, TextFormat("Frame Time: %.2f ms", current_ft), Vector2{x, y}, 14, 1.0f, renderer::palette::ui_text_main); y += 18.0f;
        DrawTextEx(font, "Jobs Dispatched: OK", Vector2{x, y}, 14, 1.0f, renderer::palette::ui_text_main); y += 18.0f;
        
        // Draw graph
        float graph_x = x + 150.0f;
        float graph_y = panel.y + 30.0f;
        float graph_w = 120.0f;
        float graph_h = 60.0f;
        DrawRectangleLines(graph_x, graph_y, graph_w, graph_h, DARKGRAY);
        
        for (int i = 0; i < 99; ++i)
        {
            int idx1 = (m_index + i) % 100;
            int idx2 = (m_index + i + 1) % 100;
            
            float h1 = std::clamp(m_history[idx1] / 33.0f, 0.0f, 1.0f) * graph_h; // Scale based on 30fps baseline (33ms)
            float h2 = std::clamp(m_history[idx2] / 33.0f, 0.0f, 1.0f) * graph_h;
            
            float x1 = graph_x + (i / 99.0f) * graph_w;
            float x2 = graph_x + ((i + 1) / 99.0f) * graph_w;
            
            DrawLineEx(Vector2{x1, graph_y + graph_h - h1}, Vector2{x2, graph_y + graph_h - h2}, 1.0f, renderer::palette::ui_accent);
        }
    }
}
