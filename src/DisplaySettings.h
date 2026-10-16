#pragma once
#include <SDL.h>
#include <algorithm>
#include <vector>

struct WindowSize
{
    int width, height;
    bool operator==(const WindowSize& other) const
    {
        return width == other.width && height == other.height;
    }
};

struct DisplaySettings
{
    int display = 0;
    int refreshRate = 60; // Conservative fallback when a driver reports zero/unknown.
    int desktopWidth = 800, desktopHeight = 600;
    SDL_Rect usable = {0, 0, 800, 600};
    std::vector<WindowSize> windowSizes;
    std::vector<int> frameLimits;
};

inline std::vector<int> supportedFrameLimits(int refreshRate)
{
    refreshRate = (std::max)(1, refreshRate);
    std::vector<int> limits;
    for (int rate : {30, 50, 60, 72, 75, 90, 100, 120, 144, 165, 180, 200, 240, 300, 360, 480})
    {
        if (rate <= refreshRate)
        {
            limits.push_back(rate);
        }
    }
    if (limits.empty() || limits.back() != refreshRate)
    {
        limits.push_back(refreshRate);
    }
    return limits;
}

inline DisplaySettings queryDisplaySettings(SDL_Window* window)
{
    DisplaySettings result;
    result.display = window ? SDL_GetWindowDisplayIndex(window) : 0;
    if (result.display < 0)
    {
        result.display = 0;
    }
    SDL_DisplayMode mode = {};
    if (SDL_GetCurrentDisplayMode(result.display, &mode) == 0)
    {
        result.desktopWidth = mode.w;
        result.desktopHeight = mode.h;
        if (mode.refresh_rate > 0)
        {
            result.refreshRate = mode.refresh_rate;
        }
    }
    if (SDL_GetDisplayUsableBounds(result.display, &result.usable) != 0)
    {
        result.usable = {0, 0, result.desktopWidth, result.desktopHeight};
    }
    const int maxWidth = (std::max)(320, result.usable.w - 32);
    const int maxHeight = (std::max)(240, result.usable.h - 64);
    auto addSize = [&](int w, int h)
    {
        WindowSize size{w, h};
        if (w >= 640 && h >= 480 && w <= maxWidth && h <= maxHeight &&
            std::find(result.windowSizes.begin(), result.windowSizes.end(), size) == result.windowSizes.end())
        {
            result.windowSizes.push_back(size);
        }
    };
    // Window sizes need not be exclusive display modes; all must fit this desktop.
    for (WindowSize size : {WindowSize{640, 480},
                            {800, 600},
                            {1024, 768},
                            {1280, 720},
                            {1280, 960},
                            {1600, 900},
                            {1920, 1080},
                            {2560, 1440},
                            {3840, 2160}})
    {
        addSize(size.width, size.height);
    }
    for (int i = 0; i < SDL_GetNumDisplayModes(result.display); ++i)
    {
        if (SDL_GetDisplayMode(result.display, i, &mode) == 0)
        {
            addSize(mode.w, mode.h);
        }
    }
    if (result.windowSizes.empty())
    {
        result.windowSizes.push_back({maxWidth, maxHeight});
    }
    std::sort(result.windowSizes.begin(), result.windowSizes.end(), [](WindowSize a, WindowSize b)
              { return a.width == b.width ? a.height < b.height : a.width < b.width; });
    result.frameLimits = supportedFrameLimits(result.refreshRate);
    return result;
}
