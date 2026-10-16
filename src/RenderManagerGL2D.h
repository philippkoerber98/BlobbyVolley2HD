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

#if HAVE_LIBGL

#include <SDL.h>

// SDL supplies the platform-specific OpenGL declarations, including the required
// Windows calling conventions. This header must not depend on include order.
#include <SDL_opengl.h>

#include <list>
#include <set>
#include <vector>

#include "RenderManager.h"

/*! \class RenderManagerGL2D
    \brief RenderManager on top of OpenGL
    \details This render manager uses OpenGL for drawing, SDL is only used for loading
            the images.
*/
class RenderManagerGL2D : public RenderManager
{
  public:
    RenderManagerGL2D();
    ~RenderManagerGL2D();

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
    void startDrawParticles() override;
    void drawParticle(const Vector2& pos, int player) override;
    void endDrawParticles() override;
    void drawGame(const DuelMatchState& gameState) override;

  private:
    void uploadFontAtlas(const std::string& name, SDL_Surface* surface) override;
    void releaseFontAtlas(const std::string& name) override;
    void drawFontGlyph(const std::string& name, const SDL_Rect& source, Vector2 position, Vector2 size,
                       Color color) override;
    // Make sure this object is created before any opengl call
    SDL_GLContext mGlContext;

    struct Texture
    {
        float indices[8];
        float w, h;
        GLuint texture;

        Texture(GLuint tex, int x, int y, int w, int h, int tw, int th);
    };

    GLuint mBackground;
    std::unique_ptr<Texture> mNetRod;
    void updateNetRod(SDL_Surface* background);
    GLuint mBallShadow;

    std::vector<GLuint> mBall;
    GLuint mBallPanels = 0;
    GLuint mBallLighting = 0;
    std::vector<GLuint> mBlob;
    std::vector<GLuint> mBlobSpecular;
    std::vector<GLuint> mBlobShadow;
    GLuint mParticle;

    std::list<Vector2> mLastBallStates;

    bool mShowShadow;

    Color mLeftBlobColor;
    Color mRightBlobColor;

    void drawQuad(float x, float y, float width, float height);
    void drawQuad(float x, float y, const Texture& tex);
    GLuint loadTexture(SDL_Surface* surface, bool specular);
    int getNextPOT(int npot);

    void glEnable(unsigned int flag);
    void glDisable(unsigned int flag);
    void glBindTexture(GLuint texture);

    GLuint mCurrentTexture = 0;
    std::set<unsigned int> mCurrentFlags;
};

#endif
