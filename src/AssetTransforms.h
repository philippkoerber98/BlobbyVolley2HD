#pragma once
#include <SDL.h>
#include <algorithm>
#include <cmath>
#include <vector>

// The champion's body and hand use a magenta material key. Neutral highlights
// stay white; gold and silver have no magenta component and retain their color.
inline SDL_Surface* colorizeChampion(SDL_Surface* source, SDL_Color color)
{
    auto* result = SDL_ConvertSurfaceFormat(source, SDL_PIXELFORMAT_RGBA32, 0);
    if (!result)
    {
        return nullptr;
    }
    for (int y = 0; y < result->h; ++y)
    {
        auto* row = static_cast<Uint8*>(result->pixels) + y * result->pitch;
        for (int x = 0; x < result->w; ++x)
        {
            auto* pixel = row + x * 4;
            const int neutral = pixel[1];
            const int magenta = (std::min)(pixel[0], pixel[2]) - neutral;
            if (pixel[3] && magenta > 0)
            {
                pixel[0] = Uint8(neutral + (magenta * color.r + 127) / 255);
                pixel[1] = Uint8(neutral + (magenta * color.g + 127) / 255);
                pixel[2] = Uint8(neutral + (magenta * color.b + 127) / 255);
            }
        }
    }
    return result;
}

// Fill the camera without stretching: a 4:3 camera sees the central part of
// the same widescreen artwork, retaining the full vertical field of view.
inline SDL_Rect backgroundSourceRect(int width, int height, float cameraWidth = 800.f)
{
    const double aspect = cameraWidth / 600.0;
    if (width > height * aspect)
    {
        int w = int(std::lround(height * aspect));
        return {(width - w) / 2, 0, w, height};
    }
    int h = int(std::lround(width / aspect));
    return {0, (height - h) / 2, width, h};
}

// White alpha mask for nine-slice rounded panels. Coverage is evaluated at the
// physical pixel size so smoothing stays one pixel wide at HD/4K and on resize.
inline SDL_Surface* createRoundedPanelMask(int radius)
{
    const int size = radius * 2 + 2, samples = 8;
    auto* mask = SDL_CreateRGBSurfaceWithFormat(0, size, size, 32, SDL_PIXELFORMAT_RGBA32);
    if (!mask)
    {
        return nullptr;
    }
    for (int y = 0; y < size; ++y)
    {
        for (int x = 0; x < size; ++x)
        {
            int inside = 0;
            for (int sy = 0; sy < samples; ++sy)
            {
                for (int sx = 0; sx < samples; ++sx)
                {
                    const float px = x + (sx + .5f) / samples, py = y + (sy + .5f) / samples;
                    const float cx = (std::max)(float(radius), (std::min)(float(size - radius), px));
                    const float cy = (std::max)(float(radius), (std::min)(float(size - radius), py));
                    const float dx = px - cx, dy = py - cy;
                    if (dx * dx + dy * dy <= radius * radius)
                    {
                        ++inside;
                    }
                }
            }
            auto* row = reinterpret_cast<Uint32*>(static_cast<Uint8*>(mask->pixels) + y * mask->pitch);
            row[x] = SDL_MapRGBA(mask->format, 255, 255, 255, Uint8((inside * 255 + 32) / 64));
        }
    }
    SDL_SetSurfaceBlendMode(mask, SDL_BLENDMODE_BLEND);
    return mask;
}

