// =============================================================
// Project Acheron — Melbourne Urban Simulation
// A deterministic C++20 city simulation of Melbourne, Australia
// =============================================================
#include "core/logger.h"
#include "ecs/world.h"
#include "ecs/world_serializer.h"
#include "engine/audio/audio_manager.h"
#include "engine/renderer/melbourne_renderer.h"
#include "engine/renderer/overlay_renderer.h"
#include "engine/renderer/simple_renderer.h"
#include "engine/renderer/texture_cache.h"
#include "engine/renderer/ui_renderer.h"
#include "gameplay/event_system.h"
#include "gameplay/interaction_system.h"
#include "gameplay/progression/progression_system.h"
#include "simulation/citizens/citizen_system.h"
#include "simulation/economy/economy_system.h"
#include "simulation/scheduler/job_system.h"
#include "simulation/traffic/traffic_system.h"
#include "utilities/profiler.h"
#include "world/melbourne_map.h"

using namespace acheron;

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>

namespace
{
    // ========================================================
    // Simulation constants
    // ========================================================
    constexpr float    FIXED_DT      = 1.0f / 60.0f;
    constexpr int      MAX_SIM_STEPS = 8;
    constexpr uint32_t MAX_CITIZENS  = 1500;

    // Grid fallback for ECS districts (we map Melbourne suburbs to grid)
    constexpr uint32_t GRID_W        = 16;
    constexpr uint32_t GRID_H        = 16;
    constexpr uint32_t MAX_DISTRICTS = GRID_W * GRID_H;
    constexpr uint32_t MAX_ROADS     = 512;

