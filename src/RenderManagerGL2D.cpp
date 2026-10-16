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
#include "RenderManagerGL2D.h"

#if HAVE_LIBGL

/* includes */
#include "AssetTransforms.h"
#include "Color.h"
#include "DisplayViewport.h"
#include "DuelMatchState.h"
#include "FileExceptions.h"
#include "FileSystem.h"
#include "Global.h"
#include "HDImagePaths.h"
#include "NetRodOcclusion.h"

static void updateGameViewport(SDL_Window* window, float courtWidth = 800.f)
{
    int width = 0, height = 0;
    SDL_GL_GetDrawableSize(window, &width, &height);
    auto view = fitGameViewport(width, height, courtWidth);
    glViewport(view.x, view.y, view.w, view.h);
}

/* implementation */
RenderManagerGL2D::Texture::Texture(GLuint tex, int x, int y, int width, int height, int tw, int th)
    : w(width), h(height), texture(tex)
{
    assert(x + w <= tw);
    assert(y + h <= th);
    indices[0] = x / (float)tw;
    indices[1] = y / (float)th;
    indices[2] = (x + w) / (float)tw;
    indices[3] = y / (float)th;
    indices[4] = (x + w) / (float)tw;
    indices[5] = (y + h) / (float)th;
    indices[6] = x / (float)tw;
    indices[7] = (y + h) / (float)th;
}
int debugStateChanges = 0;
int debugBindTextureCount = 0;

// wrapper functions for debugging purposes
void RenderManagerGL2D::glEnable(unsigned int flag)
{
    if (mCurrentFlags.find(flag) != mCurrentFlags.end())
    {
        return;
    }

    debugStateChanges++;
    ::glEnable(flag);
    mCurrentFlags.insert(flag);
}

void RenderManagerGL2D::glDisable(unsigned int flag)
{
    if (mCurrentFlags.find(flag) == mCurrentFlags.end())
    {
        return;
    }

    debugStateChanges++;
    ::glDisable(flag);
    mCurrentFlags.erase(mCurrentFlags.find(flag));
}

void RenderManagerGL2D::glBindTexture(GLuint texture)
{
    if (mCurrentTexture == texture)
    {
        return;
    }

    debugBindTextureCount++;
    ::glBindTexture(GL_TEXTURE_2D, texture);
    mCurrentTexture = texture;
}

int RenderManagerGL2D::getNextPOT(int npot)
{
    int pot = 1;
    while (pot < npot)
    {
        pot *= 2;
    }

    return pot;
}

GLuint RenderManagerGL2D::loadTexture(SDL_Surface* surface, bool specular)
{
    SDL_Surface* textureSurface;
    SDL_Surface* convertedTexture;

    textureSurface = surface;

    // Determine size of padding for 2^n format
    int oldX = textureSurface->w;
    int oldY = textureSurface->h;
    int paddedX = getNextPOT(textureSurface->w);
    int paddedY = getNextPOT(textureSurface->h);

    SDL_Rect targetRect;
    targetRect.w = oldX;
    targetRect.h = oldY;
    targetRect.x = (paddedX - oldX) / 2;
    targetRect.y = (paddedY - oldY) / 2;

    if (SDL_HasColorKey(textureSurface) || !textureSurface->format->Amask)
    {
        SDL_SetColorKey(textureSurface, SDL_TRUE, SDL_MapRGB(textureSurface->format, 0, 0, 0));
    }
    convertedTexture = SDL_CreateRGBSurface(SDL_SWSURFACE, paddedX, paddedY, 32,
#if SDL_BYTEORDER == SDL_BIG_ENDIAN
                                            0xff000000, 0x00ff0000, 0x0000ff00, 0x000000ff);
#else
                                            0x000000ff, 0x0000ff00, 0x00ff0000, 0xff000000);
