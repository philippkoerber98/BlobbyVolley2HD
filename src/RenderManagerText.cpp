/*=============================================================================
Blobby Volley 2
Copyright (C) 2006 Jonathan Sieber (jonathan_sieber@yahoo.de)
Copyright (C) 2006 Daniel Knobe (daniel-knobe@web.de)

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
=============================================================================*/

#include "RenderManager.h"
#include "DisplayViewport.h"
#include <algorithm>
#include <cmath>

float RenderManager::menuTextWidth(const std::string& text, float height)
{
    float width = 0;
    for (size_t i = 0; i < text.size();)
    {
        width += nunitoAdvance(nextFontCode(text, i)) * height;
    }
    return width;
}
void RenderManager::drawMenuText(const std::string& text, Vector2 position, float height, int style)
{
    drawRuntimeText(text, position, height, style);
}
void RenderManager::drawRuntimeText(const std::string& text, Vector2 position, float height, int style,
                                    float fixedAdvance, bool obfuscate)
{
    if (height <= 0 || text.empty())
    {
        return;
    }
    int w = 800, h = 600;
    if (mWindow)
    {
        if (SDL_GetWindowFlags(mWindow) & SDL_WINDOW_OPENGL)
        {
            SDL_GL_GetDrawableSize(mWindow, &w, &h);
        }
        else if (auto* renderer = SDL_GetRenderer(mWindow))
        {
            SDL_GetRendererOutputSize(renderer, &w, &h);
        }
        else
        {
            SDL_GetWindowSize(mWindow, &w, &h);
        }
    }
    const auto viewport = fitGameViewport(w, h, mCourtWidth);
    // Bucket physical sizes in four-pixel increments. Atlases are rebuilt from
    // outlines at the active monitor density, never enlarged from PNG glyphs.
    const int pixels = (std::max)(12, (std::min)(256, 4 * int(std::ceil(height * viewport.h / 600.f / 4))));
    auto found = mRuntimeFonts.find(pixels);
    const std::string name = "__nunito_" + std::to_string(pixels);
    if (found == mRuntimeFonts.end())
    {
        // Bound GPU memory while freely resizing or moving between monitors.
        if (mRuntimeFonts.size() >= 16)
        {
            auto oldest = mRuntimeFonts.begin();
            releaseTexture("__nunito_" + std::to_string(oldest->first));
            mRuntimeFonts.erase(oldest);
        }
        RuntimeFontAtlas atlas;
        auto* surface = createNunitoAtlas(pixels, atlas);
        uploadTexture(name, surface);
        found = mRuntimeFonts.emplace(pixels, std::move(atlas)).first;
    }
    const float scale = height / pixels;
    const Color color = style == 1   ? Color(53, 36, 20)
                        : style == 2 ? Color(255, 207, 91)
                        : style == 3 ? Color(145, 133, 111)
                                     : Color(255, 245, 220);
    for (size_t i = 0; i < text.size();)
    {
        unsigned c = nextFontCode(text, i);
        if (obfuscate)
        {
            c = '*';
        }
        if (c < 32 || c >= 384)
        {
            c = '?';
        }
        const auto& g = found->second.glyphs[c];
        float offset = fixedAdvance ? (fixedAdvance - g.advance * scale) / 2 : 0;
        if (g.source.w && g.source.h)
        {
            drawTextureRegion(name, g.source, position + Vector2(offset + g.x * scale, g.y * scale),
                              Vector2(g.source.w * scale, g.source.h * scale), color);
        }
        position.x += fixedAdvance ? fixedAdvance : g.advance * scale;
    }
}
