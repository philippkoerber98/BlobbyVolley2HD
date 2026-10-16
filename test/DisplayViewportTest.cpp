#include "DisplayViewport.h"
#include <SDL.h>
#include <iostream>
#include <stdexcept>

static void checkRendering(int width, int height)
{
    auto* surface = SDL_CreateRGBSurfaceWithFormat(0, width, height, 32, SDL_PIXELFORMAT_ARGB8888);
    auto* renderer = surface ? SDL_CreateSoftwareRenderer(surface) : nullptr;
    if (!renderer)
    {
        throw std::runtime_error(SDL_GetError());
    }
    SDL_RenderSetLogicalSize(renderer, 800, 600);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
    SDL_Rect rect = {0, 0, 800, 600};
    SDL_RenderFillRect(renderer, &rect);
    SDL_RenderPresent(renderer);
    auto v = fitGameViewport(width, height);
    auto* pixels = static_cast<Uint32*>(surface->pixels);
    bool centerRed = pixels[(height / 2) * (surface->pitch / 4) + width / 2] == 0xffff0000;
    bool borderBlack = pixels[(height / 2) * (surface->pitch / 4)] == 0xff000000;
    bool edgeRed = pixels[(height / 2) * (surface->pitch / 4) + v.x] == 0xffff0000;
    SDL_DestroyRenderer(renderer);
    SDL_FreeSurface(surface);
    if (!centerRed || !borderBlack || !edgeRed)
    {
        throw std::runtime_error("SDL letterbox rendering failed");
    }
}

static void check(int width, int height, int x, int y, int w, int h)
{
    auto v = fitGameViewport(width, height);
    if (v.x != x || v.y != y || v.w != w || v.h != h)
    {
        throw std::runtime_error("Incorrect display viewport");
    }
}

int main(int, char**)
{
    try
    {
        check(800, 600, 0, 0, 800, 600);
        check(1920, 1080, 240, 0, 1440, 1080);
        check(2560, 1440, 320, 0, 1920, 1440);
        check(3840, 2160, 480, 0, 2880, 2160);
        check(3440, 1440, 760, 0, 1920, 1440);
        check(600, 800, 0, 175, 600, 450);
        check(0, 0, 0, 0, 0, 0);
        check(1001, 751, 0, 0, 1001, 750);
        checkRendering(1920, 1080);
        checkRendering(2560, 1440);
        checkRendering(3840, 2160);
        std::cout << "Viewport tests passed\n";
        return 0;
    }
    catch (const std::exception& e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
