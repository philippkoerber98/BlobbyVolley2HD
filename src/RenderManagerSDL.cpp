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
#include "RenderManagerSDL.h"
#include "AssetTransforms.h"
#include "DisplayViewport.h"
#include "FileSystem.h"
#include "HDImagePaths.h"
#include "NetRodOcclusion.h"

/* includes */
#include "DuelMatchState.h"
#include "FileExceptions.h"

/* implementation */
SDL_Surface* RenderManagerSDL::colorSurface(SDL_Surface* surface, Color color)
{
    // Create new surface
    SDL_Surface* newSurface = SDL_ConvertSurfaceFormat(surface, SDL_PIXELFORMAT_ABGR8888, 0);

    SDL_LockSurface(newSurface);

    Uint32* surface_pixels = (Uint32*)newSurface->pixels;

    for (int p = 0; p < newSurface->w * newSurface->h; ++p)
    {
        SDL_Color pixel;
        SDL_GetRGBA(surface_pixels[p], newSurface->format, &pixel.r, &pixel.g, &pixel.b, &pixel.a);

        const bool color_key = !(pixel.r | pixel.g | pixel.b);

        if (!color_key)
        {
            int rr = (int(pixel.r) * int(color.r)) >> 8;
            int rg = (int(pixel.g) * int(color.g)) >> 8;
            int rb = (int(pixel.b) * int(color.b)) >> 8;
            int fak = int(pixel.r) * 5 - 4 * 256 - 138;

            auto clamp = [fak](int value)
            {
                // Bright pixels in the original image should remain bright!
                if (fak > 0)
                {
                    value += fak;
                }
                // This is clamped to 1 because dark colors may would be
                // color-keyed otherwise
                return std::max(1, std::min(value, 255));
            };

            surface_pixels[p] = SDL_MapRGBA(newSurface->format, clamp(rr), clamp(rg), clamp(rb), pixel.a);
        }
    }
    SDL_UnlockSurface(newSurface);

    // Alpha sprites keep their real transparency, including opaque black details.
    if (SDL_HasColorKey(surface))
    {
        SDL_SetColorKey(newSurface, SDL_TRUE, SDL_MapRGB(newSurface->format, 0, 0, 0));
    }

    return newSurface;
}

RenderManagerSDL::RenderManagerSDL() = default;

std::unique_ptr<RenderManager> RenderManager::createRenderManagerSDL()
{
    return std::unique_ptr<RenderManager>{new RenderManagerSDL()};
}

bool RenderManagerSDL::setVSync(bool enabled)
{
#if SDL_VERSION_ATLEAST(2, 0, 18)
    return SDL_RenderSetVSync(mRenderer, enabled ? 1 : 0) == 0;
#else
    return false;
#endif
}

