#include "RuntimeFont.h"
#include "FileRead.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <vector>
#define STB_TRUETYPE_IMPLEMENTATION
#include "third_party/stb_truetype.h"

namespace
{
struct Font
{
    std::vector<unsigned char> bytes;
    stbtt_fontinfo info{};
    Font()
    {
        FileRead f("gfx/hd/Nunito.ttf");
        auto raw = f.readRawBytes(f.length());
        bytes.assign(raw.begin(), raw.end());
        if (bytes.empty() || !stbtt_InitFont(&info, bytes.data(), 0))
        {
            throw std::runtime_error("Cannot load bundled Nunito.ttf");
        }
    }
};
Font& font()
{
    static Font value;
    return value;
}
} // namespace
unsigned nextFontCode(const std::string& text, size_t& i)
{
    unsigned c = static_cast<unsigned char>(text[i++]);
    if (c < 128)
    {
        return c;
    }
    if ((c & 0xe0) == 0xc0 && i < text.size() && (static_cast<unsigned char>(text[i]) & 0xc0) == 0x80)
    {
        return ((c & 31) << 6) | (static_cast<unsigned char>(text[i++]) & 63);
    }
    while (i < text.size() && (static_cast<unsigned char>(text[i]) & 0xc0) == 0x80)
    {
        ++i;
    }
    return '?';
}
float nunitoAdvance(unsigned code)
{
    int advance, bearing;
    stbtt_GetCodepointHMetrics(&font().info, code < 384 ? code : '?', &advance, &bearing);
    return advance * stbtt_ScaleForMappingEmToPixels(&font().info, 42.f) / 64.f;
}
SDL_Surface* createNunitoAtlas(int pixels, RuntimeFontAtlas& atlas)
{
    auto& info = font().info;
    atlas.pixels = pixels;
    const float scale = stbtt_ScaleForMappingEmToPixels(&info, pixels * 42.f / 64.f);
    int ascent, descent, gap;
    stbtt_GetFontVMetrics(&info, &ascent, &descent, &gap);
    const int width = 2048, padding = 3;
    int x = padding, y = padding, rowHeight = 0;
    for (unsigned c = 32; c < 384; ++c)
    {
        int x0, y0, x1, y1, advance, bearing;
        stbtt_GetCodepointBitmapBox(&info, c, scale, scale, &x0, &y0, &x1, &y1);
        const int w = x1 - x0, h = y1 - y0;
        if (x + w + padding > width)
        {
            x = padding;
            y += rowHeight + padding * 2;
            rowHeight = 0;
        }
        auto& g = atlas.glyphs[c];
        g.source = {x, y, w, h};
        g.x = float(x0);
        g.y = y0 + ascent * scale + pixels * 2.f / 64;
        stbtt_GetCodepointHMetrics(&info, c, &advance, &bearing);
        g.advance = advance * scale;
        x += w + padding * 2;
        rowHeight = (std::max)(rowHeight, h);
    }
    int height = 1;
    while (height < y + rowHeight + padding)
    {
        height *= 2;
    }
    auto* image = SDL_CreateRGBSurfaceWithFormat(0, width, height, 32, SDL_PIXELFORMAT_RGBA32);
    if (!image)
    {
        throw std::runtime_error(SDL_GetError());
    }
    // White transparent gutters prevent dark fringes under linear filtering.
    SDL_FillRect(image, nullptr, SDL_MapRGBA(image->format, 255, 255, 255, 0));
    for (unsigned c = 32; c < 384; ++c)
    {
        const auto& r = atlas.glyphs[c].source;
        if (!r.w || !r.h)
        {
            continue;
        }
        std::vector<unsigned char> alpha(r.w * r.h);
        stbtt_MakeCodepointBitmap(&info, alpha.data(), r.w, r.h, r.w, scale, scale, c);
        for (int yy = 0; yy < r.h; ++yy)
        {
            for (int xx = 0; xx < r.w; ++xx)
            {
                static_cast<Uint8*>(image->pixels)[(r.y + yy) * image->pitch + (r.x + xx) * 4 + 3] =
                    alpha[yy * r.w + xx];
            }
        }
    }
    return image;
}