// Menu-only blur: never modify the original court used by gameplay. Build once
// per selected image; renderers cache the resulting texture by filename.
inline SDL_Surface* createMenuBackdrop(SDL_Surface* source, bool preserveAspect = false)
{
    if (!source)
    {
        return nullptr;
    }
    const int h = 300, radius = 2;
    const int w = preserveAspect ? (std::max)(1, int(std::lround(h * double(source->w) / source->h))) : 400;
    auto* image = SDL_CreateRGBSurfaceWithFormat(0, w, h, 32, SDL_PIXELFORMAT_RGBA32);
    if (!image)
    {
        return nullptr;
    }
    SDL_SetSurfaceBlendMode(source, SDL_BLENDMODE_NONE);
    const auto crop =
        preserveAspect ? SDL_Rect{0, 0, source->w, source->h} : backgroundSourceRect(source->w, source->h);
    if (SDL_BlitScaled(source, &crop, image, nullptr) != 0)
    {
        SDL_FreeSurface(image);
        return nullptr;
    }
    std::vector<Uint8> first(w * h * 4), second(w * h * 4);
    for (int y = 0; y < h; ++y)
    {
        std::copy_n(static_cast<Uint8*>(image->pixels) + y * image->pitch, w * 4, first.data() + y * w * 4);
    }
    for (int pass = 0; pass < 3; ++pass)
    {
        for (int axis = 0; axis < 2; ++axis)
        {
            for (int y = 0; y < h; ++y)
            {
                for (int x = 0; x < w; ++x)
                {
                    int sums[3] = {0, 0, 0};
                    for (int d = -radius; d <= radius; ++d)
                    {
                        int xx = (std::max)(0, (std::min)(w - 1, x + (axis ? 0 : d)));
                        int yy = (std::max)(0, (std::min)(h - 1, y + (axis ? d : 0)));
                        for (int c = 0; c < 3; ++c)
                        {
                            sums[c] += first[(yy * w + xx) * 4 + c];
                        }
                    }
                    for (int c = 0; c < 3; ++c)
                    {
                        second[(y * w + x) * 4 + c] = Uint8(sums[c] / (2 * radius + 1));
                    }
                    second[(y * w + x) * 4 + 3] = 255;
                }
            }
            first.swap(second);
        }
    }
    for (int y = 0; y < h; ++y)
    {
        std::copy_n(first.data() + y * w * 4, w * 4, static_cast<Uint8*>(image->pixels) + y * image->pitch);
    }
    return image;
}

// Clean imported sprite alpha at load time without redrawing the artwork.
// Discard isolated extraction debris, retain antialiasing near real bodies,
// and give translucent edge pixels the neighboring opaque sprite color.
inline void cleanSpriteEdges(SDL_Surface* surface)
{
    if (!surface || !surface->format->Amask)
    {
        return;
    }
    auto* rgba = SDL_ConvertSurfaceFormat(surface, SDL_PIXELFORMAT_RGBA32, 0);
    const int w = rgba->w, h = rgba->h;
    std::vector<unsigned char> alpha(w * h), keep(w * h, 0), seen(w * h, 0);
    for (int y = 0; y < h; ++y)
    {
        for (int x = 0; x < w; ++x)
        {
            alpha[y * w + x] = static_cast<Uint8*>(rgba->pixels)[y * rgba->pitch + x * 4 + 3];
        }
    }
    const int minimum = (std::max)(8, w * h / 2000);
    for (int seed = 0; seed < w * h; ++seed)
    {
        if (seen[seed] || alpha[seed] < 200)
        {
            continue;
        }
        std::vector<int> body{seed};
        seen[seed] = 1;
        for (size_t k = 0; k < body.size(); ++k)
        {
            int p = body[k], x = p % w, y = p / w;
            for (int dy = -1; dy <= 1; ++dy)
            {
                for (int dx = -1; dx <= 1; ++dx)
                {
                    int xx = x + dx, yy = y + dy;
                    if (xx < 0 || xx >= w || yy < 0 || yy >= h)
                    {
                        continue;
                    }
                    int n = yy * w + xx;
                    if (!seen[n] && alpha[n] >= 200)
                    {
                        seen[n] = 1;
                        body.push_back(n);
                    }
                }
            }
        }
        if (int(body.size()) < minimum)
        {
            continue;
        }
        for (int p : body)
        {
            for (int dy = -2; dy <= 2; ++dy)
            {
                for (int dx = -2; dx <= 2; ++dx)
                {
                    int xx = p % w + dx, yy = p / w + dy;
                    if (xx >= 0 && xx < w && yy >= 0 && yy < h)
                    {
                        keep[yy * w + xx] = 1;
                    }
                }
            }
        }
    }
    for (int y = 0; y < h; ++y)
    {
        for (int x = 0; x < w; ++x)
        {
            auto* p = static_cast<Uint8*>(rgba->pixels) + y * rgba->pitch + x * 4;
            if (!keep[y * w + x])
            {
                p[3] = 0;
            }
            if (p[3] >= 245)
            {
                continue;
            }
            int best = 100;
            const Uint8* neighbor = nullptr;
            for (int dy = -3; dy <= 3; ++dy)
            {
                for (int dx = -3; dx <= 3; ++dx)
                {
                    int xx = x + dx, yy = y + dy, d = dx * dx + dy * dy;
                    if (xx < 0 || xx >= w || yy < 0 || yy >= h || d >= best)
                    {
                        continue;
                    }
                    if (keep[yy * w + xx] && alpha[yy * w + xx] >= 245)
                    {
                        best = d;
                        neighbor = static_cast<Uint8*>(rgba->pixels) + yy * rgba->pitch + xx * 4;
                    }
                }
            }
            if (neighbor)
            {
                p[0] = neighbor[0];
                p[1] = neighbor[1];
                p[2] = neighbor[2];
            }
        }
    }
    // Preserve the caller's pixel format and existing alpha blending mode.
    SDL_SetSurfaceBlendMode(rgba, SDL_BLENDMODE_NONE);
    SDL_BlitSurface(rgba, nullptr, surface, nullptr);
    SDL_FreeSurface(rgba);
}

