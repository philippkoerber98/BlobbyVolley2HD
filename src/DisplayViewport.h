#pragma once

#include <algorithm>

struct DisplayViewport
{
    int x, y, w, h;
};

// Fit a match's actual width while keeping its 600-unit camera height.
inline DisplayViewport fitGameViewport(int width, int height, float logicalWidth = 800.f)
{
    if (width <= 0 || height <= 0)
    {
        return {0, 0, 0, 0};
    }
    int w = width, h = height;
    if (width * 600.0 > height * logicalWidth)
    {
        w = (std::max)(1, int(height * logicalWidth / 600.f));
    }
    else
    {
        h = (std::max)(1, int(width * 600.f / logicalWidth));
    }
    return {(width - w) / 2, (height - h) / 2, w, h};
}
