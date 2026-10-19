#pragma once
#include <string>
#include <cstdint>
#include <vector>

struct LevelDefinition
{
    std::string id, name, directory, background;
    bool custom;
};

std::vector<LevelDefinition> availableLevels();
LevelDefinition resolveLevel(const std::string& requested);
std::vector<std::string> levelSoundCandidates(const std::string& level, const std::string& role);

std::vector<std::string> levelBallCandidates(const std::string& level);

// Network sessions only use the shipped catalog; custom folders never participate.
std::vector<LevelDefinition> builtInLevels();
uint32_t builtInLevelChecksum();
