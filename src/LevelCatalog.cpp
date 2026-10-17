#include "LevelCatalog.h"
#include "FileSystem.h"
#include <algorithm>
#include <cctype>
#include <stdexcept>

namespace
{
std::string lower(std::string value)
{
    for (auto& c : value)
    {
        c = char(std::tolower(static_cast<unsigned char>(c)));
    }
    return value;
}

std::string matchingFile(const std::string& directory, const std::string& filename)
{
    auto& fs = FileSystem::getSingleton();
    for (const auto& entry : fs.enumerateFiles(directory, "", true))
    {
        const auto path = directory + "/" + entry;
        if (lower(entry) == filename && !fs.isDirectory(path))
        {
            return path;
        }
    }
    return {};
}
} // namespace

std::vector<LevelDefinition> availableLevels()
{
    auto& fs = FileSystem::getSingleton();
    std::vector<LevelDefinition> result;
    for (bool custom : {false, true})
    {
        const std::string root = custom ? "Custom Levels" : "Levels";
        for (const auto& folder : fs.enumerateFiles(root, "", true))
        {
            const auto directory = root + "/" + folder;
            if (!fs.isDirectory(directory))
            {
                continue;
            }
            auto image = matchingFile(directory, "background.webp");
            if (image.empty())
            {
                image = matchingFile(directory, "background.png");
            }
            if (!image.empty())
            {
                result.push_back(
                    {custom ? "custom:" + folder : folder + ".bmp", folder, directory, image, custom});
            }
        }
    }
    std::sort(result.begin(), result.end(), [](const LevelDefinition& a, const LevelDefinition& b)
              { return a.custom != b.custom ? !a.custom : a.name < b.name; });
    return result;
}

LevelDefinition resolveLevel(const std::string& requested)
{
    std::string id = requested;
    const bool custom = id.compare(0, 7, "custom:") == 0;
    if (!custom)
    {
        const auto slash = id.find_last_of("/\\");
        if (slash != std::string::npos)
        {
            id = id.substr(slash + 1);
        }
        id = id.substr(0, id.find_last_of('.')) + ".bmp";
    }
    const auto name = custom ? id.substr(7) : id.substr(0, id.size() - 4);
    // Resolve just the selected folder on hot audio paths, never scan every image.
    if (!name.empty() && name != "." && name != ".." && name.find_first_of("/\\") == std::string::npos)
    {
        const auto directory = std::string(custom ? "Custom Levels/" : "Levels/") + name;
        auto image = matchingFile(directory, "background.webp");
        if (image.empty())
        {
            image = matchingFile(directory, "background.png");
        }
        if (!image.empty())
        {
            return {id, name, directory, image, custom};
        }
    }
    if (id != "strand1.bmp")
    {
        return resolveLevel("strand1.bmp");
    }
    const auto levels = availableLevels();
    if (levels.empty())
    {
        throw std::runtime_error("No level backgrounds found in Levels or Custom Levels");
    }
    return levels.front();
}

std::vector<std::string> levelSoundCandidates(const std::string& level, const std::string& role)
{
    std::vector<std::string> result;
    auto add = [&](const std::string& directory)
    {
        for (const char* extension : {".ogg", ".wav"})
        {
            auto path = matchingFile(directory, role + extension);
            if (!path.empty())
            {
                result.push_back(std::move(path));
            }
        }
    };
    if (!level.empty() && role != "menu")
    {
        add(resolveLevel(level).directory);
    }
    // Level music is opt-in. Only effects and menu music have global defaults.
    if (role != "level")
    {
        add("sounds");
    }
    return result;
}

std::vector<std::string> levelBallCandidates(const std::string& level)
{
    std::vector<std::string> result;
    const auto directory = resolveLevel(level).directory;
    for (const char* name : {"ball.webp", "ball.png"})
    {
        auto path = matchingFile(directory, name);
        if (!path.empty())
        {
            result.push_back(std::move(path));
        }
    }
    return result;
}