inline SDL_Surface* scaleAsset(SDL_Surface* source, int width, int height)
{
    if (!source)
    {
        return nullptr;
    }
    auto* result = SDL_CreateRGBSurfaceWithFormat(0, width, height, 32, SDL_PIXELFORMAT_RGBA32);
    if (!result)
    {
        return nullptr;
    }
    SDL_SetSurfaceBlendMode(source, SDL_BLENDMODE_NONE);
    if (SDL_BlitScaled(source, nullptr, result, nullptr) != 0)
    {
        SDL_FreeSurface(result);
        return nullptr;
    }
    SDL_SetSurfaceBlendMode(result, SDL_BLENDMODE_BLEND);
    return result;
}

enum class VolleyballLayer
{
    Lit,
    Panels,
    Lighting
};

// Separate panel detail from baked lighting so renderers can rotate it continuously.
inline SDL_Surface* rotateVolleyball(SDL_Surface* source, int frame,
                                     VolleyballLayer layer = VolleyballLayer::Lit)
{
    if (!source || frame < 1 || frame > 16)
    {
        return nullptr;
    }
    auto* rgba = SDL_ConvertSurfaceFormat(source, SDL_PIXELFORMAT_RGBA32, 0);
    auto* result = SDL_CreateRGBSurfaceWithFormat(0, 256, 256, 32, SDL_PIXELFORMAT_RGBA32);
    if (!rgba || !result)
    {
        SDL_FreeSurface(rgba);
        SDL_FreeSurface(result);
        return nullptr;
    }
    SDL_FillRect(result, nullptr, 0);
    double angle = (frame - 1) * 6.283185307179586 / 16;
    double c = std::cos(angle), s = std::sin(angle);
    double radius = (rgba->w < rgba->h ? rgba->w : rgba->h) * .386;
    std::vector<double> base(256 * 256), horizontal(256 * 256), illumination(256 * 256);
    for (int y = 0; y < 256; ++y)
    {
        for (int x = 0; x < 256; ++x)
        {
            int sx = int(rgba->w * .5 + (x - 127.5) * radius / 125);
            int sy = int(rgba->h * .5 + (y - 127.5) * radius / 125);
            sx = (std::max)(0, (std::min)(rgba->w - 1, sx));
            sy = (std::max)(0, (std::min)(rgba->h - 1, sy));
            Uint8 r, g, b, a;
            SDL_GetRGBA(reinterpret_cast<Uint32*>(static_cast<Uint8*>(rgba->pixels) + sy * rgba->pitch)[sx],
                        rgba->format, &r, &g, &b, &a);
            base[y * 256 + x] = (r + g + b) / 3.0;
        }
    }
    // Smooth out seams to estimate the baked-in illumination of the source.
    double weights[49];
    for (int k = -24; k <= 24; ++k)
    {
        weights[k + 24] = std::exp(-k * k / (2.0 * 12 * 12));
    }
    for (int y = 0; y < 256; ++y)
    {
        for (int x = 0; x < 256; ++x)
        {
            double sum = 0, total = 0;
            for (int k = -24; k <= 24; ++k)
            {
                int nx = x + k;
                if (nx < 0 || nx >= 256 ||
                    (nx - 127.5) * (nx - 127.5) + (y - 127.5) * (y - 127.5) > 125 * 125)
                {
                    continue;
                }
                sum += base[y * 256 + nx] * weights[k + 24];
                total += weights[k + 24];
            }
            horizontal[y * 256 + x] = total ? sum / total : base[y * 256 + x];
        }
    }
    for (int y = 0; y < 256; ++y)
    {
        for (int x = 0; x < 256; ++x)
        {
            double sum = 0, total = 0;
            for (int k = -24; k <= 24; ++k)
            {
                int ny = y + k;
                if (ny < 0 || ny >= 256 ||
                    (x - 127.5) * (x - 127.5) + (ny - 127.5) * (ny - 127.5) > 125 * 125)
                {
                    continue;
                }
                sum += horizontal[ny * 256 + x] * weights[k + 24];
                total += weights[k + 24];
            }
            illumination[y * 256 + x] = total ? sum / total : base[y * 256 + x];
        }
    }
    for (int y = 0; y < 256; ++y)
    {
        for (int x = 0; x < 256; ++x)
        {
            double dx = x - 127.5, dy = y - 127.5, distance = std::sqrt(dx * dx + dy * dy);
            if (distance > 126)
            {
                continue;
            }
            int sx = (std::max)(0, (std::min)(255, int(127.5 + dx * c + dy * s)));
            int sy = (std::max)(0, (std::min)(255, int(127.5 - dx * s + dy * c)));
            int index = sy * 256 + sx;
            double detail = base[index] / (std::max)(1.0, illumination[index]);
            detail = (std::max)(.65, (std::min)(1.04, detail));
            double nx = dx / 126, ny = dy / 126, nz = std::sqrt((std::max)(0.0, 1 - nx * nx - ny * ny));
            double diffuse = (std::max)(0.0, -.35 * nx - .55 * ny + .76 * nz);
            Uint8 shade = Uint8((std::min)(255.0, (128 + 127 * diffuse) * detail));
            Uint8 a = distance > 125 ? Uint8(255 * (126 - distance)) : 255;
            if (layer == VolleyballLayer::Panels)
            {
                shade = Uint8((std::min)(255.0, 255 * detail));
            }
            else if (layer == VolleyballLayer::Lighting)
            {
                a = Uint8(a * (1.0 - (128 + 127 * diffuse) / 255.0));
                shade = 0;
            }
            reinterpret_cast<Uint32*>(static_cast<Uint8*>(result->pixels) + y * result->pitch)[x] =
                SDL_MapRGBA(result->format, shade, shade, shade, a);
        }
    }
    SDL_FreeSurface(rgba);
    SDL_SetSurfaceBlendMode(result, SDL_BLENDMODE_BLEND);
    return result;
}