    float clamp01(float v) noexcept { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); }

    // ========================================================
    // Melbourne CBD Exploration Zone (world-space bounds)
    // Based on GPS: lat -37.816 to -37.808, lon 144.950 to 144.978
    // (Flinders St → Federation Sq → Swanston St → Elizabeth St)
    // ========================================================
    constexpr float CBD_X_MIN = 2400.0f;
    constexpr float CBD_X_MAX = 6800.0f;
    constexpr float CBD_Y_MIN = 3200.0f;
    constexpr float CBD_Y_MAX = 5000.0f;
    // Player starts at Flinders Street Station (world coords)
    constexpr float PLAYER_START_X = 4500.0f;
    constexpr float PLAYER_START_Y = 4400.0f;

    // ========================================================
    // Player Character
    // ========================================================
    struct PlayerCharacter
    {
        float x           = PLAYER_START_X;
        float y           = PLAYER_START_Y;
        float vx          = 0.0f;
        float vy          = 0.0f;
        float walk_anim   = 0.0f;  // Oscillating leg animation timer
        float facing      = 1.0f;  // +1 = right, -1 = left
        float speed       = 200.0f; // World units per second (street level)
        bool  moving      = false;
        // Nearby POI index (-1 = none)
        int   near_poi    = -1;
    };

    // Draw a top-down "boy" character at world coordinates
    // The character is a small figure with head, body, arms, and animated legs
    void draw_player(const PlayerCharacter& p, float zoom, float time)
    {
        float cx = p.x;
        float cy = p.y;

        // Scale sizes so they feel right at street-level zoom (~3.5x)
        // These are world-space sizes
        constexpr float HEAD_R   = 7.0f;
        constexpr float BODY_LEN = 14.0f;
        constexpr float LEG_LEN  = 11.0f;
        constexpr float ARM_LEN  = 9.0f;

        // Only visible at zoom > 0.8 (street level)
        if (zoom < 0.8f) {
            // At low zoom, just draw a bright dot with glow
            float pulse = 0.6f + 0.4f * std::sin(time * 4.0f);
            DrawCircleV(Vector2{cx, cy}, 12.0f, Fade(Color{0, 255, 180, 255}, pulse));
            DrawCircleV(Vector2{cx, cy}, 5.0f, WHITE);
            return;
        }

        // ---- Outer glow ----
        float pulse = 0.5f + 0.5f * std::sin(time * 3.5f);
        DrawCircleV(Vector2{cx, cy}, HEAD_R * 2.8f, Fade(Color{0, 240, 180, 255}, pulse * 0.25f));
        DrawCircleV(Vector2{cx, cy}, HEAD_R * 2.0f, Fade(Color{0, 200, 255, 255}, pulse * 0.15f));

        // ---- Shadow ----
        DrawEllipse(static_cast<int>(cx), static_cast<int>(cy + BODY_LEN + LEG_LEN),
                    static_cast<int>(HEAD_R * 1.2f), static_cast<int>(3),
                    Fade(BLACK, 0.35f));

        // ---- Walking animation ----
        float leg_swing = p.moving ? std::sin(p.walk_anim * 8.0f) * 0.4f : 0.0f;
        float arm_swing = p.moving ? std::sin(p.walk_anim * 8.0f + PI) * 0.3f : 0.0f;

        // ---- Legs ----
        Vector2 hips    { cx, cy + BODY_LEN };
        // Left leg
        Vector2 l_foot  { cx - LEG_LEN * 0.4f + leg_swing * LEG_LEN,
                           cy + BODY_LEN + LEG_LEN };
        // Right leg
        Vector2 r_foot  { cx + LEG_LEN * 0.4f - leg_swing * LEG_LEN,
                           cy + BODY_LEN + LEG_LEN };
        DrawLineEx(hips, l_foot, 2.5f, Color{0, 200, 255, 220});
        DrawLineEx(hips, r_foot, 2.5f, Color{0, 200, 255, 220});

        // ---- Body ----
        Vector2 body_top { cx, cy };
        DrawLineEx(body_top, hips, 3.0f, Color{0, 220, 200, 240});

        // ---- Arms ----
        Vector2 shoulder_l { cx - HEAD_R * 0.6f, cy + HEAD_R * 0.5f };
        Vector2 shoulder_r { cx + HEAD_R * 0.6f, cy + HEAD_R * 0.5f };
        Vector2 hand_l { cx - ARM_LEN + arm_swing * ARM_LEN * 0.5f,
                          cy + HEAD_R + ARM_LEN * 0.7f };
        Vector2 hand_r { cx + ARM_LEN - arm_swing * ARM_LEN * 0.5f,
                          cy + HEAD_R + ARM_LEN * 0.7f };
        DrawLineEx(shoulder_l, hand_l, 2.0f, Color{0, 200, 255, 200});
        DrawLineEx(shoulder_r, hand_r, 2.0f, Color{0, 200, 255, 200});

        // ---- Head ----
        DrawCircleV(Vector2{cx, cy}, HEAD_R, Color{30, 200, 255, 255});
        DrawCircleLinesV(Vector2{cx, cy}, HEAD_R, Color{180, 240, 255, 255});

        // ---- Eyes (tiny dots) ----
        float eye_offset = p.facing * 3.0f;
        DrawCircleV(Vector2{cx + eye_offset - 1.5f, cy - 1.5f}, 1.5f, Color{10, 20, 40, 255});
        DrawCircleV(Vector2{cx + eye_offset + 1.5f, cy - 1.5f}, 1.5f, Color{10, 20, 40, 255});

        // ---- "YOU" label above head ----
        float label_y = cy - HEAD_R - 16.0f;
        DrawText("YOU", static_cast<int>(cx - 12), static_cast<int>(label_y), 10, Color{0, 255, 180, 230});

        // ---- Nearby POI indicator ----
        if (p.near_poi >= 0) {
            DrawText("[F] Interact", static_cast<int>(cx - 30), static_cast<int>(cy - HEAD_R - 30),
                     9, Color{255, 220, 0, 200});
        }
    }

    // ========================================================
    // Melbourne suburb seed data (real approximations)
    // We map each suburb to a 2D grid cell for ECS purposes
    // ========================================================
    struct SuburbSeed
    {
        const char* name;
        float lat;
        float lon;
        ecs::DistrictType type;
    };

    constexpr SuburbSeed MELBOURNE_SUBURBS[] = {
        // CBD Core
        { "Melbourne CBD",       -37.8136f, 144.9631f, ecs::DistrictType::Commercial },
        { "Docklands",           -37.8145f, 144.9455f, ecs::DistrictType::Commercial },
        { "Southbank",           -37.8219f, 144.9618f, ecs::DistrictType::Commercial },
        { "East Melbourne",      -37.8110f, 144.9830f, ecs::DistrictType::Commercial },
        // Inner North
        { "Carlton",             -37.7996f, 144.9657f, ecs::DistrictType::Residential },
        { "Fitzroy",             -37.7996f, 144.9774f, ecs::DistrictType::Residential },
        { "Collingwood",         -37.8040f, 144.9852f, ecs::DistrictType::Industrial  },
        { "Parkville",           -37.7907f, 144.9527f, ecs::DistrictType::Residential },
        { "North Melbourne",     -37.7998f, 144.9455f, ecs::DistrictType::Residential },
        // Inner South
        { "South Melbourne",     -37.8310f, 144.9561f, ecs::DistrictType::Residential },
        { "St Kilda",            -37.8676f, 144.9799f, ecs::DistrictType::Residential },
        { "Port Melbourne",      -37.8349f, 144.9278f, ecs::DistrictType::Industrial  },
        { "South Yarra",         -37.8402f, 144.9912f, ecs::DistrictType::Commercial  },
        // Inner West
        { "Footscray",           -37.7999f, 144.9001f, ecs::DistrictType::Industrial  },
        { "Flemington",          -37.7862f, 144.9220f, ecs::DistrictType::Residential },
        // Inner East
        { "Richmond",            -37.8230f, 145.0017f, ecs::DistrictType::Residential },
        { "Hawthorn",            -37.8222f, 145.0284f, ecs::DistrictType::Residential },
        { "Abbotsford",          -37.8050f, 145.0024f, ecs::DistrictType::Industrial  },
        { "Prahran",             -37.8490f, 144.9930f, ecs::DistrictType::Residential },
        // Universities
        { "Uni of Melbourne",    -37.7963f, 144.9614f, ecs::DistrictType::Commercial  },
        { "RMIT",                -37.8080f, 144.9630f, ecs::DistrictType::Commercial  },
        // Parks
        { "Royal Park",          -37.7850f, 144.9490f, ecs::DistrictType::Residential },
        { "Flagstaff Gardens",   -37.8106f, 144.9534f, ecs::DistrictType::Residential },
    };

    constexpr uint32_t SUBURB_COUNT = static_cast<uint32_t>(sizeof(MELBOURNE_SUBURBS) / sizeof(MELBOURNE_SUBURBS[0]));

    // ========================================================
    // Populate ECS world from Melbourne suburb seed data
    // ========================================================
    void populate_world_from_melbourne(ecs::World& world)
    {
        world.clear();

        for (uint32_t i = 0; i < SUBURB_COUNT; ++i)
        {
            const SuburbSeed& s = MELBOURNE_SUBURBS[i];
            float wx, wy;
            world::MelbourneMap::gps_to_world(s.lat, s.lon, wx, wy);

            // Map world coord to grid cell
            uint32_t gx = static_cast<uint32_t>(wx / world::melbourne_data::MAP_WORLD_WIDTH  * GRID_W);
            uint32_t gy = static_cast<uint32_t>(wy / world::melbourne_data::MAP_WORLD_HEIGHT * GRID_H);
            gx = std::min(gx, GRID_W - 1);
            gy = std::min(gy, GRID_H - 1);

            auto eid = world.spawn_district(gx, gy);
            if (!ecs::is_valid(eid)) continue;

            auto* d = world.district(eid);
            if (!d) continue;
            d->type = s.type;
            d->level = (s.type == ecs::DistrictType::Commercial) ? 3 : 2;
            d->population = (s.type == ecs::DistrictType::Commercial) ? 12000.0f : 5000.0f;
            d->wealth     = (s.type == ecs::DistrictType::Commercial) ? 800.0f  : 350.0f;
            d->traffic    = 0.05f;
            d->stability  = 0.85f;
            d->instability = 0.10f;
        }
    }

    // ========================================================
    // Spawn initial citizen population from Melbourne POI data
    // ========================================================
    void populate_citizens(simulation::citizens::CitizenSystem& citizens,
                           const world::MelbourneMap& melb_map)
    {
        // Use seeded random for determinism
        std::srand(42);

        const uint32_t n = MAX_CITIZENS;
        for (uint32_t i = 0; i < n; ++i)
        {
            // Pick a random home POI and a random work POI
            uint32_t home_idx = std::rand() % melb_map.poi_count;
            uint32_t work_idx = std::rand() % melb_map.poi_count;

            const world::PointOfInterest& home = melb_map.pois[home_idx];
            const world::PointOfInterest& work = melb_map.pois[work_idx];

            // Add random scatter so citizens don't stack exactly on POI centers
            float scatter = 60.0f;
            float hx = home.x + ((std::rand() % 200) - 100) * scatter / 100.0f;
            float hy = home.y + ((std::rand() % 200) - 100) * scatter / 100.0f;
            float wx = work.x + ((std::rand() % 200) - 100) * scatter / 100.0f;
            float wy = work.y + ((std::rand() % 200) - 100) * scatter / 100.0f;

            // Determine occupation from work POI category
            simulation::citizens::CitizenOccupation occ;
            switch (static_cast<world::POICategory>(work.category))
            {
                case world::POICategory::Hospital:   occ = simulation::citizens::CitizenOccupation::Medic;    break;
                case world::POICategory::School:
                case world::POICategory::University: occ = simulation::citizens::CitizenOccupation::Student;  break;
                case world::POICategory::Restaurant:
                case world::POICategory::Gym:        occ = simulation::citizens::CitizenOccupation::Service;  break;
                default:                             occ = simulation::citizens::CitizenOccupation::Worker;   break;
            }

            citizens.spawn_citizen(hx, hy, wx, wy, occ);
        }
    }
}

