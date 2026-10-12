#pragma once
#include "FileSystem.h"
#include <string>

// Prefer one compressed HD image; keep PNG as an authoring/fallback format.
inline std::string preferredHDImage(const std::string& path)
{
    const auto dot=path.find_last_of('.');
    if(dot==std::string::npos)return path;
    const auto extension=path.substr(dot);
    if(extension!=".png"&&extension!=".webp")return path;
    const auto stem=path.substr(0,dot);
    if(FileSystem::getSingleton().exists(stem+".webp"))return stem+".webp";
    return stem+".png";
}