void RenderManagerSDL::init(int xResolution, int yResolution, bool fullscreen)
{
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");

    // Set modesetting
    Uint32 screenFlags = SDL_WINDOW_ALLOW_HIGHDPI;
    if (fullscreen)
    {
        screenFlags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
    }
    else
    {
        screenFlags |= SDL_WINDOW_RESIZABLE;
    }
    // Create window
    mWindow = SDL_CreateWindow(AppTitle, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, xResolution,
                               yResolution, screenFlags);

#ifndef BUILD_MACOS_BUNDLE
    // Set icon
    SDL_Surface* icon = loadSurface("Icon.bmp");
    SDL_SetColorKey(icon, SDL_TRUE, SDL_MapRGB(icon->format, 0, 0, 0));
    SDL_SetWindowIcon(mWindow, icon);
    SDL_FreeSurface(icon);
#endif

    // Create renderer to draw in window
    mRenderer = SDL_CreateRenderer(mWindow, -1, 0);

    // Hide mousecursor
    SDL_ShowCursor(0);

    // Draw directly at output resolution while keeping game coordinates stable.
    SDL_SetHint(SDL_HINT_RENDER_LOGICAL_SIZE_MODE, "letterbox");
    SDL_RenderSetLogicalSize(mRenderer, 800, 600);
    SDL_SetRenderDrawColor(mRenderer, 0, 0, 0, 255);
    SDL_RenderClear(mRenderer);

    // Load all textures and surfaces to render the game
    SDL_Surface* tmpSurface;

    // Create a 1x1 black surface which will be scaled to draw an overlay
    tmpSurface = SDL_CreateRGBSurface(0, 1, 1, 32, 0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000);
    // Because of SDL bug we can't check at the moment if color mod is available... no risk no fun ;)
    SDL_FillRect(tmpSurface, nullptr, SDL_MapRGB(tmpSurface->format, 255, 255, 255));
    mOverlayTexture = SDL_CreateTextureFromSurface(mRenderer, tmpSurface);
    SDL_FreeSurface(tmpSurface);

    // Create marker texture for mouse and ball
    tmpSurface = SDL_CreateRGBSurface(0, 5, 5, 32, 0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000);
    SDL_FillRect(tmpSurface, nullptr, SDL_MapRGB(tmpSurface->format, 255, 255, 255));
    mMarker[0] = SDL_CreateTextureFromSurface(mRenderer, tmpSurface);
    SDL_FillRect(tmpSurface, nullptr, SDL_MapRGB(tmpSurface->format, 0, 0, 0));
    mMarker[1] = SDL_CreateTextureFromSurface(mRenderer, tmpSurface);
    SDL_FreeSurface(tmpSurface);

    // Load background
    tmpSurface = loadSurface("backgrounds/strand1.bmp");
    mBackground = SDL_CreateTextureFromSurface(mRenderer, tmpSurface);
    auto* rodSurface = extractNetRod(tmpSurface);
    mNetRod = SDL_CreateTextureFromSurface(mRenderer, rodSurface);
    SDL_SetTextureBlendMode(mNetRod, SDL_BLENDMODE_BLEND);
    SDL_FreeSurface(rodSurface);
    BufferedImage* bgImage = new BufferedImage;
    bgImage->w = tmpSurface->w / assetScale("backgrounds/strand1.bmp");
    bgImage->h = tmpSurface->h / assetScale("backgrounds/strand1.bmp");
    bgImage->sdlImage = mBackground;
    SDL_FreeSurface(tmpSurface);
    mImageMap["background"] = bgImage;

    // Prepare the HD ball once; only the panel texture rotates when drawing.
    if (FileSystem::getSingleton().exists(preferredHDImage("gfx/hd/volleyball.png")))
    {
        auto* source = loadSurface("gfx/hd/volleyball.png");
        auto* panels = rotateVolleyball(source, 1, VolleyballLayer::Panels);
        auto* lighting = rotateVolleyball(source, 1, VolleyballLayer::Lighting);
        mBallPanels = SDL_CreateTextureFromSurface(mRenderer, panels);
        mBallLighting = SDL_CreateTextureFromSurface(mRenderer, lighting);
        SDL_FreeSurface(source);
        SDL_FreeSurface(panels);
        SDL_FreeSurface(lighting);
        if (!mBallPanels || !mBallLighting)
        {
            throw std::runtime_error(SDL_GetError());
        }
        SDL_SetTextureBlendMode(mBallPanels, SDL_BLENDMODE_BLEND);
        SDL_SetTextureBlendMode(mBallLighting, SDL_BLENDMODE_BLEND);
    }
    else
    {
        for (int i = 1; i <= 16; ++i)
        {
            char filename[64];
            sprintf(filename, "gfx/ball%02d.bmp", i);
            tmpSurface = loadSurface(filename);
            SDL_SetColorKey(tmpSurface, SDL_TRUE, SDL_MapRGB(tmpSurface->format, 0, 0, 0));

            SDL_Texture* ballTexture = SDL_CreateTextureFromSurface(mRenderer, tmpSurface);
            SDL_FreeSurface(tmpSurface);
            mBall.push_back(ballTexture);
        }
    }

    // Load ball shadow
    tmpSurface = loadSurface("gfx/schball.bmp");
    SDL_SetColorKey(tmpSurface, SDL_TRUE, SDL_MapRGB(tmpSurface->format, 0, 0, 0));

    SDL_SetSurfaceAlphaMod(tmpSurface, 127);
    mBallShadow = SDL_CreateTextureFromSurface(mRenderer, tmpSurface);
    SDL_FreeSurface(tmpSurface);

    // Load blobby and shadows surface
    // Load streamed textures for coloring
    for (int i = 1; i <= 5; ++i)
    {
        // Load blobby surface
        char filename[64];
        sprintf(filename, "gfx/blobbym%d.bmp", i);
        SDL_Surface* blobImage = loadSurface(filename);
        bool useColorKey = SDL_HasColorKey(blobImage);
        SDL_Surface* formattedBlobImage = SDL_ConvertSurfaceFormat(blobImage, SDL_PIXELFORMAT_ABGR8888, 0);
        SDL_FreeSurface(blobImage);

        if (useColorKey)
        {
            SDL_SetColorKey(formattedBlobImage, SDL_TRUE, SDL_MapRGB(formattedBlobImage->format, 0, 0, 0));
        }
        for (int j = 0; j < formattedBlobImage->w * formattedBlobImage->h; j++)
        {
            SDL_Color* pixel = &(((SDL_Color*)formattedBlobImage->pixels)[j]);
            if (useColorKey && !(pixel->r | pixel->g | pixel->b))
            {
                pixel->a = 0;
            }
        }

        mStandardBlob.push_back(formattedBlobImage);

        // Load blobby shadow surface
        sprintf(filename, "gfx/sch1%d.bmp", i);
        SDL_Surface* blobShadow = loadSurface(filename);
        bool shadowUsesColorKey = SDL_HasColorKey(blobShadow);
        SDL_Surface* formatedBlobShadowImage =
            SDL_ConvertSurfaceFormat(blobShadow, SDL_PIXELFORMAT_ABGR8888, 0);
        SDL_FreeSurface(blobShadow);

        SDL_SetSurfaceAlphaMod(formatedBlobShadowImage, 127);
        if (shadowUsesColorKey)
        {
            SDL_SetColorKey(formatedBlobShadowImage, SDL_TRUE,
                            SDL_MapRGB(formatedBlobShadowImage->format, 0, 0, 0));
        }
        for (int j = 0; j < formatedBlobShadowImage->w * formatedBlobShadowImage->h; j++)
        {
            SDL_Color* pixel = &(((SDL_Color*)formatedBlobShadowImage->pixels)[j]);
            if (shadowUsesColorKey && !(pixel->r | pixel->g | pixel->b))
            {
                pixel->a = 0;
            }
            else
            {
                pixel->a = shadowUsesColorKey ? 127 : pixel->a * 127 / 255;
            }
        }

        mStandardBlobShadow.push_back(formatedBlobShadowImage);

        // Prepare blobby textures
        SDL_Texture* leftBlobTex =
            SDL_CreateTexture(mRenderer, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STREAMING,
                              formattedBlobImage->w, formattedBlobImage->h);
        SDL_SetTextureBlendMode(leftBlobTex, SDL_BLENDMODE_BLEND);
        SDL_UpdateTexture(leftBlobTex, nullptr, formattedBlobImage->pixels, formattedBlobImage->pitch);

        mLeftBlob.emplace_back(leftBlobTex, Color(255, 255, 255));

        SDL_Texture* rightBlobTex =
            SDL_CreateTexture(mRenderer, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STREAMING,
                              formattedBlobImage->w, formattedBlobImage->h);
        SDL_SetTextureBlendMode(rightBlobTex, SDL_BLENDMODE_BLEND);
        SDL_UpdateTexture(rightBlobTex, nullptr, formattedBlobImage->pixels, formattedBlobImage->pitch);

        mRightBlob.emplace_back(rightBlobTex, Color(255, 255, 255));

        // Prepare blobby shadow textures
        SDL_Texture* leftBlobShadowTex =
            SDL_CreateTexture(mRenderer, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STREAMING,
                              formatedBlobShadowImage->w, formatedBlobShadowImage->h);
        SDL_SetTextureBlendMode(leftBlobShadowTex, SDL_BLENDMODE_BLEND);
        mLeftBlobShadow.emplace_back(leftBlobShadowTex, Color(255, 255, 255));
        SDL_UpdateTexture(leftBlobShadowTex, nullptr, formatedBlobShadowImage->pixels,
                          formatedBlobShadowImage->pitch);

        SDL_Texture* rightBlobShadowTex =
            SDL_CreateTexture(mRenderer, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STREAMING,
                              formatedBlobShadowImage->w, formatedBlobShadowImage->h);
        SDL_SetTextureBlendMode(rightBlobShadowTex, SDL_BLENDMODE_BLEND);
        mRightBlobShadow.emplace_back(rightBlobShadowTex, Color(255, 255, 255));
        SDL_UpdateTexture(rightBlobShadowTex, nullptr, formatedBlobShadowImage->pixels,
                          formatedBlobShadowImage->pitch);
    }

    // Load blood surface
    SDL_Surface* blobStandardBlood = loadSurface("gfx/blood.bmp");
    bool bloodUsesColorKey = assetScale("gfx/blood.bmp") == 1;
    SDL_Surface* formatedBlobStandardBlood =
        SDL_ConvertSurfaceFormat(blobStandardBlood, SDL_PIXELFORMAT_ABGR8888, 0);
    SDL_FreeSurface(blobStandardBlood);

    if (bloodUsesColorKey)
    {
        SDL_SetColorKey(formatedBlobStandardBlood, SDL_TRUE,
                        SDL_MapRGB(formatedBlobStandardBlood->format, 0, 0, 0));
    }
    for (int j = 0; j < formatedBlobStandardBlood->w * formatedBlobStandardBlood->h; j++)
    {
        SDL_Color* pixel = &(((SDL_Color*)formatedBlobStandardBlood->pixels)[j]);
        if (bloodUsesColorKey && !(pixel->r | pixel->g | pixel->b))
        {
            pixel->a = 0;
        }
        else
        {
            if (bloodUsesColorKey)
            {
                pixel->a = 255;
            }
        }
    }

    mStandardBlobBlood = formatedBlobStandardBlood;

    // Create streamed textures for blood
    SDL_Texture* leftBlobBlood =
        SDL_CreateTexture(mRenderer, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STREAMING,
                          formatedBlobStandardBlood->w, formatedBlobStandardBlood->h);
    SDL_SetTextureBlendMode(leftBlobBlood, SDL_BLENDMODE_BLEND);
    mLeftBlobBlood = DynamicColoredTexture(leftBlobBlood, Color(255, 0, 0));
    SDL_UpdateTexture(leftBlobBlood, nullptr, formatedBlobStandardBlood->pixels,
                      formatedBlobStandardBlood->pitch);

    SDL_Texture* rightBlobBlood =
        SDL_CreateTexture(mRenderer, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STREAMING,
                          formatedBlobStandardBlood->w, formatedBlobStandardBlood->h);
    SDL_SetTextureBlendMode(rightBlobBlood, SDL_BLENDMODE_BLEND);
    mRightBlobBlood = DynamicColoredTexture(rightBlobBlood, Color(255, 0, 0));
    SDL_UpdateTexture(rightBlobBlood, nullptr, formatedBlobStandardBlood->pixels,
                      formatedBlobStandardBlood->pitch);

    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");
}