#endif
    SDL_FillRect(convertedTexture, nullptr, SDL_MapRGBA(convertedTexture->format, 0, 0, 0, 0));
    SDL_SetSurfaceBlendMode(textureSurface, SDL_BLENDMODE_NONE);
    SDL_BlitSurface(textureSurface, nullptr, convertedTexture, &targetRect);

    if (specular)
    {
        for (int y = 0; y < convertedTexture->h; ++y)
        {
            for (int x = 0; x < convertedTexture->w; ++x)
            {
                SDL_Color* pixel = &(((SDL_Color*)convertedTexture->pixels)[y * convertedTexture->w + x]);
                int luminance = int(pixel->r) * 5 - 4 * 256 - 138;
                luminance = luminance > 0 ? luminance : 0;
                luminance = luminance < 255 ? luminance : 255;
                pixel->r = luminance;
                pixel->g = luminance;
                pixel->b = luminance;
            }
        }
    }

    GLuint texture;
    glGenTextures(1, &texture);
    glBindTexture(texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, convertedTexture->w, convertedTexture->h, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, convertedTexture->pixels);
    SDL_FreeSurface(textureSurface);
    SDL_FreeSurface(convertedTexture);

    return texture;
}

void RenderManagerGL2D::updateNetRod(SDL_Surface* background)
{
    auto* rod = extractNetRod(background);
    int width = rod->w, height = rod->h;
    int tw = getNextPOT(width), th = getNextPOT(height);
    if (mNetRod)
    {
        glDeleteTextures(1, &mNetRod->texture);
    }
    GLuint texture = loadTexture(rod, false);
    mNetRod.reset(new Texture(texture, (tw - width) / 2, (th - height) / 2, width, height, tw, th));
    mNetRod->w = NetRodOcclusion::w;
    mNetRod->h = NetRodOcclusion::h;
}

void RenderManagerGL2D::drawQuad(float x, float y, float w, float h)
{
    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);

    GLfloat vertices[] = {x - w / 2.f, y - h / 2.f, x + w / 2.f, y - h / 2.f,
                          x + w / 2.f, y + h / 2.f, x - w / 2.f, y + h / 2.f};

    GLfloat texCoords[] = {0.f, 0.f, 1.f, 0.f, 1.f, 1.f, 0.f, 1.f};

    glVertexPointer(2, GL_FLOAT, 0, vertices);
    glTexCoordPointer(2, GL_FLOAT, 0, texCoords);
    glDrawArrays(GL_QUADS, 0, 4);

    glDisableClientState(GL_VERTEX_ARRAY);
    glDisableClientState(GL_TEXTURE_COORD_ARRAY);
}

void RenderManagerGL2D::drawQuad(float x, float y, const Texture& tex)
{
    glBindTexture(tex.texture);

    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);

    float w = tex.w;
    float h = tex.h;

    GLfloat vertices[] = {x - w / 2.f, y - h / 2.f, x + w / 2.f, y - h / 2.f,
                          x + w / 2.f, y + h / 2.f, x - w / 2.f, y + h / 2.f};

    glVertexPointer(2, GL_FLOAT, 0, vertices);
    glTexCoordPointer(2, GL_FLOAT, 0, tex.indices);
    glDrawArrays(GL_QUADS, 0, 4);

    glDisableClientState(GL_VERTEX_ARRAY);
    glDisableClientState(GL_TEXTURE_COORD_ARRAY);
}

RenderManagerGL2D::RenderManagerGL2D() = default;

std::unique_ptr<RenderManager> RenderManager::createRenderManagerGL2D()
{
    return std::unique_ptr<RenderManager>{new RenderManagerGL2D()};
}

bool RenderManagerGL2D::setVSync(bool enabled)
{
    return SDL_GL_SetSwapInterval(enabled ? 1 : 0) == 0;
}

