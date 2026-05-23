#include "texture_cache.h"
#include "../../core/logger.h"

namespace acheron::renderer
{
    TextureCache::~TextureCache()
    {
        unload_all();
    }

    void TextureCache::load_all()
    {
        // Load fonts
        m_fonts["orbitron_regular"] = LoadFontEx("assets/fonts/orbitron_regular.ttf", 16, nullptr, 0);
        m_fonts["orbitron_medium"] = LoadFontEx("assets/fonts/orbitron_medium.ttf", 20, nullptr, 0);
        m_fonts["orbitron_bold"] = LoadFontEx("assets/fonts/orbitron_bold.ttf", 28, nullptr, 0);
        m_fonts["orbitron_black"] = LoadFontEx("assets/fonts/orbitron_black.ttf", 36, nullptr, 0);

        // Load textures
        m_textures["tileset_tiny_town"] = LoadTexture("assets/tiles/tileset_tiny_town.png");
        m_textures["tileset_cyberpunk"] = LoadTexture("assets/tiles/tileset_cyberpunk.png");
        m_textures["ui_panel_dark"] = LoadTexture("assets/ui/ui_grey_button_rectangle_border.png");
        m_textures["ui_arrow_east"] = LoadTexture("assets/ui/ui_grey_arrow_basic_e.png");
        
        // Load vehicles
        m_textures["vehicle_50"] = LoadTexture("assets/vehicles/vehicle_50.png");
        m_textures["vehicle_51"] = LoadTexture("assets/vehicles/vehicle_51.png");
        m_textures["vehicle_52"] = LoadTexture("assets/vehicles/vehicle_52.png");
        m_textures["vehicle_53"] = LoadTexture("assets/vehicles/vehicle_53.png");

        ACH_LOG_INFO("TextureCache loaded fonts and textures.");
    }

    void TextureCache::unload_all()
    {
        for (auto& [id, tex] : m_textures)
        {
            UnloadTexture(tex);
        }
        m_textures.clear();

        for (auto& [id, font] : m_fonts)
        {
            UnloadFont(font);
        }
        m_fonts.clear();
    }

    Texture2D TextureCache::get_texture(const std::string& id) const
    {
        auto it = m_textures.find(id);
        if (it != m_textures.end()) return it->second;
        return Texture2D{};
    }

    Font TextureCache::get_font(const std::string& id) const
    {
        auto it = m_fonts.find(id);
        if (it != m_fonts.end()) return it->second;
        return GetFontDefault();
    }
}