RenderManagerSDL::~RenderManagerSDL()
{
    SDL_DestroyTexture(mOverlayTexture);

    for (auto& i : mMarker)
    {
        SDL_DestroyTexture(i);
    }

    for (auto& i : mBall)
    {
        SDL_DestroyTexture(i);
    }
    SDL_DestroyTexture(mBallPanels);
    SDL_DestroyTexture(mBallLighting);

    SDL_DestroyTexture(mBallShadow);
    SDL_DestroyTexture(mNetRod);

    for (unsigned int i = 0; i < mStandardBlob.size(); ++i)
    {
        SDL_FreeSurface(mStandardBlob[i]);
        SDL_FreeSurface(mStandardBlobShadow[i]);
        SDL_DestroyTexture(mLeftBlob[i].mSDLsf);
        SDL_DestroyTexture(mLeftBlobShadow[i].mSDLsf);
        SDL_DestroyTexture(mRightBlob[i].mSDLsf);
        SDL_DestroyTexture(mRightBlobShadow[i].mSDLsf);
    }

    SDL_FreeSurface(mStandardBlobBlood);
    SDL_DestroyTexture(mLeftBlobBlood.mSDLsf);
    SDL_DestroyTexture(mRightBlobBlood.mSDLsf);

    for (const auto& image : mImageMap)
    {
        if (!image.second)
        {
            continue;
        }
        SDL_DestroyTexture(image.second->sdlImage);
        delete image.second;
    }

    SDL_DestroyRenderer(mRenderer);
    SDL_DestroyWindow(mWindow);
}