int main()
{
    using namespace acheron;

    // ========================================================
    // Logger
    // ========================================================
    core::LoggerConfig logger_config{};
    logger_config.enable_console = true;
    logger_config.enable_file    = false;
    core::Logger::init(logger_config);
    ACH_LOG_INFO("Acheron Melbourne Simulation booting...");

    // ========================================================
    // Melbourne Map — load real GPS road data
    // ========================================================
    world::MelbourneMap melb_map;
    if (!melb_map.initialize())
    {
        ACH_LOG_ERROR("Failed to initialize Melbourne map");
        return 1;
    }
    ACH_LOG_INFO("Melbourne map loaded: %u edges, %u POIs", melb_map.edge_count, melb_map.poi_count);

    // ========================================================
    // ECS World — suburbs as districts
    // ========================================================
    ecs::World world;
    ecs::World::Config world_config{};
    world_config.grid_width   = GRID_W;
    world_config.grid_height  = GRID_H;
    world_config.max_districts = MAX_DISTRICTS;
    world_config.max_roads    = MAX_ROADS;
    if (!world.initialize(world_config))
    {
        ACH_LOG_ERROR("Failed to initialize ECS world");
        return 1;
    }

    populate_world_from_melbourne(world);
    ACH_LOG_INFO("Melbourne suburbs populated: %u districts, %u roads", world.district_count(), world.road_count());

    // ========================================================
    // Renderer & Window
    // ========================================================
    renderer::SimpleRenderer renderer;
    renderer::SimpleRenderer::Config renderer_config{};
    renderer_config.width      = 1920;
    renderer_config.height     = 1080;
    renderer_config.title      = "Acheron \xe2\x80\x94 Melbourne Urban Simulation";
    renderer_config.target_fps = 60;
    if (!renderer.init(renderer_config))
    {
        ACH_LOG_ERROR("Failed to initialize renderer");
        return 1;
    }

    renderer::TextureCache texture_cache;
    texture_cache.load_all();

    renderer::MelbourneRenderer melb_renderer;
    renderer::CitizenRenderer   citizen_renderer;
    renderer::UIRenderer        ui_renderer;
    renderer::OverlayRenderer   overlay_renderer;
    ui_renderer.init();

    // ========================================================
    // Simulation Systems
    // ========================================================
    simulation::scheduler::JobSystem job_system;
    simulation::scheduler::JobSystemConfig job_config{};
    job_config.worker_count          = 4;
    job_config.pin_workers           = false;
    job_config.job_capacity          = 256;
    job_config.dependency_capacity   = 256;
    job_config.worker_queue_capacity = 64;
    if (!job_system.init(job_config))
    {
        ACH_LOG_ERROR("Failed to initialize job system");
        return 1;
    }

    utilities::FrameProfiler profiler;
    if (!profiler.initialize(60.0f))
    {
        ACH_LOG_ERROR("Failed to initialize profiler");
        return 1;
    }

    simulation::traffic::TrafficSystem traffic_system;
    simulation::traffic::RoadNetworkConfig traffic_config{};
    traffic_config.grid_width         = GRID_W;
    traffic_config.grid_height        = GRID_H;
    traffic_config.max_lanes          = MAX_DISTRICTS;
    traffic_config.cell_size          = 1.0f;
    traffic_config.max_density        = 1.0f;
    traffic_config.congestion_wave_speed = 0.5f;
    if (!traffic_system.initialize(traffic_config))
    {
        ACH_LOG_ERROR("Failed to initialize traffic system");
        return 1;
    }

    simulation::economy::EconomySystem economy_system;
    simulation::economy::EconomyConfig economy_config{};
    economy_config.max_districts       = MAX_DISTRICTS;
    economy_config.max_trade_routes    = MAX_ROADS;
    economy_config.base_production_rate = 12.0f;
    economy_config.trade_efficiency    = 0.9f;
    economy_config.resource_decay_rate = 0.02f;
    if (!economy_system.initialize(economy_config))
    {
        ACH_LOG_ERROR("Failed to initialize economy system");
        return 1;
    }

    gameplay::progression::ProgressionSystem progression_system;
    if (!progression_system.initialize())
    {
        ACH_LOG_ERROR("Failed to initialize progression system");
        return 1;
    }

    // Melbourne citizen agents
    simulation::citizens::CitizenSystem citizen_system;
    simulation::citizens::CitizenSystemConfig citizen_config{};
    citizen_config.max_citizens     = MAX_CITIZENS;
    citizen_config.sim_day_seconds  = 90.0f;  // 90 real seconds = 1 Melbourne sim-day
    citizen_config.base_speed       = 120.0f; // World pixels per second
    if (!citizen_system.initialize(citizen_config))
    {
        ACH_LOG_ERROR("Failed to initialize citizen system");
        return 1;
    }

    populate_citizens(citizen_system, melb_map);
    ACH_LOG_INFO("Citizens spawned: %u", citizen_system.citizen_count());

    gameplay::InteractionSystem interaction_system;
    gameplay::EventSystem       event_system;
    engine::audio::AudioManager audio_manager;
    audio_manager.init();

    // ========================================================
    // Initial Progression State (Melbourne scale)
    // ========================================================
    auto& prog_state = progression_system.get_mutable_state();
    prog_state.total_population    = 500000;
    prog_state.total_income        = 120000;
    prog_state.infrastructure_level = 2;

    // ========================================================
    // Main Loop State
    // ========================================================
    float    accumulator     = 0.0f;
    uint32_t speed           = 1;
    bool     is_paused       = false;
    bool     show_profiler   = true;
    bool     demo_mode       = false;
    float    demo_timer      = 0.0f;
    int      overlay_mode    = 0;   // 0=None, 1=Traffic, 2=Economy, 3=Population, 4=Stability, 5=Pathfinding
    std::string save_path    = "save_state.bin";

    // Locate citizen state
    uint32_t selected_citizen_uid = 0;

    // ========================================================
    // Player Character & Exploration Mode
    // ========================================================
    PlayerCharacter player;
    bool exploration_mode  = false;  // G key toggles
    float explore_cam_zoom = 3.5f;   // Street-level zoom target
    float prev_overview_x  = world::melbourne_data::MAP_WORLD_WIDTH  * 0.5f;
    float prev_overview_y  = world::melbourne_data::MAP_WORLD_HEIGHT * 0.5f;
    float prev_overview_z  = 0.12f;

    // ========================================================
    // Game State: Splash Screen
    // ========================================================
    enum class GameState { Splash, Playing };
    GameState game_state  = GameState::Splash;
    float     splash_anim = 0.0f; // time accumulator for splash animations

    ACH_LOG_INFO("Acheron Melbourne Simulation running.");


    // ========================================================
    // MAIN LOOP
    // ========================================================
    while (!renderer.should_close())
    {
        const float frame_dt = std::min(GetFrameTime(), 0.25f);
        splash_anim += frame_dt;

        // ============================================================
        // SPLASH SCREEN
        // ============================================================
        if (game_state == GameState::Splash)
        {
            if (renderer.begin_frame())
            {
                ClearBackground(renderer::palette::background);

                // Draw the mini preview map (already centered)
                renderer.begin_world();
                melb_renderer.render_roads(melb_map, renderer.get_camera(),
                                            GetScreenWidth(), GetScreenHeight(), 0);
                renderer.end_world();

                // Dim overlay
                DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(),
                              Fade(Color{8, 12, 22, 255}, 0.72f));

                // --- Title Panel ---
                auto font = texture_cache.get_font("orbitron_regular");
                int sw = GetScreenWidth(), sh = GetScreenHeight();

                // Outer glow rect
                float pulse = 0.7f + 0.3f * std::sin(splash_anim * 2.5f);
                Rectangle outer = { (float)sw*0.5f - 340.0f, (float)sh*0.5f - 200.0f, 680.0f, 400.0f };
                DrawRectangleLinesEx(outer, 2.0f, Fade(renderer::palette::ui_border, pulse));
                DrawRectangleRec(outer, Fade(renderer::palette::ui_panel_bg, 0.92f));

                // Logo text: ACHERON
                const char* title = "ACHERON";
                float title_size  = 72.0f;
                Vector2 title_dim = MeasureTextEx(font, title, title_size, 2);
                DrawTextEx(font, title,
                           Vector2{ (float)sw*0.5f - title_dim.x*0.5f, (float)sh*0.5f - 160.0f },
                           title_size, 2, renderer::palette::ui_accent);

                // Subtitle
                const char* sub = "Melbourne Urban Simulation";
                float sub_size = 18.0f;
                Vector2 sub_dim = MeasureTextEx(font, sub, sub_size, 1);
                DrawTextEx(font, sub,
                           Vector2{ (float)sw*0.5f - sub_dim.x*0.5f, (float)sh*0.5f - 72.0f },
                           sub_size, 1, renderer::palette::ui_text_dim);

                // Stats line
                const char* stats = TextFormat("%u road nodes  |  %u agents  |  23 suburbs",
                                               world::melbourne_data::NODE_COUNT,
                                               citizen_system.citizen_count());
                float stats_size = 13.0f;
                Vector2 stats_dim = MeasureTextEx(font, stats, stats_size, 1);
                DrawTextEx(font, stats,
                           Vector2{ (float)sw*0.5f - stats_dim.x*0.5f, (float)sh*0.5f - 30.0f },
                           stats_size, 1, Fade(renderer::palette::ui_text_dim, 0.7f));

                // ENTER button
                Rectangle btn = { (float)sw*0.5f - 100.0f, (float)sh*0.5f + 60.0f, 200.0f, 52.0f };
                bool hovered  = CheckCollisionPointRec(GetMousePosition(), btn);
                float btn_pulse = hovered ? 1.0f : (0.6f + 0.4f * std::sin(splash_anim * 3.0f));
                DrawRectangleRec(btn, Fade(renderer::palette::ui_accent, hovered ? 0.25f : 0.12f));
                DrawRectangleLinesEx(btn, 2.0f, Fade(renderer::palette::ui_accent, btn_pulse));
                const char* btn_text = "ENTER SIMULATION";
                float btn_font_size = 14.0f;
                Vector2 btn_dim = MeasureTextEx(font, btn_text, btn_font_size, 1);
                DrawTextEx(font, btn_text,
                           Vector2{ btn.x + btn.width*0.5f - btn_dim.x*0.5f,
                                    btn.y + btn.height*0.5f - btn_dim.y*0.5f },
                           btn_font_size, 1,
                           hovered ? renderer::palette::ui_accent : renderer::palette::ui_text_main);

                // Version
                DrawTextEx(font, "v0.7.0  |  OpenStreetMap data (c) contributors, ODbL",
                           Vector2{ (float)sw*0.5f - 170.0f, (float)sh*0.5f + 150.0f },
                           10.0f, 0.5f, Fade(renderer::palette::ui_text_dim, 0.5f));

                // Controls hint
                DrawTextEx(font, "WASD/Scroll=Camera    L=Locate    F1-F6=Overlays    SPACE=Pause",
                           Vector2{ (float)sw*0.5f - 220.0f, (float)sh*0.5f + 170.0f },
                           11.0f, 0.5f, Fade(renderer::palette::ui_text_dim, 0.45f));

                renderer.end_frame();

                // Enter on button click or ENTER key
                if ((hovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) ||
                    IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE))
                {
                    game_state = GameState::Playing;
                    renderer.fly_to(world::melbourne_data::MAP_WORLD_WIDTH  * 0.5f,
                                    world::melbourne_data::MAP_WORLD_HEIGHT * 0.5f,
                                    0.12f);
                    audio_manager.play_alert();
                }
            }
            audio_manager.update();
            continue; // Skip simulation update while on splash
        }

        // ============================================================
        // (Playing state continues below)
        // ============================================================


        // ---- Camera update ----
        if (exploration_mode)
        {
            // In exploration mode: WASD/arrow keys move the player
            // Camera smoothly follows the player at street level
            player.moving = false;
            float pdx = 0.0f, pdy = 0.0f;
            if (IsKeyDown(KEY_W) || IsKeyDown(KEY_UP))    { pdy -= 1.0f; player.moving = true; }
            if (IsKeyDown(KEY_S) || IsKeyDown(KEY_DOWN))  { pdy += 1.0f; player.moving = true; }
            if (IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT))  { pdx -= 1.0f; player.moving = true; player.facing = -1.0f; }
            if (IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT)) { pdx += 1.0f; player.moving = true; player.facing =  1.0f; }

            // Normalize diagonal
            float plen = std::sqrt(pdx*pdx + pdy*pdy);
            if (plen > 0.01f) { pdx /= plen; pdy /= plen; }

            // Move player and clamp to CBD district boundary
            player.x += pdx * player.speed * frame_dt;
            player.y += pdy * player.speed * frame_dt;
            player.x = std::clamp(player.x, CBD_X_MIN, CBD_X_MAX);
            player.y = std::clamp(player.y, CBD_Y_MIN, CBD_Y_MAX);

            // Advance walk animation
            if (player.moving) player.walk_anim += frame_dt;

            // Detect nearby POI
            player.near_poi = -1;
            for (uint32_t pi = 0; pi < melb_map.poi_count; ++pi)
            {
                float dx = melb_map.pois[pi].x - player.x;
                float dy = melb_map.pois[pi].y - player.y;
                if ((dx*dx + dy*dy) < 120.0f * 120.0f)
                { player.near_poi = static_cast<int>(pi); break; }
            }

            // Camera follows player with smooth lerp
            renderer.fly_to(player.x, player.y, explore_cam_zoom);
        }
        else if (demo_mode)
        {
            demo_timer += frame_dt;
            renderer.pan_camera(15.0f * frame_dt, 8.0f * frame_dt);
            if (demo_timer > 12.0f)
            {
                demo_timer = 0.0f;
                overlay_mode = (overlay_mode + 1) % 6;
                audio_manager.play_blip();
            }
        }
        else if (!renderer.is_flying())
        {
            renderer.update_camera();
        }

        renderer.update_fly_to(frame_dt);

        // ---- World mouse ----
        Vector2 world_mouse = renderer.get_world_mouse_position();

        // ---- Audio ----
        audio_manager.update();

        // ---- Keyboard input ----
        if (IsKeyPressed(KEY_TAB))   { show_profiler = !show_profiler; audio_manager.play_blip(); }
        if (IsKeyPressed(KEY_SPACE)) { is_paused     = !is_paused;     audio_manager.play_blip(); }
        if (IsKeyPressed(KEY_M))     { demo_mode     = !demo_mode;     audio_manager.play_blip(); }
        if (IsKeyPressed(KEY_F5))    { ecs::WorldSerializer::save_to_file(world, save_path); audio_manager.play_alert(); }
        if (IsKeyPressed(KEY_F9))    { ecs::WorldSerializer::load_from_file(world, save_path); audio_manager.play_alert(); }
        if (IsKeyPressed(KEY_F11))   { ToggleFullscreen(); }

        // Exploration mode (G key)
        if (IsKeyPressed(KEY_G))
        {
            exploration_mode = !exploration_mode;
            if (exploration_mode)
            {
                // Save current overview position
                prev_overview_x = renderer.get_camera().target.x;
                prev_overview_y = renderer.get_camera().target.y;
                prev_overview_z = renderer.get_camera().zoom;
                // Fly to player position at street level
                renderer.fly_to(player.x, player.y, explore_cam_zoom);
                audio_manager.play_alert();
            }
            else
            {
                // Fly back to overview
                renderer.fly_to(prev_overview_x, prev_overview_y, prev_overview_z);
                audio_manager.play_blip();
            }
        }

        // Speed controls (only in overview mode)
        if (!exploration_mode)
        {
            if (IsKeyPressed(KEY_Q)) speed = std::max(1u, speed / 2);
            if (IsKeyPressed(KEY_E)) speed = std::min(8u, speed * 2);
        }

        // Exploration scroll zoom
        if (exploration_mode)
        {
            float wheel = GetMouseWheelMove();
            if (wheel != 0.0f)
            {
                explore_cam_zoom += wheel * 0.4f * explore_cam_zoom;
                explore_cam_zoom = std::clamp(explore_cam_zoom, 1.5f, 8.0f);
                renderer.fly_to(player.x, player.y, explore_cam_zoom);
            }
        }

        // Overlay toggles
        if (IsKeyPressed(KEY_F1)) { overlay_mode = (overlay_mode == 1) ? 0 : 1; audio_manager.play_blip(); }
        if (IsKeyPressed(KEY_F2)) { overlay_mode = (overlay_mode == 2) ? 0 : 2; audio_manager.play_blip(); }
        if (IsKeyPressed(KEY_F3)) { overlay_mode = (overlay_mode == 3) ? 0 : 3; audio_manager.play_blip(); }
        if (IsKeyPressed(KEY_F4)) { overlay_mode = (overlay_mode == 4) ? 0 : 4; audio_manager.play_blip(); }
        if (IsKeyPressed(KEY_F5)) { overlay_mode = (overlay_mode == 5) ? 0 : 5; audio_manager.play_blip(); }
        // F6 = profiler (handled separately below)
        if (IsKeyPressed(KEY_F6)) { show_profiler = !show_profiler; audio_manager.play_blip(); }

        // Legacy number keys
        if (IsKeyPressed(KEY_ONE))   overlay_mode = 1;
        if (IsKeyPressed(KEY_TWO))   overlay_mode = 2;
        if (IsKeyPressed(KEY_THREE)) overlay_mode = 3;
        if (IsKeyPressed(KEY_FOUR))  overlay_mode = 4;
        if (IsKeyPressed(KEY_FIVE))  overlay_mode = 5;
        if (IsKeyPressed(KEY_ZERO))  overlay_mode = 0;

        // Locate Citizen — L key picks the nearest citizen or cycles to next
        if (IsKeyPressed(KEY_L))
        {
            // Find nearest citizen to screen center
            float best_dist = 1e9f;
            float cx = renderer.get_camera().target.x;
            float cy = renderer.get_camera().target.y;

            if (selected_citizen_uid == 0)
            {
                // First press: find nearest
                for (uint32_t i = 0; i < citizen_system.citizen_count(); ++i)
                {
                    const auto* c = citizen_system.citizen_slot(i);
                    if (!c) continue;
                    float dx = c->x - cx;
                    float dy = c->y - cy;
                    float d  = dx*dx + dy*dy;
                    if (d < best_dist) { best_dist = d; selected_citizen_uid = c->uid; }
                }
            }
            else
            {
                // Subsequent presses: cycle to next
                selected_citizen_uid = (selected_citizen_uid % citizen_system.citizen_count()) + 1;
            }

            // Fly to selected citizen
            const auto* target_c = citizen_system.find_citizen(selected_citizen_uid);
            if (target_c)
            {
                renderer.fly_to(target_c->x, target_c->y, 3.5f);
                audio_manager.play_alert();
            }
        }
        if (IsKeyPressed(KEY_ESCAPE)) selected_citizen_uid = 0;

        // Event trigger
        if (IsKeyPressed(KEY_T)) event_system.trigger_blackout(world);

        // ---- Interaction ----
        interaction_system.update(world, world_mouse, 64.0f);

        // ---- Fixed-step simulation ----
        profiler.begin_frame();
        if (!is_paused) accumulator += frame_dt * static_cast<float>(speed);

        int steps = 0;
        while (accumulator >= FIXED_DT && steps < MAX_SIM_STEPS)
        {
            traffic_system.update(FIXED_DT);
            economy_system.update(FIXED_DT);
            event_system.update(world, FIXED_DT);
            citizen_system.update(FIXED_DT, traffic_system.get_congestion_metric());

            // Update Melbourne road congestion from traffic density
            float global_cong = traffic_system.get_congestion_metric();
            for (uint32_t i = 0; i < melb_map.edge_count; ++i)
            {
                auto& edge = melb_map.edges[i];
                // Modulate by edge class: arterials absorb more, residentials saturate quicker
                float class_factor = 1.0f - static_cast<float>(static_cast<uint8_t>(edge.road_class)) * 0.06f;
                float base_cong = global_cong * class_factor;
                // Drift toward equilibrium slowly
                edge.congestion += (base_cong - edge.congestion) * 0.05f;
                edge.congestion  = clamp01(edge.congestion);
            }

            // Sync ECS districts
            for (uint32_t i = 0; i < world.district_count(); ++i)
            {
                auto* d = world.district_slot(i);
                if (!d) continue;
                float traffic = traffic_system.get_density(d->grid_x, d->grid_y);
                float capital = economy_system.get_resource(d->entity.index, simulation::economy::ResourceType::Capital);
                float wealth  = (capital > 0.0f) ? capital : d->wealth;
                float stability  = clamp01(1.0f - traffic * 0.65f + wealth / 1500.0f);
                float instability = clamp01(1.0f - stability);
                world.set_district_state(d->entity, d->population, wealth, traffic, stability, instability, instability * 0.5f);
            }

            progression_system.update();
            world.update();
            accumulator -= FIXED_DT;
            ++steps;
        }

        // ---- Rendering ----
        if (renderer.begin_frame())
        {
            ClearBackground(renderer::palette::background);

            renderer.begin_world();

            // 1. Melbourne Road Network (real GPS data)
            melb_renderer.render_roads(melb_map, renderer.get_camera(),
                                        GetScreenWidth(), GetScreenHeight(), overlay_mode);

            // 2. Points of Interest
            melb_renderer.render_pois(melb_map, renderer.get_camera(),
                                       GetScreenWidth(), GetScreenHeight());

            // 3. Citizens
            citizen_renderer.render(citizen_system, renderer.get_camera(),
                                     selected_citizen_uid, GetTime());

            // 4. Player Character (drawn in world-space)
            draw_player(player, renderer.get_camera().zoom, GetTime());

            // 4b. District boundary visualization when in exploration mode
            if (exploration_mode)
            {
                // Draw CBD district boundary as a glowing rectangle
                float pulse = 0.5f + 0.5f * std::sin(GetTime() * 2.0f);
                Rectangle cbd_rect { CBD_X_MIN, CBD_Y_MIN,
                                     CBD_X_MAX - CBD_X_MIN,
                                     CBD_Y_MAX - CBD_Y_MIN };
                DrawRectangleLinesEx(cbd_rect, 4.0f,
                    Fade(Color{0, 220, 150, 255}, pulse * 0.6f));
                DrawRectangleLinesEx(cbd_rect, 1.5f,
                    Fade(Color{0, 255, 180, 255}, pulse));

                // Draw a small compass / area indicator in world space
                // (just a "MELBOURNE CBD" text near the boundary corner)
                DrawText("MELBOURNE CBD",
                         static_cast<int>(CBD_X_MIN + 10),
                         static_cast<int>(CBD_Y_MIN + 10),
                         18, Color{0, 255, 180, 180});
            }

            // 5. POI labels (only when zoomed in)
            auto font_tex = texture_cache.get_font("orbitron_regular");
            melb_renderer.render_labels(melb_map, font_tex, renderer.get_camera());

            // 6. Overlays (heatmaps etc.)
            // Use cell_px equivalent for overlay renderer (Melbourne uses world coords directly)
            overlay_renderer.render(world, 64.0f, static_cast<renderer::OverlayMode>(overlay_mode));


            renderer.end_world();

            // 6. HUD / UI (only in overview mode — or dim in exploration)
            if (!exploration_mode)
            {
                ui_renderer.render(texture_cache, GetScreenWidth(), GetScreenHeight(),
                                    world, traffic_system, economy_system, progression_system,
                                    overlay_mode, speed,
                                    interaction_system.get_selected_district(),
                                    is_paused, show_profiler);
            }

            // 7. EXPLORATION MODE HUD
            if (exploration_mode)
            {
                auto exp_font = texture_cache.get_font("orbitron_regular");
                int sw = GetScreenWidth(), sh = GetScreenHeight();

                // --- Top banner ---
                Rectangle banner { 0, 0, (float)sw, 38.0f };
                DrawRectangleRec(banner, Fade(Color{8, 20, 35, 255}, 0.88f));
                DrawRectangleLinesEx(banner, 1.0f, Fade(Color{0, 255, 180, 255}, 0.4f));

                // Mode indicator
                float bpulse = 0.7f + 0.3f * std::sin(GetTime() * 3.0f);
                DrawCircleV(Vector2{16.0f, 19.0f}, 7.0f, Fade(Color{0, 255, 120, 255}, bpulse));
                DrawTextEx(exp_font, "GROUND EXPLORATION  |  Melbourne CBD District",
                           Vector2{30.0f, 10.0f}, 16.0f, 1, Color{0, 255, 180, 230});

                // Controls (right side of banner)
                DrawTextEx(exp_font, "WASD/Arrow=Move    G=Exit    Scroll=Zoom",
                           Vector2{(float)sw - 340.0f, 12.0f}, 13.0f, 1,
                           Fade(Color{160, 200, 220, 255}, 0.7f));

                // --- Nearby POI popup (bottom-center) ---
                if (player.near_poi >= 0 && player.near_poi < static_cast<int>(melb_map.poi_count))
                {
                    const world::PointOfInterest& nearby = melb_map.pois[player.near_poi];
                    static const Color poi_colors[] = {
                        Color{255,60,60,255}, Color{60,220,100,255}, Color{60,140,255,255},
                        Color{40,200,80,255}, Color{255,200,40,255}, Color{255,120,40,255},
                        Color{140,80,255,255}, Color{180,200,255,255}
                    };
                    uint8_t cat = nearby.category < 8 ? nearby.category : 7;
                    static const char* cat_names[] = {
                        "HOSPITAL","SCHOOL","UNIVERSITY","PARK","STATION","RESTAURANT","GYM","OFFICE"
                    };

                    float popup_w = 320.0f, popup_h = 66.0f;
                    float popup_x = (float)sw * 0.5f - popup_w * 0.5f;
                    float popup_y = (float)sh - popup_h - 16.0f;
                    DrawRectangleRec(Rectangle{popup_x, popup_y, popup_w, popup_h},
                                     Fade(renderer::palette::ui_panel_bg, 0.92f));
                    DrawRectangleLinesEx(Rectangle{popup_x, popup_y, popup_w, popup_h},
                                         1.5f, poi_colors[cat]);
                    DrawCircleV(Vector2{popup_x + 22.0f, popup_y + popup_h * 0.5f},
                                9.0f, poi_colors[cat]);
                    DrawTextEx(exp_font, nearby.name,
                               Vector2{popup_x + 40.0f, popup_y + 10.0f}, 15.0f, 1, WHITE);
                    DrawTextEx(exp_font, cat_names[cat],
                               Vector2{popup_x + 40.0f, popup_y + 30.0f}, 11.0f, 1,
                               Fade(poi_colors[cat], 0.9f));
                    DrawTextEx(exp_font, "[F] Inspect",
                               Vector2{popup_x + popup_w - 80.0f, popup_y + 38.0f},
                               10.0f, 1, Fade(Color{255,220,0,255}, 0.8f));
                }

                // --- Player stats (bottom-left) ---
                {
                    Rectangle stats_panel { 10.0f, (float)sh - 80.0f, 180.0f, 72.0f };
                    DrawRectangleRec(stats_panel, Fade(renderer::palette::ui_panel_bg, 0.85f));
                    DrawRectangleLinesEx(stats_panel, 1.0f, renderer::palette::ui_border);
                    DrawTextEx(exp_font, "YOU", Vector2{20.0f, (float)sh - 72.0f}, 13.0f, 1,
                               Color{0, 255, 180, 255});
                    DrawTextEx(exp_font, "Melbourne CBD", Vector2{20.0f, (float)sh - 56.0f},
                               10.0f, 1, Fade(WHITE, 0.6f));
                    DrawTextEx(exp_font,
                               player.moving ? "Walking..." : "Standing",
                               Vector2{20.0f, (float)sh - 40.0f}, 10.0f, 1,
                               player.moving ? Color{0,220,100,200} : Fade(WHITE, 0.4f));
                    DrawTextEx(exp_font, "Press G to return to City View",
                               Vector2{20.0f, (float)sh - 24.0f}, 9.0f, 1,
                               Fade(renderer::palette::ui_text_dim, 0.7f));
                }
            }

            // 8. Selected citizen panel (if any and not in exploration mode)

            if (selected_citizen_uid > 0)
            {
                const auto* sel = citizen_system.find_citizen(selected_citizen_uid);
                if (sel)
                {
                    auto font = texture_cache.get_font("orbitron_regular");
                    const char* occ_names[] = { "Worker", "Student", "Medic", "Service", "Retired" };
                    const char* state_names[] = { "Idle", "Commuting", "At Work", "Going Home", "Visiting", "Emergency" };
                    const char* mood_names[]  = { "Happy", "Content", "Stressed", "Unhappy" };

                    int sw = GetScreenWidth();
                    Rectangle panel = { (float)sw - 280.0f, 180.0f, 275.0f, 190.0f };
                    DrawRectangleRec(panel, Fade(renderer::palette::ui_panel_bg, 0.85f));
                    DrawRectangleLinesEx(panel, 1.0f, renderer::palette::ui_border);

                    uint8_t occ  = static_cast<uint8_t>(sel->occupation);
                    uint8_t st   = static_cast<uint8_t>(sel->state);
                    uint8_t mood = static_cast<uint8_t>(sel->mood);

                    DrawTextEx(font, TextFormat("CITIZEN #%u", sel->uid),
                               Vector2{panel.x + 10, panel.y + 10}, 15, 1, renderer::palette::ui_accent);
                    DrawTextEx(font, TextFormat("Occupation: %s", occ < 5 ? occ_names[occ] : "?"),
                               Vector2{panel.x + 10, panel.y + 38}, 13, 1, WHITE);
                    DrawTextEx(font, TextFormat("State: %s", st < 6 ? state_names[st] : "?"),
                               Vector2{panel.x + 10, panel.y + 58}, 13, 1, SKYBLUE);
                    DrawTextEx(font, TextFormat("Mood: %s", mood < 4 ? mood_names[mood] : "?"),
                               Vector2{panel.x + 10, panel.y + 78}, 13, 1,
                               sel->mood == simulation::citizens::CitizenMood::Happy ? GREEN :
                               sel->mood == simulation::citizens::CitizenMood::Unhappy ? RED : YELLOW);
                    DrawTextEx(font, TextFormat("Pos: (%.0f, %.0f)", sel->x, sel->y),
                               Vector2{panel.x + 10, panel.y + 98}, 12, 1, renderer::palette::ui_text_dim);
                    DrawTextEx(font, TextFormat("Dest: (%.0f, %.0f)", sel->dest_x, sel->dest_y),
                               Vector2{panel.x + 10, panel.y + 116}, 12, 1, renderer::palette::ui_text_dim);
                    DrawTextEx(font, "[L] Next Citizen  [ESC] Deselect",
                               Vector2{panel.x + 10, panel.y + 155}, 10, 1, renderer::palette::ui_text_dim);
                }
            }

            // 8. MINI-MAP (top-right corner)
            {
                constexpr float MAP_W = 200.0f;
                constexpr float MAP_H = 160.0f;
                constexpr float MAP_PAD = 10.0f;
                int sw = GetScreenWidth();
                float map_x = sw - MAP_W - MAP_PAD;
                float map_y = MAP_PAD;

                // Background
                DrawRectangle(static_cast<int>(map_x), static_cast<int>(map_y),
                              static_cast<int>(MAP_W), static_cast<int>(MAP_H),
                              Fade(renderer::palette::ui_panel_bg, 0.85f));
                DrawRectangleLinesEx(Rectangle{map_x, map_y, MAP_W, MAP_H}, 1.0f,
                                     renderer::palette::ui_border);

                // Compute minimap scale
                float scale_x = MAP_W / world::melbourne_data::MAP_WORLD_WIDTH;
                float scale_y = MAP_H / world::melbourne_data::MAP_WORLD_HEIGHT;

                // Draw roads on minimap
                for (uint32_t ei = 0; ei < melb_map.edge_count; ++ei)
                {
                    const world::RoadEdge& edge = melb_map.edges[ei];
                    if (edge.road_class > world::RoadClass::Secondary) continue; // Only major roads
                    const auto& fn = world::melbourne_data::NODES[edge.from_node];
                    const auto& tn = world::melbourne_data::NODES[edge.to_node];
                    Vector2 s { map_x + fn.x * scale_x, map_y + fn.y * scale_y };
                    Vector2 e { map_x + tn.x * scale_x, map_y + tn.y * scale_y };
                    Color mc = (edge.road_class == world::RoadClass::MotorwayTrunk)
                               ? Color{255, 200, 40, 200} : Color{100, 140, 200, 140};
                    DrawLineV(s, e, mc);
                }

                // Draw citizen dots on minimap
                for (uint32_t ci = 0; ci < citizen_system.citizen_count(); ++ci)
                {
                    const auto* c = citizen_system.citizen_slot(ci);
                    if (!c) continue;
                    float mx = map_x + c->x * scale_x;
                    float my = map_y + c->y * scale_y;
                    DrawPixel(static_cast<int>(mx), static_cast<int>(my), Color{0, 255, 120, 160});
                }

                // Draw camera viewport rect on minimap
                {
                    const Camera2D& cam = renderer.get_camera();
                    int scw = GetScreenWidth(), sch = GetScreenHeight();
                    float vw = static_cast<float>(scw)  / cam.zoom;
                    float vh = static_cast<float>(sch) / cam.zoom;
                    float vx = map_x + (cam.target.x - vw * 0.5f) * scale_x;
                    float vy = map_y + (cam.target.y - vh * 0.5f) * scale_y;
                    DrawRectangleLinesEx(Rectangle{vx, vy, vw * scale_x, vh * scale_y},
                                         1.0f, Fade(WHITE, 0.5f));
                }

                // Label
                auto mini_font = texture_cache.get_font("orbitron_regular");
                DrawTextEx(mini_font, "MELBOURNE", Vector2{map_x + 4, map_y + MAP_H - 14}, 9, 0.5f,
                           Fade(renderer::palette::ui_accent, 0.6f));
            }

            renderer.end_frame();
        }


        profiler.end_frame();
    }

    // ========================================================
    // Shutdown
    // ========================================================
    citizen_system.shutdown();
    progression_system.shutdown();
    economy_system.shutdown();
    traffic_system.shutdown();
    job_system.shutdown();
    profiler.shutdown();
    renderer.shutdown();
    world.shutdown();
    core::Logger::shutdown();

    return 0;
}