void RenderManagerGL2D::init(int xResolution, int yResolution, bool fullscreen)
{
    glDisable(GL_DEPTH_TEST);
#if !(defined _MSC_VER)
    mCurrentFlags.insert(GL_MULTISAMPLE);
#endif
    SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 16);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

    // Set modesetting
    Uint32 screenFlags = SDL_WINDOW_OPENGL | SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_RESIZABLE;
    if (fullscreen)
    {
        screenFlags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
    }

    // Create window
    mWindow = SDL_CreateWindow(AppTitle, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, xResolution,
                               yResolution, screenFlags);

    // Set icon
#if !(defined BUILD_MACOS_BUNDLE)
    SDL_Surface* icon = loadSurface("Icon.bmp");
    SDL_SetColorKey(icon, SDL_TRUE, SDL_MapRGB(icon->format, 0, 0, 0));
    SDL_SetWindowIcon(mWindow, icon);
    SDL_FreeSurface(icon);
#endif

    // Create gl context
    mGlContext = SDL_GL_CreateContext(mWindow);

    SDL_ShowCursor(0);
#if !(defined _MSC_VER)
    glDisable(GL_MULTISAMPLE);
#endif

    mLeftBlobColor = Color(255, 0, 0);
    mRightBlobColor = Color(0, 255, 0);
    glEnable(GL_TEXTURE_2D);

    // Load background
    SDL_Surface* bgSurface = loadSurface("backgrounds/strand1.bmp");
    updateNetRod(bgSurface);
    BufferedImage* bgBufImage = new BufferedImage;
    bgBufImage->w = getNextPOT(bgSurface->w) / assetScale("backgrounds/strand1.bmp");
    bgBufImage->h = getNextPOT(bgSurface->h) / assetScale("backgrounds/strand1.bmp");
    bgBufImage->sourceWidth = bgSurface->w;
    bgBufImage->sourceHeight = bgSurface->h;
    bgBufImage->glHandle = loadTexture(bgSurface, false);
    mBackground = bgBufImage->glHandle;
    mImageMap["background"] = bgBufImage;

    mBallShadow = loadTexture(loadSurface("gfx/schball.bmp"), false);

    if (FileSystem::getSingleton().exists(preferredHDImage("gfx/hd/volleyball.png")))
    {
        auto* source = loadSurface("gfx/hd/volleyball.png");
        mBallPanels = loadTexture(rotateVolleyball(source, 1, VolleyballLayer::Panels), false);
        mBallLighting = loadTexture(rotateVolleyball(source, 1, VolleyballLayer::Lighting), false);
        SDL_FreeSurface(source);
    }
    else
    {
        for (int i = 1; i <= 16; ++i)
        {
            char filename[64];
            sprintf(filename, "gfx/ball%02d.bmp", i);
            GLuint ballImage = loadTexture(loadSurface(filename), false);
            mBall.push_back(ballImage);
        }
    }

    for (int i = 1; i <= 5; ++i)
    {
        char filename[64];
        sprintf(filename, "gfx/blobbym%d.bmp", i);
        GLuint blobImage = loadTexture(loadSurface(filename), false);
        mBlob.push_back(blobImage);
        sprintf(filename, "gfx/blobbym%d.bmp", i);
        GLuint blobSpecular = loadTexture(loadSurface(filename), true);
        mBlobSpecular.push_back(blobSpecular);
        sprintf(filename, "gfx/sch1%d.bmp", i);
        GLuint blobShadow = loadTexture(loadSurface(filename), false);
        mBlobShadow.push_back(blobShadow);
    }

    mParticle = loadTexture(loadSurface("gfx/blood.bmp"), false);

    updateGameViewport(mWindow, mCourtWidth);
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, 800, 600, 0, -1.0, 1.0);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    glEnable(GL_TEXTURE_2D);

    glAlphaFunc(GL_GREATER, 0.5);
    glEnable(GL_ALPHA_TEST);
}

