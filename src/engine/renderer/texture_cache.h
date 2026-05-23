#pragma once

#include "raylib.h"
#include <string>
#include <unordered_map>
#include <vector>

namespace acheron::renderer
{
    class TextureCache final
    {
    public:
        TextureCache() = default;
        ~TextureCache();

        TextureCache(const TextureCache&) = delete;
        TextureCache& operator=(const TextureCache&) = delete;

        void load_all();
        void unload_all();

        Texture2D get_texture(const std::string& id) const;
        Font get_font(const std::string& id) const;

    private:
        std::unordered_map<std::string, Texture2D> m_textures;
        std::unordered_map<std::string, Font> m_fonts;
    };
}
