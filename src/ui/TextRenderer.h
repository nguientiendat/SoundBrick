#pragma once
#include <SDL2/SDL.h>
#include <string>
#include <unordered_map>
#include <vector>

#include <stb/stb_truetype.h>

struct GlyphCache {
    SDL_Texture* texture;
    int width, height;
    int xoff, yoff;
    int advance;
};

class TextRenderer {
public:
    TextRenderer(SDL_Renderer* renderer);
    ~TextRenderer();
    bool loadFont(const std::string& fontPath, float size);
    void drawText(const std::string& text, int x, int y, SDL_Color color);
private:
    SDL_Renderer* m_renderer;
    stbtt_fontinfo m_fontInfo;
    std::vector<unsigned char> m_ttfBuffer;
    float m_scale;
    std::unordered_map<int, GlyphCache> m_glyphs;
    
    int decodeUTF8(const char*& text);
    GlyphCache& getGlyph(int codepoint);
};