bool RenderManagerSDL::setBackground(const std::string& filename)
{
    try
    {
        SDL_Surface* tempBackgroundSurface = loadSurface(filename);
        SDL_Texture* tempBackgroundTexture = SDL_CreateTextureFromSurface(mRenderer, tempBackgroundSurface);
        auto* rodSurface = extractNetRod(tempBackgroundSurface);
        SDL_DestroyTexture(mNetRod);
        mNetRod = SDL_CreateTextureFromSurface(mRenderer, rodSurface);
        SDL_SetTextureBlendMode(mNetRod, SDL_BLENDMODE_BLEND);
        SDL_FreeSurface(rodSurface);
        BufferedImage* oldBackground = mImageMap["background"];
        SDL_DestroyTexture(oldBackground->sdlImage);
        delete oldBackground;

        BufferedImage* newImage = new BufferedImage;
        newImage->w = tempBackgroundSurface->w / assetScale(filename);
        newImage->h = tempBackgroundSurface->h / assetScale(filename);
        newImage->sdlImage = tempBackgroundTexture;
        SDL_FreeSurface(tempBackgroundSurface);
        mBackground = newImage->sdlImage;
        mImageMap["background"] = newImage;
    }
    catch (const FileLoadException&)
    {
        return false;
    }
    return true;
}

