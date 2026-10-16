#pragma once
#include <SDL.h>
#include <algorithm>
#include <vector>

// Logical 800x600 background coordinates. The modern backgrounds have a
// slightly wider pole than the original 14-pixel restoration strip.
struct NetRodOcclusion
{
    static constexpr int x = 390;
    static constexpr int y = 420;
    static constexpr int w = 20;
    static constexpr int h = 120;
};

// Recover the white/yellow pole silhouette from the central background strip.
// Fill between its outer material edges so lettering on yellow poles stays
// part of the foreground too. Sand outside the silhouette stays transparent.
inline SDL_Surface* extractNetRod(SDL_Surface* background)
{
    auto* rgba = SDL_ConvertSurfaceFormat(background, SDL_PIXELFORMAT_RGBA32, 0);
    const int stripWidth = NetRodOcclusion::w * rgba->h / 600;
    SDL_Rect area = {(rgba->w - stripWidth) / 2, NetRodOcclusion::y * rgba->h / 600, stripWidth,
                     NetRodOcclusion::h * rgba->h / 600};
    auto* result = SDL_CreateRGBSurfaceWithFormat(0, area.w, area.h, 32, SDL_PIXELFORMAT_RGBA32);
    SDL_FillRect(result, nullptr, 0);
    auto material = [](const Uint8* p, int kind)
    {
        if (kind == 2)
        {
            return p[0] > 90 && p[0] > p[1] * 1.8f && p[0] > p[2] * 1.8f;
        }
        return kind == 1 ? p[0] > 140 && p[1] > 100 && p[2] < 110
                         : (std::max)({p[0], p[1], p[2]}) - (std::min)({p[0], p[1], p[2]}) < 26 &&
                               (std::min)({p[0], p[1], p[2]}) > 60;
    };
    // Determine the pole material from its center, away from the rounded cap
    // and spectators. Yellow sand must not count as a white pole's silhouette.
    int yellowVotes = 0, whiteVotes = 0, redVotes = 0;
    for (int yy = 545 * rgba->h / 600; yy < 580 * rgba->h / 600; ++yy)
    {
        auto* p = static_cast<Uint8*>(rgba->pixels) + yy * rgba->pitch + (rgba->w / 2) * 4;
        yellowVotes += material(p, 1);
        whiteVotes += material(p, 0);
        redVotes += material(p, 2);
    }
    int kind = redVotes > (std::max)(yellowVotes, whiteVotes) ? 2 : yellowVotes > whiteVotes ? 1 : 0;
    std::vector<int> leftEdges, rightEdges;
    for (int yy = 545 * rgba->h / 600; yy < 580 * rgba->h / 600; ++yy)
    {
        int left = area.w, right = -1;
        for (int x = 0; x < area.w; ++x)
        {
            auto* p = static_cast<Uint8*>(rgba->pixels) + yy * rgba->pitch + (area.x + x) * 4;
            if (material(p, kind))
            {
                left = (std::min)(left, x);
                right = (std::max)(right, x);
            }
        }
        if (right >= left)
        {
            leftEdges.push_back(left);
            rightEdges.push_back(right);
        }
    }
    int left = area.w / 2, right = left;
    if (!leftEdges.empty())
    {
        std::sort(leftEdges.begin(), leftEdges.end());
        std::sort(rightEdges.begin(), rightEdges.end());
        left = (std::max)(0, leftEdges[leftEdges.size() / 2] - 1);
        right = (std::min)(area.w - 1, rightEdges[rightEdges.size() / 2] + 1);
    }
    for (int y = 0; y < area.h; ++y)
    {
        for (int x = 0; x < area.w; ++x)
        {
            auto* d = static_cast<Uint8*>(result->pixels) + y * result->pitch + x * 4;
            // Color bleed prevents black seams under linear alpha filtering.
            int edgeX = area.x + (std::max)(left, (std::min)(right, x));
            auto* edge = static_cast<Uint8*>(rgba->pixels) + (area.y + y) * rgba->pitch + edgeX * 4;
            d[0] = edge[0];
            d[1] = edge[1];
            d[2] = edge[2];
            d[3] = (x >= left && x <= right) ? 255 : 0;
        }
    }
    SDL_FreeSurface(rgba);
    return result;
}
