#include "BackgroundSettings.h"
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
#include "RenderManager.h"

/* includes */
#include "FileRead.h"
#include "FileSystem.h"
#include "AssetTransforms.h"
#include <SDL_image.h>
#include <algorithm>
#include <cstdlib>
#include <sstream>
#include "DisplayViewport.h"

float RenderManager::menuTextWidth(const std::string& text,float height)
{
    float width=0;for(size_t i=0;i<text.size();)width+=nunitoAdvance(nextFontCode(text,i))*height;return width;
}
void RenderManager::drawMenuText(const std::string& text,Vector2 position,float height,int style)
{ drawRuntimeText(text,position,height,style); }
void RenderManager::drawRuntimeText(const std::string& text,Vector2 position,float height,int style,float fixedAdvance,bool obfuscate)
{
    if(height<=0||text.empty())return;
    int w=800,h=600;
    if(mWindow){if(SDL_GetWindowFlags(mWindow)&SDL_WINDOW_OPENGL)SDL_GL_GetDrawableSize(mWindow,&w,&h);
        else if(auto* renderer=SDL_GetRenderer(mWindow))SDL_GetRendererOutputSize(renderer,&w,&h);
        else SDL_GetWindowSize(mWindow,&w,&h);}
    const auto viewport=fitGameViewport(w,h,mCourtWidth);
    // Bucket physical sizes in four-pixel increments. Atlases are rebuilt from
    // outlines at the active monitor density, never enlarged from PNG glyphs.
    const int pixels=(std::max)(12,(std::min)(256,4*int(std::ceil(height*viewport.h/600.f/4))));
    auto found=mRuntimeFonts.find(pixels);
    const std::string name="__nunito_"+std::to_string(pixels);
    if(found==mRuntimeFonts.end()){
        // Bound GPU memory while freely resizing or moving between monitors.
        if(mRuntimeFonts.size()>=16){auto oldest=mRuntimeFonts.begin();
            releaseFontAtlas("__nunito_"+std::to_string(oldest->first));mRuntimeFonts.erase(oldest);}
        RuntimeFontAtlas atlas;auto* surface=createNunitoAtlas(pixels,atlas);
        uploadFontAtlas(name,surface);found=mRuntimeFonts.emplace(pixels,std::move(atlas)).first;
    }
    const float scale=height/pixels;
    const Color color=style==1?Color(53,36,20):style==2?Color(255,207,91):Color(255,245,220);
    for(size_t i=0;i<text.size();){unsigned c=nextFontCode(text,i);if(obfuscate)c='*';if(c<32||c>=384)c='?';
        const auto& g=found->second.glyphs[c];
        float offset=fixedAdvance?(fixedAdvance-g.advance*scale)/2:0;
        if(g.source.w&&g.source.h)drawFontGlyph(name,g.source,position+Vector2(offset+g.x*scale,g.y*scale),
            Vector2(g.source.w*scale,g.source.h*scale),color);
        position.x+=fixedAdvance?fixedAdvance:g.advance*scale;
    }
}
#include "Blood.h"
#include "IUserConfigReader.h"

/* implementation */

RenderManager::~RenderManager() = default;

RenderManager::RenderManager() :
	mBloodMgr(new BloodManager(IUserConfigReader::createUserConfigReader("config.xml")->getBool("blood")))
{
	mMouseMarkerPosition = -100.0;
}

BloodManager& RenderManager::getBlood() {
	return *mBloodMgr;
}

SDL_Surface* RenderManager::highlightSurface(SDL_Surface* surface, int luminance)
{
    auto* result = SDL_ConvertSurfaceFormat(surface,SDL_PIXELFORMAT_RGBA32,0);
    if (!result) return nullptr;
    bool colorKey = SDL_HasColorKey(surface);
    SDL_SetColorKey(result,SDL_FALSE,0);
    for(int y=0;y<result->h;++y) for(int x=0;x<result->w;++x)
    {
        auto* pixel=reinterpret_cast<Uint32*>(static_cast<Uint8*>(result->pixels)+y*result->pitch)+x;
        Uint8 r,g,b,a; SDL_GetRGBA(*pixel,result->format,&r,&g,&b,&a);
        if(colorKey && !(r|g|b)) a=0;
        if(a) { r=Uint8((std::min)(255,int(r)+luminance)); g=Uint8((std::min)(255,int(g)+luminance)); b=Uint8((std::min)(255,int(b)+luminance)); }
        *pixel=SDL_MapRGBA(result->format,r,g,b,a);
    }
    SDL_SetSurfaceBlendMode(result,SDL_BLENDMODE_BLEND);
    return result;
}