void RenderManagerSDL::setBlobColor(int player, Color color)
{
    if (color != mBlobColor[player])
    {
        mBlobColor[player] = color;
    }
    else
    {
        return;
    }

    DynamicColoredTexture* handledBlobBlood;

    if (player == LEFT_PLAYER)
    {
        handledBlobBlood = &mLeftBlobBlood;
    }
    if (player == RIGHT_PLAYER)
    {
        handledBlobBlood = &mRightBlobBlood;
    }

    SDL_Surface* tempSurface = colorSurface(mStandardBlobBlood, mBlobColor[player]);
    SDL_UpdateTexture(handledBlobBlood->mSDLsf, nullptr, tempSurface->pixels, tempSurface->pitch);
    SDL_FreeSurface(tempSurface);
}

void RenderManagerSDL::colorizeBlobs(int player, int frame)
{
    std::vector<DynamicColoredTexture>* handledBlob = nullptr;
    std::vector<DynamicColoredTexture>* handledBlobShadow = nullptr;

    if (player == LEFT_PLAYER)
    {
        handledBlob = &mLeftBlob;
        handledBlobShadow = &mLeftBlobShadow;
    }
    if (player == RIGHT_PLAYER)
    {
        handledBlob = &mRightBlob;
        handledBlobShadow = &mRightBlobShadow;
    }

    if ((*handledBlob)[frame].mColor != mBlobColor[player])
    {
        SDL_Surface* tempSurface = colorSurface(mStandardBlob[frame], mBlobColor[player]);
        SDL_UpdateTexture((*handledBlob)[frame].mSDLsf, nullptr, tempSurface->pixels, tempSurface->pitch);
        SDL_FreeSurface(tempSurface);

        SDL_Surface* tempSurface2 = colorSurface(mStandardBlobShadow[frame], mBlobColor[player]);
        SDL_UpdateTexture((*handledBlobShadow)[frame].mSDLsf, nullptr, tempSurface2->pixels,
                          tempSurface2->pitch);
        SDL_FreeSurface(tempSurface2);

        (*handledBlob)[frame].mColor = mBlobColor[player];
    }
}

void RenderManagerSDL::showShadow(bool shadow)
{
    mShowShadow = shadow;
}

void RenderManagerSDL::uploadFontAtlas(const std::string& name, SDL_Surface* surface)
{
    auto* image = new BufferedImage;
    image->w = surface->w;
    image->h = surface->h;
    image->sdlImage = SDL_CreateTextureFromSurface(mRenderer, surface);
    SDL_FreeSurface(surface);
    if (!image->sdlImage)
    {
        delete image;
        throw std::runtime_error(SDL_GetError());
    }
    SDL_SetTextureBlendMode(image->sdlImage, SDL_BLENDMODE_BLEND);
    SDL_SetTextureScaleMode(image->sdlImage, SDL_ScaleModeLinear);
    mImageMap[name] = image;
}
void RenderManagerSDL::releaseFontAtlas(const std::string& name)
{
    auto found = mImageMap.find(name);
    if (found == mImageMap.end())
    {
        return;
    }
    SDL_DestroyTexture(found->second->sdlImage);
    delete found->second;
    mImageMap.erase(found);
}
void RenderManagerSDL::drawFontGlyph(const std::string& name, const SDL_Rect& source, Vector2 position,
                                     Vector2 size, Color color)
{
    auto* texture = mImageMap.at(name)->sdlImage;
    SDL_SetTextureColorMod(texture, color.r, color.g, color.b);
    const SDL_FRect destination = {position.x, position.y, size.x, size.y};
    SDL_RenderCopyF(mRenderer, texture, &source, &destination);
}
void RenderManagerSDL::drawText(const std::string& text, Vector2 position, unsigned int flags)
{
    const float cell = flags & TF_SMALL_FONT ? FONT_WIDTH_SMALL : FONT_WIDTH_NORMAL;
    drawRuntimeText(text, position, cell * 1.35f, flags & TF_HIGHLIGHT ? 2 : 0, cell,
                    (flags & TF_OBFUSCATE) != 0);
}

