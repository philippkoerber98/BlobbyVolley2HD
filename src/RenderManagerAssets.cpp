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

#include "RenderManager.h"
#include "AssetTransforms.h"
#include "BackgroundSettings.h"
#include "FileRead.h"
#include "FileSystem.h"
#include <SDL_image.h>
#include <cstdlib>
#include <sstream>
#include <map>

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


