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

/* header include */
#include "RenderManager.h"

/* includes */
#include "AssetTransforms.h"
#include <algorithm>

#include "Blood.h"
#include "IUserConfigReader.h"

/* implementation */

RenderManager::~RenderManager() = default;

RenderManager::RenderManager()
    : mBloodMgr(new BloodManager(IUserConfigReader::createUserConfigReader("config.xml")->getBool("blood")))
{
    mMouseMarkerPosition = -100.0;
}

BloodManager& RenderManager::getBlood()
{
    return *mBloodMgr;
}

SDL_Surface* RenderManager::highlightSurface(SDL_Surface* surface, int luminance)
{
    auto* result = SDL_ConvertSurfaceFormat(surface, SDL_PIXELFORMAT_RGBA32, 0);
    if (!result)
    {
        return nullptr;
    }
    bool colorKey = SDL_HasColorKey(surface);
    SDL_SetColorKey(result, SDL_FALSE, 0);
    for (int y = 0; y < result->h; ++y)
    {
        for (int x = 0; x < result->w; ++x)
        {
            auto* pixel =
                reinterpret_cast<Uint32*>(static_cast<Uint8*>(result->pixels) + y * result->pitch) + x;
            Uint8 r, g, b, a;
            SDL_GetRGBA(*pixel, result->format, &r, &g, &b, &a);
            if (colorKey && !(r | g | b))
            {
                a = 0;
            }
            if (a)
            {
                r = Uint8((std::min)(255, int(r) + luminance));
                g = Uint8((std::min)(255, int(g) + luminance));
                b = Uint8((std::min)(255, int(b) + luminance));
            }
            *pixel = SDL_MapRGBA(result->format, r, g, b, a);
        }
    }
    SDL_SetSurfaceBlendMode(result, SDL_BLENDMODE_BLEND);
    return result;
}

SDL_Surface* RenderManager::createEmptySurface(unsigned int width, unsigned int height)
{
    SDL_Surface* newSurface =
        SDL_CreateRGBSurface(0, width, height, 32, 0x000000FF, 0x0000FF00, 0x00FF0000, 0x00000000);

    return newSurface;
}

void RenderManager::setMouseMarker(float position)
{
    mMouseMarkerPosition = position;
}

SDL_Rect RenderManager::blobRect(const Vector2& position)
{
    SDL_Rect rect = {(short)(lround(position.x) - 37), (short)(lround(position.y) - 44), 75, 89};

    return rect;
}

SDL_Rect RenderManager::ballRect(const Vector2& position)
{
    SDL_Rect rect = {(short)(lround(position.x) - 32), (short)(lround(position.y) - 32), 64, 64};
    return rect;
}

Vector2 RenderManager::ballShadowPosition(const Vector2& position)
{
    return {position.x + (500.f - position.y) / 4.f + 16.f, 500.f - (500.f - position.y) / 16.f - 10.f};
}

SDL_Rect RenderManager::ballShadowRect(const Vector2& position)
{
    SDL_Rect rect = {short(lround(position.x) - 64), short(lround(position.y) - 16), 69, 17};

    return rect;
}

Vector2 RenderManager::blobShadowPosition(const Vector2& position)
{
    return {position.x + (500.f - position.y) / 4 + 16.f, 500.f - (500.f - position.y) / 16.f - 10.f};
}

SDL_Rect RenderManager::blobShadowRect(const Vector2& position)
{
    SDL_Rect rect = {short(lround(position.x) - 64), short(lround(position.y) - 16), 128, 32};
    return rect;
}

void RenderManager::setTitle(const std::string& title)
{
    SDL_SetWindowTitle(mWindow, title.c_str());
}

SDL_Window* RenderManager::getWindow()
{
    return mWindow;
}

Color RenderManager::getOscillationColor() const
{
    float time = float(SDL_GetTicks()) / 1000.f;

    return {int((std::sin(time * 1.5) + 1.0) * 128), int((std::sin(time * 2.5) + 1.0) * 128),
            int((std::sin(time * 3.5) + 1.0) * 128)};
}

void RenderManager::drawConfetti(Uint32 started)
{
    const float time = static_cast<Uint32>(SDL_GetTicks() - started) / 1000.f;
    const Color colors[] = {Color(255, 207, 91), Color(105, 185, 255), Color(255, 164, 82),
                            Color(255, 245, 220)};
    for (unsigned i = 0; i < 100; ++i)
    {
        // Stable per-particle values, independent of game/network random state.
        const unsigned seed = i * 2654435761u + 2246822519u;
        const float phase = (seed % 6283) / 1000.f;
        const float x = 12 + (seed % int(mCourtWidth - 24)) + 14 * std::sin(time * .8f + phase);
        const float y = std::fmod((seed / 997) % 640 + time * (42 + (seed % 53)), 640.f) - 20;
        // Keep winner, score and all controls unobstructed.
        if (x > 137 + (mCourtWidth - 800) / 2 && x < 663 + (mCourtWidth - 800) / 2 && y > 110 && y < 582)
        {
            continue;
        }
        if (x < 1 || x > mCourtWidth - 7 || y < 0 || y > 596)
        {
            continue;
        }
        const float w = 1.5f + 3 * std::abs(std::cos(time * 2 + phase));
        const float h = 3 + 3 * std::abs(std::sin(time * 1.3f + phase));
        drawOverlay(.88f, Vector2(x, y), Vector2(x + w, y + h), colors[i % 4]);
    }
}