void RenderManagerSDL::drawSceneBlur(const std::string& key)
{
    const std::string name = "__scene_blur";
    if (mSceneBlurKey != key)
    {
        int w = 0, h = 0;
        SDL_GetRendererOutputSize(mRenderer, &w, &h);
        const auto v = fitGameViewport(w, h, mCourtWidth);
        SDL_Rect rect{v.x, v.y, v.w, v.h};
        auto* capture = SDL_CreateRGBSurfaceWithFormat(0, v.w, v.h, 32, SDL_PIXELFORMAT_RGBA32);
        if (!capture)
        {
            return;
        }
        if (SDL_RenderReadPixels(mRenderer, &rect, SDL_PIXELFORMAT_RGBA32, capture->pixels, capture->pitch) !=
            0)
        {
            SDL_FreeSurface(capture);
            return;
        }
        auto* blur = createMenuBackdrop(capture, true);
        SDL_FreeSurface(capture);
        if (!blur)
        {
            return;
        }
        const int blurWidth = blur->w, blurHeight = blur->h;
        auto* texture = SDL_CreateTextureFromSurface(mRenderer, blur);
        SDL_FreeSurface(blur);
        if (!texture)
        {
            return;
        }
        auto*& image = mImageMap[name];
        if (!image)
        {
            image = new BufferedImage;
        }
        else
        {
            SDL_DestroyTexture(image->sdlImage);
        }
        image->sdlImage = texture;
        image->w = blurWidth;
        image->h = blurHeight;
        mSceneBlurKey = key;
    }
    drawImage(name, Vector2(mCourtWidth / 2, 300), Vector2(mCourtWidth, 600));
}

void RenderManagerSDL::drawImage(const std::string& filename, Vector2 position, Vector2 size)
{
    if (filename == "background")
    {
        if (size == Vector2(0, 0))
        {
            size = Vector2(800, 600);
        }
        int w, h;
        SDL_QueryTexture(mBackground, nullptr, nullptr, &w, &h);
        const auto crop = backgroundSourceRect(w, h, size.x * 600 / size.y);
        const SDL_FRect destination = {position.x - size.x / 2, position.y - size.y / 2, size.x, size.y};
        SDL_RenderCopyF(mRenderer, mBackground, &crop, &destination);
        return;
    }
    BufferedImage* imageBuffer = mImageMap[filename];

    if (!imageBuffer)
    {
        imageBuffer = new BufferedImage;
        SDL_Surface* tmpSurface = loadSurface(filename);
        if (assetScale(filename) == 1)
        {
            SDL_SetColorKey(tmpSurface, SDL_TRUE, SDL_MapRGB(tmpSurface->format, 0, 0, 0));
        }
        imageBuffer->sdlImage = SDL_CreateTextureFromSurface(mRenderer, tmpSurface);
        imageBuffer->w = tmpSurface->w / assetScale(filename);
        imageBuffer->h = tmpSurface->h / assetScale(filename);
        SDL_FreeSurface(tmpSurface);
        mImageMap[filename] = imageBuffer;
    }

    if (filename.compare(0, 17, "menu-backgrounds/") == 0 && size != Vector2(0, 0))
    {
        int w, h;
        SDL_QueryTexture(imageBuffer->sdlImage, nullptr, nullptr, &w, &h);
        const auto crop = backgroundSourceRect(w, h, size.x * 600 / size.y);
        const SDL_FRect destination = {position.x - size.x / 2, position.y - size.y / 2, size.x, size.y};
        SDL_RenderCopyF(mRenderer, imageBuffer->sdlImage, &crop, &destination);
        return;
    }

    if (size == Vector2(0, 0))
    {
        // No scaling
        const SDL_Rect blitRect = {(short)lround(position.x - float(imageBuffer->w) / 2.0),
                                   (short)lround(position.y - float(imageBuffer->h) / 2.0),
                                   (short)imageBuffer->w, (short)imageBuffer->h};
        SDL_RenderCopy(mRenderer, imageBuffer->sdlImage, nullptr, &blitRect);
    }
    else
    {
        // Scaling
        const SDL_Rect blitRect = {(short)lround(position.x - float(size.x) / 2.0),
                                   (short)lround(position.y - float(size.y) / 2.0), (short)size.x,
                                   (short)size.y};
        SDL_RenderCopy(mRenderer, imageBuffer->sdlImage, nullptr, &blitRect);
    }
}