SDL_Surface* RenderManager::loadSurface(const std::string& filename)
{
	const std::string menuPrefix="menu-backgrounds/";
	if(filename.compare(0,menuPrefix.size(),menuPrefix)==0)
	{
		auto* source=loadSurface("backgrounds/"+resolveBackgroundName(filename.substr(menuPrefix.size())));
		auto* blurred=createMenuBackdrop(source,true); SDL_FreeSurface(source);
		if(!blurred) BOOST_THROW_EXCEPTION(FileLoadException(filename));
		return blurred;
	}
	auto readBitmap = [](const std::string& path) -> SDL_Surface*
	{
		FileRead file(preferredHDImage(path));
		int fileLength = file.length();
		auto content = file.readRawBytes(fileLength);
		return IMG_Load_RW(SDL_RWFromMem(content.data(), fileLength), 1);
	};

	if (filename.size() == 14 && filename.compare(0, 8, "gfx/ball") == 0)
	{
		int frame = std::atoi(filename.substr(8,2).c_str());
		if (FileSystem::getSingleton().exists(preferredHDImage("gfx/hd/volleyball.png")))
		{
			auto* source = readBitmap("gfx/hd/volleyball.png");
			auto* result = rotateVolleyball(source, frame);
			SDL_FreeSurface(source);
			if (result) { mAssetScales[filename] = 4; return result; }
		}
	}
	// Fonts are deliberately excluded. Original BMPs remain the fallback.
	if (filename.size() > 4 && filename.substr(filename.size()-4) == ".bmp" &&
		filename.find("/font") == std::string::npos)
	{
		auto slash = filename.find_last_of('/');
		std::string directory = slash == std::string::npos ? "gfx/" : filename.substr(0, slash+1);
		std::string base = slash == std::string::npos ? filename : filename.substr(slash+1);
		std::string path = preferredHDImage(directory + "hd/" + base.substr(0, base.size()-4) + ".png");
		if (FileSystem::getSingleton().exists(path))
		{
			int logicalWidth=0,logicalHeight=0;
			if(filename.compare(0,12,"backgrounds/")==0){logicalWidth=800;logicalHeight=600;}
			if(FileSystem::getSingleton().exists("gfx/hd/metrics.txt"))
			{
				FileRead metrics("gfx/hd/metrics.txt");auto bytes=metrics.readRawBytes(metrics.length());
				std::istringstream rows(std::string(bytes.begin(),bytes.end()));
				std::string name;int w,h;
				while(rows>>name>>w>>h)if(name==filename){logicalWidth=w;logicalHeight=h;break;}
			}
			auto* original = logicalWidth==0 && FileSystem::getSingleton().exists(filename) ? readBitmap(filename) : nullptr;
			if(original){logicalWidth=original->w;logicalHeight=original->h;}
			auto* png = readBitmap(path);
			// Backgrounds retain their native widescreen dimensions. Both renderers
			// choose a centered camera crop when drawing, including after mode changes.
			if(filename.compare(0,12,"backgrounds/")==0){SDL_FreeSurface(original);mAssetScales[filename]=1;return png;}
			if(filename.find("blobbym")!=std::string::npos || base=="titel.bmp" || base=="Icon.bmp" || base=="cursor.bmp" || base=="pokal.bmp")
				cleanSpriteEdges(png);
			auto* result = logicalWidth>0 ? scaleAsset(png, logicalWidth*4, logicalHeight*4) : nullptr;
			SDL_FreeSurface(original); SDL_FreeSurface(png);
			if (result) { mAssetScales[filename] = 4; return result; }
		}
	}

	SDL_Surface* newSurface = readBitmap(filename);

	if (!newSurface)
		BOOST_THROW_EXCEPTION ( FileLoadException(filename) );
	if (filename.compare(0, 11, "gfx/blobbym") == 0 || filename.compare(0, 8, "gfx/sch1") == 0)
		SDL_SetColorKey(newSurface, SDL_TRUE, SDL_MapRGB(newSurface->format, 0, 0, 0));

	return newSurface;
}


