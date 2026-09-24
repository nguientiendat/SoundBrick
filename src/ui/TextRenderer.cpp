#include "TextRenderer.h"

#define STB_TRUETYPE_IMPLEMENTATION
#include <stb/stb_truetype.h>

#include <fstream>
#include <iostream>

TextRenderer::TextRenderer(SDL_Renderer* renderer) : m_renderer(renderer), m_scale(1.0f) {}

TextRenderer::~TextRenderer() {
    for (auto& pair : m_glyphs) {
        if (pair.second.texture) SDL_DestroyTexture(pair.second.texture);
    }
}

bool TextRenderer::loadFont(const std::string& fontPath, float size) {
    std::ifstream file(fontPath, std::ios::binary | std::ios::ate);
    if (!file.is_open()) return false;
    std::streamsize fileSize = file.tellg();
    file.seekg(0, std::ios::beg);
    m_ttfBuffer.resize(fileSize);
    if (!file.read((char*)m_ttfBuffer.data(), fileSize)) return false;

    if (!stbtt_InitFont(&m_fontInfo, m_ttfBuffer.data(), stbtt_GetFontOffsetForIndex(m_ttfBuffer.data(), 0))) return false;
    m_scale = stbtt_ScaleForPixelHeight(&m_fontInfo, size);
    return true;
}

int TextRenderer::decodeUTF8(const char*& text) {
    unsigned char c = *text++;
    if (c < 0x80) return c;
    if ((c & 0xE0) == 0xC0) return ((c & 0x1F) << 6) | ((*text++) & 0x3F);
    if ((c & 0xF0) == 0xE0) {
        int cp = ((c & 0x0F) << 12);
        cp |= ((*text++) & 0x3F) << 6;
        cp |= ((*text++) & 0x3F);
        return cp;
    }
    if ((c & 0xF8) == 0xF0) {
        int cp = ((c & 0x07) << 18);
        cp |= ((*text++) & 0x3F) << 12;
        cp |= ((*text++) & 0x3F) << 6;
        cp |= ((*text++) & 0x3F);
        return cp;
    }
    return '?';
}

GlyphCache& TextRenderer::getGlyph(int codepoint) {
    auto it = m_glyphs.find(codepoint);
    if (it != m_glyphs.end()) return it->second;

    GlyphCache gc = {nullptr, 0, 0, 0, 0};
    unsigned char* bitmap = stbtt_GetCodepointBitmap(&m_fontInfo, 0, m_scale, codepoint, &gc.width, &gc.height, &gc.xoff, &gc.yoff);
    
    int advance, lsb;
    stbtt_GetCodepointHMetrics(&m_fontInfo, codepoint, &advance, &lsb);
    gc.advance = advance * m_scale;

    if (bitmap) {
        SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormat(0, gc.width, gc.height, 32, SDL_PIXELFORMAT_ARGB8888);
        if (surface) {
            SDL_LockSurface(surface);
            Uint32* pixels = (Uint32*)surface->pixels;
            for (int i = 0; i < gc.width * gc.height; i++) {
                Uint8 alpha = bitmap[i];
                pixels[i] = (alpha << 24) | 0xFFFFFF; // Trắng, dùng alpha để anti-alias
            }
            SDL_UnlockSurface(surface);
            
            gc.texture = SDL_CreateTextureFromSurface(m_renderer, surface);
            SDL_SetTextureBlendMode(gc.texture, SDL_BLENDMODE_BLEND);
            SDL_FreeSurface(surface);
        }
        stbtt_FreeBitmap(bitmap, nullptr);
    }
    m_glyphs[codepoint] = gc;
    return m_glyphs[codepoint];
}

void TextRenderer::drawText(const std::string& text, int x, int y, SDL_Color color) {
    if (m_ttfBuffer.empty()) return;
    int currentX = x;
    const char* ptr = text.c_str();
    
    int ascent, descent, lineGap;
    stbtt_GetFontVMetrics(&m_fontInfo, &ascent, &descent, &lineGap);
    int baseline = y + ascent * m_scale;

    while (*ptr) {
        int cp = decodeUTF8(ptr);
        GlyphCache& gc = getGlyph(cp);
        if (gc.texture) {
            SDL_SetTextureColorMod(gc.texture, color.r, color.g, color.b);
            SDL_SetTextureAlphaMod(gc.texture, color.a);
            SDL_Rect dst = { currentX + gc.xoff, baseline + gc.yoff, gc.width, gc.height };
            SDL_RenderCopy(m_renderer, gc.texture, nullptr, &dst);
        }
        currentX += gc.advance;
    }
}