void RenderManagerSDL::drawRoundedPanel(float opacity, Vector2 pos1, Vector2 pos2, Color col)
{
    const float w = pos2.x - pos1.x, h = pos2.y - pos1.y;
    if (w <= 0 || h <= 0)
    {
        return;
    }
    if (w < 8 || h < 8)
    {
        drawOverlay(opacity, pos1, pos2, col);
        return;
    }
    int ow = 0, oh = 0;
    SDL_GetRendererOutputSize(mRenderer, &ow, &oh);
    const auto viewport = fitGameViewport(ow, oh, mCourtWidth);
    const int radius = (std::max)(1, int(std::ceil(4.f * viewport.w / mCourtWidth)));
    const auto key = "__rounded-panel-" + std::to_string(radius);
    auto*& image = mImageMap[key];
    if (!image)
    {
        auto* mask = createRoundedPanelMask(radius);
        if (!mask)
        {
            return;
        }
        auto* texture = SDL_CreateTextureFromSurface(mRenderer, mask);
        SDL_FreeSurface(mask);
        if (!texture)
        {
            return;
        }
        image = new BufferedImage;
        image->sdlImage = texture;
        image->w = image->h = radius * 2 + 2;
        SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
    }
    auto* texture = image->sdlImage;
    SDL_SetTextureColorMod(texture, col.r, col.g, col.b);
    SDL_SetTextureAlphaMod(texture, Uint8((std::max)(0.f, (std::min)(1.f, opacity)) * 255));
    const int source[] = {0, radius, radius + 2, radius * 2 + 2};
    const float x[] = {pos1.x, pos1.x + 4, pos2.x - 4, pos2.x},
                y[] = {pos1.y, pos1.y + 4, pos2.y - 4, pos2.y};
    for (int j = 0; j < 3; ++j)
    {
        for (int i = 0; i < 3; ++i)
        {
            SDL_Rect src{source[i], source[j], source[i + 1] - source[i], source[j + 1] - source[j]};
            SDL_FRect dst{x[i], y[j], x[i + 1] - x[i], y[j + 1] - y[j]};
            SDL_RenderCopyF(mRenderer, texture, &src, &dst);
        }
    }
}

void RenderManagerSDL::drawOverlay(float opacity, Vector2 pos1, Vector2 pos2, Color col)
{
    SDL_Rect ovRect;
    ovRect.x = (int)lround(pos1.x);
    ovRect.y = (int)lround(pos1.y);
    ovRect.w = (int)lround(pos2.x - pos1.x);
    ovRect.h = (int)lround(pos2.y - pos1.y);
    SDL_SetTextureAlphaMod(mOverlayTexture, lround(opacity * 255));
    SDL_SetTextureColorMod(mOverlayTexture, col.r, col.g, col.b);
    SDL_RenderCopy(mRenderer, mOverlayTexture, nullptr, &ovRect);
}

void RenderManagerSDL::drawBlob(const Vector2& pos, const Color& col, float scale)
{
    SDL_Rect position;
    position.x = (int)lround(pos.x);
    position.y = (int)lround(pos.y);

    static int toDraw = 0;

    setBlobColor(toDraw, col);
    /// \todo this recolores the current frame (0)
    /// + shadows; that's not exactly what we want
    colorizeBlobs(toDraw, 0);

    //  Second dirty workaround in the function to have the right position of blobs in the GUI
    position.x = position.x - int(75 * scale / 2);
    position.y = position.y - int(89 * scale / 2);

    if (toDraw == 1)
    {
        SDL_QueryTexture(mRightBlob[0].mSDLsf, nullptr, nullptr, &position.w, &position.h);
        position.w /= assetScale("gfx/blobbym1.bmp");
        position.h /= assetScale("gfx/blobbym1.bmp");
        position.w = int(position.w * scale);
        position.h = int(position.h * scale);
        SDL_RenderCopy(mRenderer, mRightBlob[0].mSDLsf, nullptr, &position);
        toDraw = 0;
    }
    else
    {
        SDL_QueryTexture(mLeftBlob[0].mSDLsf, nullptr, nullptr, &position.w, &position.h);
        position.w /= assetScale("gfx/blobbym1.bmp");
        position.h /= assetScale("gfx/blobbym1.bmp");
        position.w = int(position.w * scale);
        position.h = int(position.h * scale);
        SDL_RenderCopy(mRenderer, mLeftBlob[0].mSDLsf, nullptr, &position);
        toDraw = 1;
    }
}

void RenderManagerSDL::drawParticle(const Vector2& pos, int player)
{
    SDL_Rect blitRect = {
        (short)lround(pos.x - float(9) / 2.0),
        (short)lround(pos.y - float(9) / 2.0),
        (short)9,
        (short)9,
    };

    DynamicColoredTexture blood = player == LEFT_PLAYER ? mLeftBlobBlood : mRightBlobBlood;

    SDL_RenderCopy(mRenderer, blood.mSDLsf, nullptr, &blitRect);
}

void RenderManagerSDL::refresh()
{
    SDL_RenderPresent(mRenderer);
    SDL_RenderClear(mRenderer);
}

