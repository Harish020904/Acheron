#include "simulation_panel.h"

namespace acheron::ui
{
    void SimulationPanel::render(const renderer::TextureCache& cache, int screen_width, const ecs::World& world, const simulation::traffic::TrafficSystem& traffic, const simulation::economy::EconomySystem& economy, const gameplay::progression::ProgressionSystem& progression, int sim_speed)
    {
        (void)progression;
        Font font_bold = cache.get_font("orbitron_bold");
        Font font_reg = cache.get_font("orbitron_regular");
        
        int panel_w = 280;
        int panel_h = 240;
        Rectangle panel = { static_cast<float>(screen_width - panel_w - 20), 20.0f, static_cast<float>(panel_w), static_cast<float>(panel_h) };
        
        // Background and border
        DrawRectangleRec(panel, renderer::palette::ui_panel_bg);
        DrawRectangleLinesEx(panel, 1.0f, renderer::palette::ui_border);
        
        // Title
        DrawRectangle(panel.x, panel.y, panel_w, 30, Fade(renderer::palette::ui_border, 0.3f));
        DrawTextEx(font_bold, "SYSTEM TELEMETRY", Vector2{panel.x + 10, panel.y + 6}, 18, 1.0f, renderer::palette::ui_text_main);
        
        // Stats
        float y = panel.y + 40.0f;
        float x = panel.x + 15.0f;
        float spacing = 22.0f;
        
        DrawTextEx(font_reg, TextFormat("DISTRICTS: %d", world.district_count()), Vector2{x, y}, 16, 1.0f, renderer::palette::stable_blue); y += spacing;
        DrawTextEx(font_reg, TextFormat("ROADS: %d", world.road_count()), Vector2{x, y}, 16, 1.0f, renderer::palette::ui_text_dim); y += spacing;
        
        float cong = traffic.get_congestion_metric();
        Color traffic_col = renderer::palette::traffic_flow;
        if (cong > 0.4f) traffic_col = renderer::palette::traffic_warn;
        if (cong > 0.7f) traffic_col = renderer::palette::traffic_heavy;
        
        DrawTextEx(font_reg, TextFormat("TRAFFIC: %.1f%%", cong * 100.0f), Vector2{x, y}, 16, 1.0f, traffic_col); y += spacing;
        DrawTextEx(font_reg, TextFormat("ECONOMY: %.0f", economy.get_total_wealth()), Vector2{x, y}, 16, 1.0f, renderer::palette::economic_growth); y += spacing;
        
        DrawTextEx(font_reg, TextFormat("SIM SPEED: %dx", sim_speed), Vector2{x, y}, 16, 1.0f, renderer::palette::ui_accent); y += spacing;
        
        // Controls hint
        y += 10.0f;
        DrawTextEx(font_reg, "CONTROLS:", Vector2{x, y}, 14, 1.0f, renderer::palette::ui_text_dim); y += 18.0f;
        DrawTextEx(font_reg, "[WASD] Pan  [Scroll] Zoom", Vector2{x, y}, 14, 1.0f, renderer::palette::ui_text_dim); y += 16.0f;
        DrawTextEx(font_reg, "[SPACE] Pause  [1/2/3] Speed", Vector2{x, y}, 14, 1.0f, renderer::palette::ui_text_dim); y += 16.0f;
        DrawTextEx(font_reg, "[D] Expand  [T/E/C] Events", Vector2{x, y}, 14, 1.0f, renderer::palette::ui_text_dim);
    }
}
