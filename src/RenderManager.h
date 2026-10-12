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

#include <map>
#include <SDL.h>
#include "RuntimeFont.h"

#include "Vector.h"
#include "Color.h"
#include "BlobbyDebug.h"


class BloodManager;
struct DuelMatchState;

// Text definitions
static const int FONT_WIDTH_NORMAL = 24;
static const int LINE_SPACER_NORMAL	=	 6;	// Extra space between 2 lines in a normal SelectBox.
static const int FONT_WIDTH_SMALL = 12;
static const int LINE_SPACER_SMALL	=	 3;	// Extra space between 2 lines in a small SelectBox.



// Text flags (usable for the RenderManager::drawText() flag parameter)
// Just using one byte for now - up to 8 flags.
static const int TF_NORMAL		= 0x00; // 0 == false (backward compatibility for state modules)
static const int TF_HIGHLIGHT	= 0x01; // 1 == true (backward compatibility for state modules)
static const int TF_SMALL_FONT	= 0x02; // Draw a smaller font. (8px instead of 24px)
static const int TF_OBFUSCATE	= 0x04; // Obfuscate the text with asterisks. (for password Editboxes)

// Text Alignment Flags
static const int TF_ALIGN_LEFT	= 0x00;	// Text aligned to the left (default)
static const int TF_ALIGN_CENTER= 0x08;	// Text centered
static const int TF_ALIGN_RIGHT = 0x10;	// Text aligned right



/*! \struct BufferedImage
	\brief image data
	\details couples the raw image data with its size in a way that is
			independent of the used renderer.
*/
struct BufferedImage : public ObjectCounter<BufferedImage>
{
	int w;
	int h;
	int sourceWidth = 0, sourceHeight = 0;
	union
	{
		SDL_Texture* sdlImage;
		unsigned glHandle;
	};
};


/*! \class RenderManager
	\brief class for managing rendering
	\details
	This rendering class reduces all drawing stuff to a few calls
	to refresh the objects states. It also abstracts from specific
	graphics APIs.
	The following implementations are planned (ordered by importance)

	RenderManagerSDL:
	 Uses standard SDL blits for drawing. It depends on precomputed
	 rotated sprites and colors the blobs manually.
	 Its fixed to the traditional resolution 800x600.
	RenderManagerGL2D:
	 This manager relies on OpenGL to accelerate 2D drawing on systems
	 like Linux/X11 where SDL acceleration is difficult. It rotates and
	 colors its sprites in realtime, but still uses 2D graphics.
	RenderManagerGL3D:
     The GL3D is the top-end RenderManager. It uses newly created meshes
     and therefore supports vertex morphing for the blobs. It makes use
     of OpenGL to present special effects like per-pixel-lighting,
     stencil shadows, motion blur, and much more. It will requiere
     OpenGL 2.0 compliant graphics hardware.

	\todo This classes need a complete rework! They include far too much information about the actual game.
*/
class RenderManager : public ObjectCounter<RenderManager>
{
	public:
		virtual ~RenderManager();

		static std::unique_ptr<RenderManager> createRenderManagerSDL();
		static std::unique_ptr<RenderManager> createRenderManagerGL2D();
		static std::unique_ptr<RenderManager> createRenderManagerNull();

		Color getOscillationColor() const;
		BloodManager& getBlood();

		// This swaps the screen buffers and should be called
		// after all draw calls
		virtual void refresh() {};
		virtual void setCourtWidth(float width) { if(width!=mCourtWidth)resetSceneBlur();mCourtWidth=width; }
		float courtWidth() const { return mCourtWidth; }
		virtual void drawSceneBlur(const std::string& key) {}
		void resetSceneBlur() { mSceneBlurKey.clear(); }
		virtual bool setVSync(bool enabled) { return false; }

		// Init with the desired Resolution.
		// Note: It is not guaranteed that this resolution will be selected
		virtual void init(int xResolution, int yResolution, bool fullscreen) {};

		// Set a background image by filename
		// Note: There is a default, you don't need to do this
		// Returns true on success
		virtual bool setBackground(const std::string& filename) { return true; };

		// Colors the standard blob image, which are red and green by default
		virtual void setBlobColor(int player, Color color) {};

		virtual void showShadow(bool shadow) {};

		void setMouseMarker(float position);

		// This simply draws the given text with its top left corner at the
		// given position and doesn't care about line feeds.
		virtual void drawText(const std::string& text, Vector2 position, unsigned int flags = TF_NORMAL) {};
		void drawMenuText(const std::string& text, Vector2 position, float height, int style = 0);
		static float menuTextWidth(const std::string& text, float height);

		// This loads and draws an image by name
		// The according Surface is automatically colorkeyed
		// The image is centered around position
		virtual void drawImage(const std::string& filename, Vector2 position, Vector2 size = Vector2(0,0)) {};

		// This draws a greyed-out area
		virtual void drawOverlay(float opacity, Vector2 pos1, Vector2 pos2, Color col = Color(0,0,0)) {}

		// Celebration particles use real render time and never advance the simulation RNG.
		void drawConfetti(Uint32 started);

		virtual void drawRoundedPanel(float opacity, Vector2 pos1, Vector2 pos2, Color col) { drawOverlay(opacity,pos1,pos2,col); }

		// Draws a blob
		virtual void drawBlob(const Vector2& pos, const Color& col, float scale = 1){};

		// Draws the game
		virtual void drawGame(const DuelMatchState& gameState) = 0;

		// Enables particle drawing
		virtual void startDrawParticles() {};
		//Draw blood particle
		virtual void drawParticle(const Vector2& pos, int player){};
		// Finishes drawing particles
		virtual void endDrawParticles() {};

		// This function may be useful for displaying framerates
		void setTitle(const std::string& title);

		// Returns the window
		SDL_Window* getWindow();
	protected:
		RenderManager();
		SDL_Surface* highlightSurface(SDL_Surface* surface, int luminance);
		SDL_Surface* loadSurface(const std::string& filename);
		virtual void uploadFontAtlas(const std::string& name,SDL_Surface* surface) { SDL_FreeSurface(surface); }
		virtual void releaseFontAtlas(const std::string& name) {}
		virtual void drawFontGlyph(const std::string& name,const SDL_Rect& source,Vector2 position,Vector2 size,Color color) {}
		void drawRuntimeText(const std::string& text,Vector2 position,float height,int style,float fixedAdvance=0,bool obfuscate=false);
		std::map<int,RuntimeFontAtlas> mRuntimeFonts;
		int assetScale(const std::string& filename) const
		{
			auto it = mAssetScales.find(filename);
			return it == mAssetScales.end() ? 1 : it->second;
		}
		SDL_Surface* createEmptySurface(unsigned int width, unsigned int height);

		float mCourtWidth = 800.f;
		SDL_Window* mWindow;
		std::string mSceneBlurKey;

		Vector2 blobShadowPosition(const Vector2& position);
		Vector2 ballShadowPosition(const Vector2& position);

		SDL_Rect blobRect(const Vector2& position);
		SDL_Rect blobShadowRect(const Vector2& position);
		SDL_Rect ballRect(const Vector2& position);
		SDL_Rect ballShadowRect(const Vector2& position);

		std::map<std::string, BufferedImage*> mImageMap;
		std::map<std::string, int> mAssetScales;

		float mMouseMarkerPosition;

	private:
		std::unique_ptr<BloodManager> mBloodMgr;
};