RenderManagerGL2D::~RenderManagerGL2D()
{
    if (!mBall.empty())
    {
        glDeleteTextures(mBall.size(), mBall.data());
    }
    glDeleteTextures(1, &mBallPanels);
    glDeleteTextures(1, &mBallLighting);
    glDeleteTextures(1, &mBallShadow);
    if (mNetRod)
    {
        glDeleteTextures(1, &mNetRod->texture);
    }
    glDeleteTextures(mBlob.size(), mBlob.data());
    glDeleteTextures(mBlobSpecular.size(), mBlobSpecular.data());
    glDeleteTextures(mBlobShadow.size(), mBlobShadow.data());

    for (auto& iter : mImageMap)
    {
        if (!iter.second)
        {
            continue;
        }
        glDeleteTextures(1, &iter.second->glHandle);
        delete iter.second;
    }

    glDeleteTextures(1, &mParticle);

    SDL_GL_DeleteContext(mGlContext);
    SDL_DestroyWindow(mWindow);
}

bool RenderManagerGL2D::setBackground(const std::string& filename)
{
    try
    {
        SDL_Surface* newSurface = loadSurface(filename);
        updateNetRod(newSurface);
        glDeleteTextures(1, &mBackground);
        delete mImageMap["background"];
        BufferedImage* imgBuffer = new BufferedImage;
        imgBuffer->w = getNextPOT(newSurface->w) / assetScale(filename);
        imgBuffer->h = getNextPOT(newSurface->h) / assetScale(filename);
        imgBuffer->sourceWidth = newSurface->w;
        imgBuffer->sourceHeight = newSurface->h;
        imgBuffer->glHandle = loadTexture(newSurface, false);
        mBackground = imgBuffer->glHandle;
        mImageMap["background"] = imgBuffer;
    }
    catch (const FileLoadException&)
    {
        return false;
    }
    return true;
}

void RenderManagerGL2D::setBlobColor(int player, Color color)
{
    if (player == LEFT_PLAYER)
    {
        mLeftBlobColor = color;
    }

    if (player == RIGHT_PLAYER)
    {
        mRightBlobColor = color;
    }
}

void RenderManagerGL2D::showShadow(bool shadow)
{
    mShowShadow = shadow;
}

void RenderManagerGL2D::uploadTexture(const std::string& name, SDL_Surface* surface)
{
    auto* image = new BufferedImage;
    image->sourceWidth = surface->w;
    image->sourceHeight = surface->h;
    image->w = getNextPOT(surface->w);
    image->h = getNextPOT(surface->h);
    image->glHandle = loadTexture(surface, false);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    mImageMap[name] = image;
}
void RenderManagerGL2D::releaseTexture(const std::string& name)
{
    auto found = mImageMap.find(name);
    if (found == mImageMap.end())
    {
        return;
    }
    glDeleteTextures(1, &found->second->glHandle);
    delete found->second;
    mImageMap.erase(found);
}
void RenderManagerGL2D::drawTextureRegion(const std::string& name, const SDL_Rect& source, Vector2 position,
                                          Vector2 size, Color color)
{
    const auto* image = mImageMap.at(name);
    Texture region(image->glHandle, (image->w - image->sourceWidth) / 2 + source.x,
                   (image->h - image->sourceHeight) / 2 + source.y, source.w, source.h, image->w, image->h);
    region.w = size.x;
    region.h = size.y;
    glEnable(GL_TEXTURE_2D);
    glDisable(GL_ALPHA_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4ub(color.r, color.g, color.b, 255);
    drawQuad(position.x + size.x / 2, position.y + size.y / 2, region);
}
void RenderManagerGL2D::drawText(const std::string& text, Vector2 position, unsigned int flags)
{
    const float cell = flags & TF_SMALL_FONT ? FONT_WIDTH_SMALL : FONT_WIDTH_NORMAL;
    drawRuntimeText(text, position, cell * 1.35f, flags & TF_HIGHLIGHT ? 2 : 0, cell,
                    (flags & TF_OBFUSCATE) != 0);
}

void RenderManagerGL2D::drawSceneBlur(const std::string& key)
{
    const std::string name = "__scene_blur";
    if (mSceneBlurKey != key)
    {
        GLint v[4];
        glGetIntegerv(GL_VIEWPORT, v);
        if (v[2] <= 0 || v[3] <= 0)
        {
            return;
        }
        auto* capture = SDL_CreateRGBSurfaceWithFormat(0, v[2], v[3], 32, SDL_PIXELFORMAT_RGBA32);
        if (!capture)
        {
            return;
        }
        glReadPixels(v[0], v[1], v[2], v[3], GL_RGBA, GL_UNSIGNED_BYTE, capture->pixels);
        auto* pixels = static_cast<Uint8*>(capture->pixels);
        for (int y = 0; y < capture->h / 2; ++y)
        {
            for (int x = 0; x < capture->pitch; ++x)
            {
                std::swap(pixels[y * capture->pitch + x], pixels[(capture->h - 1 - y) * capture->pitch + x]);
            }
        }
        auto* blur = createMenuBackdrop(capture, true);
        SDL_FreeSurface(capture);
        if (!blur)
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
            glDeleteTextures(1, &image->glHandle);
        }
        image->sourceWidth = blur->w;
        image->sourceHeight = blur->h;
        image->w = getNextPOT(blur->w);
        image->h = getNextPOT(blur->h);
        image->glHandle = loadTexture(blur, false);
        mSceneBlurKey = key;
    }
    drawImage(name, Vector2(mCourtWidth / 2, 300), Vector2(mCourtWidth, 600));
}