SDL_Surface* RenderManager::createEmptySurface(unsigned int width, unsigned int height)
{
	SDL_Surface* newSurface = SDL_CreateRGBSurface(0, width, height, 32, 0x000000FF, 0x0000FF00, 0x00FF0000, 0x00000000);

	return newSurface;
}

void RenderManager::setMouseMarker(float position)
{
	mMouseMarkerPosition = position;
}

SDL_Rect RenderManager::blobRect(const Vector2& position)
{
	SDL_Rect rect = {
		(short)(lround(position.x) - 37),
		(short)(lround(position.y) - 44),
		75,
		89
	};

	return rect;
}

SDL_Rect RenderManager::ballRect(const Vector2& position)
{
	SDL_Rect rect = {
		(short)(lround(position.x) - 32),
		(short)(lround(position.y) - 32),
		64,
		64
	};
	return rect;
}

Vector2 RenderManager::ballShadowPosition(const Vector2& position)
{
	return {
		position.x + (500.f - position.y) / 4.f + 16.f,
		500.f - (500.f - position.y) / 16.f - 10.f
	};
}

SDL_Rect RenderManager::ballShadowRect(const Vector2& position)
{
	SDL_Rect rect = {
		short(lround(position.x) - 64),
		short(lround(position.y) - 16),
		69,
		17
	};

	return rect;
}

Vector2 RenderManager::blobShadowPosition(const Vector2& position)
{
	return {
		position.x + (500.f - position.y) / 4 + 16.f,
		500.f - (500.f - position.y) / 16.f - 10.f
	};
}

SDL_Rect RenderManager::blobShadowRect(const Vector2& position)
{
	SDL_Rect rect = {
		short(lround(position.x) - 64),
		short(lround(position.y) - 16),
		128,
		32
	};
	return rect;
}

void RenderManager::setTitle(const std::string& title)
{
	SDL_SetWindowTitle(mWindow, title.c_str());
}

SDL_Window* RenderManager::getWindow()
{
	return mWindow;
}

Color RenderManager::getOscillationColor() const
{
	float time = float(SDL_GetTicks()) / 1000.f;

	return {
		int((std::sin(time*1.5) + 1.0) * 128),
		int((std::sin(time*2.5) + 1.0) * 128),
		int((std::sin(time*3.5) + 1.0) * 128)
	};
}

void RenderManager::drawConfetti(Uint32 started)
{
 const float time=static_cast<Uint32>(SDL_GetTicks()-started)/1000.f;
 const Color colors[]={Color(255,207,91),Color(105,185,255),Color(255,164,82),Color(255,245,220)};
 for(unsigned i=0;i<100;++i){
  // Stable per-particle values, independent of game/network random state.
  const unsigned seed=i*2654435761u+2246822519u;
  const float phase=(seed%6283)/1000.f;
  const float x=12+(seed%int(mCourtWidth-24))+14*std::sin(time*.8f+phase);
  const float y=std::fmod((seed/997)%640+time*(42+(seed%53)),640.f)-20;
  // Keep winner, score and all controls unobstructed.
  if(x>137+(mCourtWidth-800)/2&&x<663+(mCourtWidth-800)/2&&y>110&&y<582)continue;
  if(x<1||x>mCourtWidth-7||y<0||y>596)continue;
  const float w=1.5f+3*std::abs(std::cos(time*2+phase));
  const float h=3+3*std::abs(std::sin(time*1.3f+phase));
  drawOverlay(.88f,Vector2(x,y),Vector2(x+w,y+h),colors[i%4]);
 }
}
