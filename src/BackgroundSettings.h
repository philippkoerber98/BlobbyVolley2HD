#pragma once
#include "FileSystem.h"
#include "HDImagePaths.h"
#include <set>
#include <vector>
#include <algorithm>
#include <stdexcept>
#include <string>

inline std::vector<std::string> availableBackgroundNames()
{
    auto& fs=FileSystem::getSingleton();std::set<std::string> names;
    for(const char* extension:{".webp",".png"})
        for(const auto& name:fs.enumerateFiles("backgrounds/hd",extension,true))
            names.insert(name.substr(0,name.find_last_of('.'))+".bmp");
    return {names.begin(),names.end()};
}

// Saved preferences can outlive renamed or removed background files.
inline std::string resolveBackgroundName(const std::string& requested)
{
    auto& fs=FileSystem::getSingleton();
    auto available=[&](const std::string& name){
        if(name.empty())return false;
        return fs.exists(preferredHDImage("backgrounds/hd/"+name.substr(0,name.find_last_of('.'))+".png"));
    };
    if(available(requested))return requested.substr(0,requested.find_last_of('.'))+".bmp";
    if(available("strand1.bmp"))return "strand1.bmp";
    auto names=availableBackgroundNames();
    std::sort(names.begin(),names.end());
    if(names.empty())throw std::runtime_error("No HD backgrounds found in backgrounds/hd");
    return names.front();
}
