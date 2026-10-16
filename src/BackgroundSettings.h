#pragma once
#include "LevelCatalog.h"
#include <string>
#include <vector>

inline std::vector<std::string> availableBackgroundNames()
{
    std::vector<std::string> names;
    for (const auto& level : availableLevels())
    {
        names.push_back(level.id);
    }
    return names;
}

// Preserve saved built-in IDs while discovering complete level folders.
inline std::string resolveBackgroundName(const std::string& requested)
{
    return resolveLevel(requested).id;
}
