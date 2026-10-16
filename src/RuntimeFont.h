#pragma once
#include <SDL.h>
#include <array>
#include <string>

struct RuntimeGlyph
{
    SDL_Rect source{};
    float x = 0, y = 0, advance = 0;
};
struct RuntimeFontAtlas
{
    std::array<RuntimeGlyph, 384> glyphs;
    int pixels = 0;
};
// Bundled Nunito only; never read untrusted/system fonts.
float nunitoAdvance(unsigned code);
SDL_Surface* createNunitoAtlas(int pixels, RuntimeFontAtlas& atlas);
unsigned nextFontCode(const std::string& text, size_t& offset);