void RenderManagerGL2D::drawImage(const std::string& filename, Vector2 position, Vector2 size)
{
    if (filename == "background" && size == Vector2(0, 0))
    {
        size = Vector2(800, 600);
    }
    glEnable(GL_TEXTURE_2D);
    glDisable(GL_ALPHA_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    BufferedImage* imageBuffer = mImageMap[filename];
    if (!imageBuffer)
    {
        imageBuffer = new BufferedImage;
        SDL_Surface* newSurface = loadSurface(filename);
        imageBuffer->sourceWidth = newSurface->w;
        imageBuffer->sourceHeight = newSurface->h;
        imageBuffer->w = getNextPOT(newSurface->w) / assetScale(filename);
        imageBuffer->h = getNextPOT(newSurface->h) / assetScale(filename);
        imageBuffer->glHandle = loadTexture(newSurface, false);
        mImageMap[filename] = imageBuffer;
    }

    glColor4f(1.0, 1.0, 1.0, 1.0);
    // glLoadIdentity();
    // glTranslatef(position.x , position.y, 0.0);
    glBindTexture(imageBuffer->glHandle);
    if (size != Vector2(0, 0) && imageBuffer->sourceWidth > 0)
    {
        const int tw = getNextPOT(imageBuffer->sourceWidth), th = getNextPOT(imageBuffer->sourceHeight);
        const bool backdrop = filename == "background" || filename.compare(0, 17, "menu-backgrounds/") == 0;
        const auto crop = backdrop ? backgroundSourceRect(imageBuffer->sourceWidth, imageBuffer->sourceHeight,
                                                          size.x * 600 / size.y)
                                   : SDL_Rect{0, 0, imageBuffer->sourceWidth, imageBuffer->sourceHeight};
        Texture region(imageBuffer->glHandle, (tw - imageBuffer->sourceWidth) / 2 + crop.x,
                       (th - imageBuffer->sourceHeight) / 2 + crop.y, crop.w, crop.h, tw, th);
        region.w = size.x;
        region.h = size.y;
        drawQuad(position.x, position.y, region);
    }
    else
    {
        drawQuad(position.x, position.y, imageBuffer->w, imageBuffer->h);
    }
}

void RenderManagerGL2D::drawRoundedPanel(float opacity, Vector2 pos1, Vector2 pos2, Color col)
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
    GLint viewport[4];
    glGetIntegerv(GL_VIEWPORT, viewport);
    const int radius = (std::max)(1, int(std::ceil(4.f * viewport[2] / mCourtWidth)));
    const int size = radius * 2 + 2, pot = getNextPOT(size), padding = (pot - size) / 2;
    const auto key = "__rounded-panel-" + std::to_string(radius);
    auto*& image = mImageMap[key];
    if (!image)
    {
        auto* mask = createRoundedPanelMask(radius);
        if (!mask)
        {
            return;
        }
        image = new BufferedImage;
        image->glHandle = loadTexture(mask, false);
        image->w = image->h = pot;
    }
    glEnable(GL_TEXTURE_2D);
    glDisable(GL_ALPHA_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(col.r / 255.f, col.g / 255.f, col.b / 255.f, opacity);
    glBindTexture(image->glHandle);
    const int source[] = {0, radius, radius + 2, size};
    const float x[] = {pos1.x, pos1.x + 4, pos2.x - 4, pos2.x},
                y[] = {pos1.y, pos1.y + 4, pos2.y - 4, pos2.y};
    for (int j = 0; j < 3; ++j)
    {
        for (int i = 0; i < 3; ++i)
        {
            Texture region(image->glHandle, padding + source[i], padding + source[j],
                           source[i + 1] - source[i], source[j + 1] - source[j], pot, pot);
            region.w = x[i + 1] - x[i];
            region.h = y[j + 1] - y[j];
            drawQuad((x[i] + x[i + 1]) / 2, (y[j] + y[j + 1]) / 2, region);
        }
    }
}

void RenderManagerGL2D::drawOverlay(float opacity, Vector2 pos1, Vector2 pos2, Color col)
{
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_ALPHA_TEST);
    glEnable(GL_BLEND);

    glColor4f(col.r / 255.f, col.g / 255.f, col.b / 255.f, opacity);
    // glLoadIdentity();
    glBegin(GL_QUADS);
    glVertex2f(pos1.x, pos1.y);
    glVertex2f(pos1.x, pos2.y);
    glVertex2f(pos2.x, pos2.y);
    glVertex2f(pos2.x, pos1.y);
    glEnd();
}