void RenderManagerSDL::drawGame(const DuelMatchState& gameState)
{
    int backgroundWidth, backgroundHeight;
    SDL_QueryTexture(mBackground, nullptr, nullptr, &backgroundWidth, &backgroundHeight);
    const auto crop = backgroundSourceRect(backgroundWidth, backgroundHeight, mCourtWidth);
    const SDL_Rect backgroundDestination = {0, 0, int(std::lround(mCourtWidth)), 600};
    SDL_RenderCopy(mRenderer, mBackground, &crop, &backgroundDestination);

    SDL_Rect position;

    // Ball marker
    position.y = 5;
    position.x = (int)lround(gameState.getBallPosition().x - 2.5);
    position.w = 5;
    position.h = 5;
    SDL_RenderCopy(mRenderer, mMarker[(int)SDL_GetTicks() % 1000 >= 500], nullptr, &position);

    // Mouse marker
    position.y = 590;
    position.x = (int)lround(mMouseMarkerPosition - 2.5);
    position.w = 5;
    position.h = 5;
    SDL_RenderCopy(mRenderer, mMarker[(int)SDL_GetTicks() % 1000 >= 500], nullptr, &position);

    if (mShowShadow)
    {
        // Ball Shadow
        auto shadow = ballShadowPosition(gameState.getBallPosition());
        SDL_FRect shadowRect = {shadow.x - 64, shadow.y - 16, 69, 17};
        SDL_RenderCopyF(mRenderer, mBallShadow, nullptr, &shadowRect);

        // Left blob shadow
        shadow = blobShadowPosition(gameState.getBlobPosition(LEFT_PLAYER));
        shadowRect = {shadow.x - 64, shadow.y - 16, 128, 32};
        int animationState = int(gameState.getBlobState(LEFT_PLAYER)) % 5;
        SDL_RenderCopyF(mRenderer, mLeftBlobShadow[animationState].mSDLsf, nullptr, &shadowRect);

        // Right blob shadow
        shadow = blobShadowPosition(gameState.getBlobPosition(RIGHT_PLAYER));
        shadowRect = {shadow.x - 64, shadow.y - 16, 128, 32};
        animationState = int(gameState.getBlobState(RIGHT_PLAYER)) % 5;
        SDL_RenderCopyF(mRenderer, mRightBlobShadow[animationState].mSDLsf, nullptr, &shadowRect);
    }

    // Restore the rod
    SDL_Rect rodPosition;
    rodPosition.x = int(std::lround((mCourtWidth - NetRodOcclusion::w) / 2));
    rodPosition.y = NetRodOcclusion::y;
    rodPosition.w = NetRodOcclusion::w;
    rodPosition.h = NetRodOcclusion::h;
    SDL_RenderCopy(mRenderer, mNetRod, nullptr, &rodPosition);

    // Drawing the Ball
    // Preserve fractional positions from interpolation through the SDL draw call.
    const auto ball = gameState.getBallPosition();
    SDL_FRect movingPosition = {ball.x - 32, ball.y - 32, 64, 64};
    if (mBallPanels)
    {
        SDL_RenderCopyExF(mRenderer, mBallPanels, nullptr, &movingPosition,
                          gameState.getBallRotation() * 180.0 / M_PI, nullptr, SDL_FLIP_NONE);
        SDL_RenderCopyF(mRenderer, mBallLighting, nullptr, &movingPosition);
    }
    else
    {
        int animationState = int(gameState.getBallRotation() / M_PI / 2 * 16) % 16;
        SDL_RenderCopyF(mRenderer, mBall[animationState], nullptr, &movingPosition);
    }

    // update blob colors
    int leftFrame = int(gameState.getBlobState(LEFT_PLAYER)) % 5;
    int rightFrame = int(gameState.getBlobState(RIGHT_PLAYER)) % 5;
    colorizeBlobs(LEFT_PLAYER, leftFrame);
    colorizeBlobs(RIGHT_PLAYER, rightFrame);

    // Drawing left blob
    const auto left = gameState.getBlobPosition(LEFT_PLAYER);
    movingPosition = {left.x - 37, left.y - 44, 75, 89};
    SDL_RenderCopyF(mRenderer, mLeftBlob[leftFrame].mSDLsf, nullptr, &movingPosition);

    // Drawing right blob
    const auto right = gameState.getBlobPosition(RIGHT_PLAYER);
    movingPosition = {right.x - 37, right.y - 44, 75, 89};
    SDL_RenderCopyF(mRenderer, mRightBlob[rightFrame].mSDLsf, nullptr, &movingPosition);
}

void RenderManagerSDL::setCourtWidth(float width)
{
    RenderManager::setCourtWidth(width);
    SDL_RenderSetLogicalSize(mRenderer, int(std::lround(width)), 600);
}
