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

#pragma once

#include <SDL.h>
#include <vector>

#include "Global.h"
#include "RenderManager.h"

/*! \class RenderManagerSDL
    \brief Render Manager on top of SDL
    \details This render manager uses SDL for all drawing operations. This means it is
            highly portable, but somewhat slow (e.g. when doing morphing blobs).
*/
class RenderManagerSDL : public RenderManager
{
  public:
    RenderManagerSDL();
    ~RenderManagerSDL();

    void init(int xResolution, int yResolution, bool fullscreen) override;
    void refresh() override;
    bool setVSync(bool enabled) override;

    bool setBackground(const std::string& filename) override;
    void setBlobColor(int player, Color color) override;
    void showShadow(bool shadow) override;

    void drawText(const std::string& text, Vector2 position, unsigned int flags = TF_NORMAL) override;
    void setCourtWidth(float width) override;
    void drawSceneBlur(const std::string& key) override;
    void drawImage(const std::string& filename, Vector2 position, Vector2 size) override;
    void drawRoundedPanel(float opacity, Vector2 pos1, Vector2 pos2, Color col) override;
    void drawOverlay(float opacity, Vector2 pos1, Vector2 pos2, Color col) override;
    void drawBlob(const Vector2& pos, const Color& col, float scale = 1) override;
    void drawParticle(const Vector2& pos, int player) override;
    void drawGame(const DuelMatchState& gameState) override;

  private:
    void uploadFontAtlas(const std::string& name, SDL_Surface* surface) override;
    void releaseFontAtlas(const std::string& name) override;
    void drawFontGlyph(const std::string& name, const SDL_Rect& source, Vector2 position, Vector2 size,
                       Color color) override;
    struct DynamicColoredTexture
    {
        // constructors
        // start surface is expected to have color 0xffffff
        DynamicColoredTexture() : mSDLsf(nullptr), mColor(255, 255, 255) {};
        explicit DynamicColoredTexture(SDL_Texture* sf) : mSDLsf(sf), mColor(255, 255, 255) {};

        DynamicColoredTexture(SDL_Texture* sf, Color c) : mSDLsf(sf), mColor(c) {};

        SDL_Texture* mSDLsf;
        Color mColor;
    };

    SDL_Texture* mBackground;
    SDL_Texture* mNetRod = nullptr;
    SDL_Texture* mBallShadow;
    SDL_Texture* mMarker[2];

    std::vector<SDL_Texture*> mBall;
    SDL_Texture* mBallPanels = nullptr;
    SDL_Texture* mBallLighting = nullptr;
    std::vector<SDL_Surface*> mStandardBlob;
    std::vector<SDL_Surface*> mStandardBlobShadow;
    SDL_Surface* mStandardBlobBlood;
    std::vector<DynamicColoredTexture> mLeftBlob;
    std::vector<DynamicColoredTexture> mLeftBlobShadow;
    DynamicColoredTexture mLeftBlobBlood;

    std::vector<DynamicColoredTexture> mRightBlob;
    std::vector<DynamicColoredTexture> mRightBlobShadow;
    DynamicColoredTexture mRightBlobBlood;

    SDL_Texture* mOverlayTexture = nullptr;

    SDL_Renderer* mRenderer = nullptr;

    bool mShowShadow;

    // Store color for caching
    Color mBlobColor[MAX_PLAYERS];

    // colors a surface
    // the returned SDL_Surface* is already converted into DisplayFormat
    SDL_Surface* colorSurface(SDL_Surface* surface, Color color);

    void colorizeBlobs(int player, int frame);
};