void RenderManagerGL2D::drawBlob(const Vector2& pos, const Color& col, float scale)
{
    glEnable(GL_TEXTURE_2D);
    glDisable(GL_ALPHA_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // glLoadIdentity();
    // glTranslatef(pos.x, pos.y, 0.6);
    glBindTexture(mBlob[0]);
    glColor3ub(col.r, col.g, col.b);
    drawQuad(pos.x, pos.y, 128.0f * scale, 128.0f * scale);

    glEnable(GL_BLEND);
    glColor4f(1.0, 1.0, 1.0, 1.0);
    glBindTexture(mBlobSpecular[0]);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    drawQuad(pos.x, pos.y, 128.0f * scale, 128.0f * scale);
    glDisable(GL_BLEND);
}

void RenderManagerGL2D::startDrawParticles()
{
    glEnable(GL_TEXTURE_2D);
    glEnable(GL_ALPHA_TEST);
    glDisable(GL_BLEND);

    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    glBindTexture(mParticle);
    glBegin(GL_QUADS);
}

void RenderManagerGL2D::drawParticle(const Vector2& pos, int player)
{
    // glLoadIdentity();
    // glTranslatef(pos.x, pos.y, 0.6);

    if (player == LEFT_PLAYER)
    {
        glColor3ub(mLeftBlobColor.r, mLeftBlobColor.g, mLeftBlobColor.b);
    }
    if (player == RIGHT_PLAYER)
    {
        glColor3ub(mRightBlobColor.r, mRightBlobColor.g, mRightBlobColor.b);
    }
    if (player > 1)
    {
        glColor3ub(255, 0, 0);
    }

    float w = 16.0;
    float h = 16.0;
    glTexCoord2f(0.0, 0.0);
    glVertex2f(pos.x - w / 2.f, pos.y - h / 2.f);
    glTexCoord2f(1.0, 0.0);
    glVertex2f(pos.x + w / 2.f, pos.y - h / 2.f);
    glTexCoord2f(1.0, 1.0);
    glVertex2f(pos.x + w / 2.f, pos.y + h / 2.f);
    glTexCoord2f(0.0, 1.0);
    glVertex2f(pos.x - w / 2.f, pos.y + h / 2.f);
}

void RenderManagerGL2D::endDrawParticles()
{
    glEnd();
}

void RenderManagerGL2D::refresh()
{
    // std::cout << debugStateChanges << "\n";
    SDL_GL_SwapWindow(mWindow);
    updateGameViewport(mWindow, mCourtWidth);
    glClear(GL_COLOR_BUFFER_BIT);
    debugStateChanges = 0;
    // std::cerr << debugBindTextureCount << "\n";
    debugBindTextureCount = 0;
}

void RenderManagerGL2D::drawGame(const DuelMatchState& gameState)
{
    // Background
    glDisable(GL_ALPHA_TEST);
    glColor4f(1.0, 1.0, 1.0, 1.0);
    glBindTexture(mBackground);
    glLoadIdentity();
    glDisable(GL_BLEND);
    const auto* bg = mImageMap["background"];
    const auto crop = backgroundSourceRect(bg->sourceWidth, bg->sourceHeight, mCourtWidth);
    const int tw = getNextPOT(bg->sourceWidth), th = getNextPOT(bg->sourceHeight);
    Texture backgroundRegion(mBackground, (tw - bg->sourceWidth) / 2 + crop.x,
                             (th - bg->sourceHeight) / 2 + crop.y, crop.w, crop.h, tw, th);
    backgroundRegion.w = mCourtWidth;
    backgroundRegion.h = 600;
    drawQuad(mCourtWidth / 2, 300.f, backgroundRegion);

    if (mShowShadow)
    {
        // Generic shadow settings

        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glEnable(GL_BLEND);

        // Blob shadows
        Vector2 pos;

        pos = blobShadowPosition(gameState.getBlobPosition(LEFT_PLAYER));
        glColor4ub(mLeftBlobColor.r, mLeftBlobColor.g, mLeftBlobColor.b, 128);
        glBindTexture(mBlobShadow[int(gameState.getBlobState(LEFT_PLAYER)) % 5]);
        drawQuad(pos.x, pos.y, 128.0, 32.0);

        pos = blobShadowPosition(gameState.getBlobPosition(RIGHT_PLAYER));
        glColor4ub(mRightBlobColor.r, mRightBlobColor.g, mRightBlobColor.b, 128);
        glBindTexture(mBlobShadow[int(gameState.getBlobState(RIGHT_PLAYER)) % 5]);
        drawQuad(pos.x, pos.y, 128.0, 32.0);

        // Ball shadow
        pos = ballShadowPosition(gameState.getBallPosition());
        glColor4f(1.0, 1.0, 1.0, 0.5);
        glBindTexture(mBallShadow);
        drawQuad(pos.x, pos.y, 128.0, 32.0);

        glDisable(GL_BLEND);
    }

    // The embedded pole is a masked foreground layer above ground shadows.
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(1.f, 1.f, 1.f, 1.f);
    mNetRod->w = NetRodOcclusion::w;
    drawQuad(mCourtWidth / 2, NetRodOcclusion::y + NetRodOcclusion::h / 2.f, *mNetRod);
    glDisable(GL_BLEND);
    glEnable(GL_ALPHA_TEST);

    // General object settings
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    // The Ball

    glColor4f(1.0, 1.0, 1.0, 1.0);
    if (mBallPanels)
    {
        glDisable(GL_ALPHA_TEST);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glBindTexture(mBallPanels);
        glPushMatrix();
        glTranslatef(gameState.getBallPosition().x, gameState.getBallPosition().y, 0);
        glRotatef(gameState.getBallRotation() * 180.0 / M_PI, 0, 0, 1);
        drawQuad(0, 0, 64.0, 64.0);
        glPopMatrix();
        glBindTexture(mBallLighting);
    }
    else
    {
        glBindTexture(mBall[int(gameState.getBallRotation() / M_PI / 2 * 16) % 16]);
    }
    /*
        float opacity = 0.0;
        for (std::list<Vector2>::iterator iter = mLastBallStates.begin();
            iter != mLastBallStates.end(); ++iter)
        {
    //		glColor4f(1.0 / MotionBlurIterations,
    //			1.0 / MotionBlurIterations, 1.0 / MotionBlurIterations, 1.0 - opacity);
            glColor4f(1.0, 1.0, 1.0, opacity);


            Vector2& ballPosition = *iter;
    */
    drawQuad(gameState.getBallPosition().x, gameState.getBallPosition().y, 64.0, 64.0);

    /*
            opacity += 0.1;
        }
        if (mLastBallStates.size() > MotionBlurIterations)
                mLastBallStates.pop_back();
        glDisable(GL_BLEND);
    */

    // blob normal
    glDisable(GL_ALPHA_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    // left blob
    glBindTexture(mBlob[int(gameState.getBlobState(LEFT_PLAYER)) % 5]);
    glColor3ub(mLeftBlobColor.r, mLeftBlobColor.g, mLeftBlobColor.b);
    drawQuad(gameState.getBlobPosition(LEFT_PLAYER).x, gameState.getBlobPosition(LEFT_PLAYER).y, 128.0,
             128.0);

    // right blob
    glBindTexture(mBlob[int(gameState.getBlobState(RIGHT_PLAYER)) % 5]);
    glColor3ub(mRightBlobColor.r, mRightBlobColor.g, mRightBlobColor.b);
    drawQuad(gameState.getBlobPosition(RIGHT_PLAYER).x, gameState.getBlobPosition(RIGHT_PLAYER).y, 128.0,
             128.0);

    // blob specular
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    glEnable(GL_BLEND);
    glColor4f(1.0, 1.0, 1.0, 1.0);
    // left blob
    glBindTexture(mBlobSpecular[int(gameState.getBlobState(LEFT_PLAYER)) % 5]);
    drawQuad(gameState.getBlobPosition(LEFT_PLAYER).x, gameState.getBlobPosition(LEFT_PLAYER).y, 128.0,
             128.0);

    // right blob
    glBindTexture(mBlobSpecular[int(gameState.getBlobState(RIGHT_PLAYER)) % 5]);
    drawQuad(gameState.getBlobPosition(RIGHT_PLAYER).x, gameState.getBlobPosition(RIGHT_PLAYER).y, 128.0,
             128.0);

    glDisable(GL_BLEND);

    // Ball marker
    glDisable(GL_ALPHA_TEST);
    glDisable(GL_TEXTURE_2D);
    GLubyte markerColor = SDL_GetTicks() % 1000 >= 500 ? 255 : 0;
    glColor3ub(markerColor, markerColor, markerColor);
    drawQuad(gameState.getBallPosition().x, 7.5, 5.0, 5.0);

    // Mouse marker

    // Position relativ zu BallMarker
    drawQuad(mMouseMarkerPosition, 592.5, 5.0, 5.0);

    glEnable(GL_TEXTURE_2D);
    glEnable(GL_ALPHA_TEST);
}

#else

#include "RenderManager.h"

std::unique_ptr<RenderManager> RenderManager::createRenderManagerGL2D()
{
    std::cerr << "OpenGL not available! Falling back to SDL renderer" << std::endl;
    return RenderManager::createRenderManagerSDL();
}

#endif

void RenderManagerGL2D::setCourtWidth(float width)
{
    RenderManager::setCourtWidth(width);
    updateGameViewport(mWindow, width);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, width, 600, 0, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
}
