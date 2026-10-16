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
#include "ImagePaths.h"
#include "FileRead.h"
#include "FileSystem.h"
#include <SDL_image.h>
#include <cstdlib>
#include <sstream>
#include <map>

namespace
{
SDL_Point logicalSpriteSize(const std::string& filename)
{
    // Logical dimensions are metadata, not a reason to retain obsolete BMPs.
    // Read this immutable catalog once rather than reopening it for every sprite.
    static const auto sizes = []()
    {
        FileRead metrics("gfx/metrics.txt");
        const auto bytes = metrics.readRawBytes(metrics.length());
        std::istringstream rows(std::string(bytes.begin(), bytes.end()));
        std::map<std::string, SDL_Point> result;
        std::string name;
        int width, height;
        while (rows >> name >> width >> height)
        {
            result[name] = SDL_Point{width, height};
        }
        return result;
    }();
    const auto found = sizes.find(filename);
    return found == sizes.end() ? SDL_Point{0, 0} : found->second;
}
} // namespace

void RenderManager::drawChampion(const Vector2& center, const Color& color, float height)
{
    if (height <= 0)
    {
        return;
    }
    if (!mChampionSource)
    {
        mChampionSource.reset(loadSurface("gfx/champion.webp"));
    }
    const std::string texture = "__champion";
    if (!mChampionTextureReady || color != mChampionColor)
    {
        auto* tinted = colorizeChampion(mChampionSource.get(), {color.r, color.g, color.b, 255});
        if (!tinted)
        {
            throw std::runtime_error(SDL_GetError());
        }
        releaseTexture(texture);
        mChampionTextureReady = false;
        uploadTexture(texture, tinted);
        mChampionColor = color;
        mChampionTextureReady = true;
    }
    const Vector2 size(height * mChampionSource->w / mChampionSource->h, height);
    const SDL_Rect source{0, 0, mChampionSource->w, mChampionSource->h};
    drawTextureRegion(texture, source, center - size * .5f, size, Color(255, 255, 255));
}

SDL_Surface* RenderManager::loadSurface(const std::string& filename)
{
    const std::string menuPrefix = "menu-backgrounds/";
    if (filename.compare(0, menuPrefix.size(), menuPrefix) == 0)
    {
        auto* source =
            loadSurface("backgrounds/" + resolveBackgroundName(filename.substr(menuPrefix.size())));
        auto* blurred = createMenuBackdrop(source, true);
        SDL_FreeSurface(source);
        if (!blurred)
        {
            BOOST_THROW_EXCEPTION(FileLoadException(filename));
        }
        return blurred;
    }
    auto readImage = [](const std::string& path) -> SDL_Surface*
    {
        FileRead file(preferredImage(path));
        int fileLength = file.length();
        auto content = file.readRawBytes(fileLength);
        auto* image = IMG_Load_RW(SDL_RWFromMem(content.data(), fileLength), 1);
        if (!image)
        {
            BOOST_THROW_EXCEPTION(FileLoadException(path));
        }
        return image;
    };

    if (filename.compare(0, 12, "backgrounds/") == 0)
    {
        const auto level = resolveLevel(filename.substr(12));
        try
        {
            return readImage(level.background);
        }
        catch (const std::exception&)
        {
            const auto fallback = resolveLevel("strand1.bmp");
            if (fallback.id == level.id)
            {
                throw;
            }
            return readImage(fallback.background);
        }
    }

    if (filename.size() == 14 && filename.compare(0, 8, "gfx/ball") == 0)
    {
        int frame = std::atoi(filename.substr(8, 2).c_str());
        if (FileSystem::getSingleton().exists(preferredImage("gfx/volleyball.png")))
        {
            auto* source = readImage("gfx/volleyball.png");
            auto* result = rotateVolleyball(source, frame);
            SDL_FreeSurface(source);
            if (result)
            {
                mAssetScales[filename] = 4;
                return result;
            }
        }
    }
    // Historical BMP names are logical identifiers mapped exclusively to HD art.
    if (filename.size() > 4 && filename.substr(filename.size() - 4) == ".bmp")
    {
        auto slash = filename.find_last_of('/');
        std::string directory = slash == std::string::npos ? "gfx/" : filename.substr(0, slash + 1);
        std::string base = slash == std::string::npos ? filename : filename.substr(slash + 1);
        std::string path = preferredImage(directory + base.substr(0, base.size() - 4) + ".png");
        if (FileSystem::getSingleton().exists(path))
        {
            auto* image = readImage(path);
            const auto logicalSize = logicalSpriteSize(filename);
            if (logicalSize.x <= 0 || logicalSize.y <= 0)
            {
                SDL_FreeSurface(image);
                BOOST_THROW_EXCEPTION(FileLoadException("Missing HD sprite dimensions: " + filename));
            }
            if (filename.find("blobbym") != std::string::npos || base == "titel.bmp" || base == "Icon.bmp" ||
                base == "cursor.bmp" || base == "pokal.bmp")
            {
                cleanSpriteEdges(image);
            }
            auto* result = scaleAsset(image, logicalSize.x * 4, logicalSize.y * 4);
            SDL_FreeSurface(image);
            if (result)
            {
                mAssetScales[filename] = 4;
                return result;
            }
        }
        BOOST_THROW_EXCEPTION(FileLoadException(filename));
    }
    return readImage(filename);
}
