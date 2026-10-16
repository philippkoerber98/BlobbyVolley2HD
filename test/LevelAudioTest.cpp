#include "BackgroundSettings.h"
#include "FileRead.h"
#include "FileSystem.h"
#include "FileWrite.h"
#include "LevelCatalog.h"
#include "RenderManager.h"
#include "SoundManager.h"
#include <SDL_image.h>
#include <algorithm>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>

namespace
{
class LevelImageProbe : public RenderManager
{
  public:
    using RenderManager::loadSurface;
    void drawGame(const DuelMatchState&) override {}
};

void requireAudio(bool value, const char* message)
{
    if (!value)
    {
        throw std::runtime_error(message);
    }
}
} // namespace

void testLevelAudio(const std::string& outputDirectory)
{
    auto& fs = FileSystem::getSingleton();
    const auto originalCount = availableLevels().size();
    fs.setWriteDir(outputDirectory);
    const std::string good = "Custom Levels/Audio Test";
    const std::string bad = "Custom Levels/Broken Audio Test";
    fs.mkdir("Custom Levels");
    fs.mkdir(good);
    fs.mkdir(bad);
    fs.mkdir("Custom Levels/No Background");
    fs.mkdir("sounds");
    auto* image = SDL_CreateRGBSurfaceWithFormat(0, 32, 18, 32, SDL_PIXELFORMAT_RGBA32);
    SDL_FillRect(image, nullptr, SDL_MapRGBA(image->format, 90, 140, 210, 255));
    requireAudio(IMG_SavePNG(image, (outputDirectory + "/" + good + "/BACKGROUND.PNG").c_str()) == 0,
                 "Cannot create custom level fixture");
    requireAudio(IMG_SavePNG(image, (outputDirectory + "/" + bad + "/background.png").c_str()) == 0,
                 "Cannot create fallback level fixture");
    SDL_FreeSurface(image);
    std::ifstream fixture(std::string(BLOBBY_TEST_SOURCE_DIR) + "/fixtures/tone.ogg", std::ios::binary);
    const std::vector<char> tone((std::istreambuf_iterator<char>(fixture)), std::istreambuf_iterator<char>());
    requireAudio(tone.size() > 100, "Missing Ogg Vorbis fixture");
    std::vector<std::string> created{good + "/BACKGROUND.PNG", bad + "/background.png"};
    auto write = [&](const std::string& name, const std::vector<char>& bytes)
    {
        FileWrite file(name);
        file.write(bytes.data(), bytes.size());
        created.push_back(name);
    };
    write(good + "/MUSIC.OGG", tone);
    write(good + "/pfiff.ogg", tone);
    write("sounds/menu.ogg", tone);
    write("sounds/music.ogg", tone);
    write("sounds/victory.ogg", tone);
    write(bad + "/music.ogg", {'b', 'a', 'd'});
    write(bad + "/bums.ogg", {'b', 'a', 'd'});
    requireAudio(availableLevels().size() == originalCount + 2,
                 "Custom levels need one background, no config");
    requireAudio(resolveLevel("custom:Audio Test").background == good + "/BACKGROUND.PNG",
                 "Custom level filenames should be case-insensitive");
    requireAudio(resolveLevel("custom:../../invalid").id == "strand1.bmp",
                 "Invalid level must recover safely");
    requireAudio(resolveLevel("strand2.webp").id == "strand2.bmp", "Saved built-in identifiers must migrate");
    {
        LevelImageProbe renderer;
        auto* background = renderer.loadSurface("backgrounds/custom:Audio Test");
        requireAudio(background->w == 32 && background->h == 18, "Custom image dimensions were changed");
        SDL_FreeSurface(background);
        write(bad + "/background.png", {'b', 'a', 'd'});
        background = renderer.loadSurface("backgrounds/custom:Broken Audio Test");
        requireAudio(background->w > 32 && background->h > 18,
                     "Broken image did not use the default background");
        SDL_FreeSurface(background);
    }
    {
        SoundManager audio;
        requireAudio(audio.ready(), "Dummy audio device or Vorbis support unavailable");
        audio.selectLevel("custom:Audio Test");
        audio.setScene(SoundManager::Scene::Menu);
        requireAudio(audio.musicPath() == "sounds/menu.ogg" && audio.musicPlaying(), "Menu Ogg not playing");
        const auto menuStarts = audio.musicStartCount();
        audio.setScene(SoundManager::Scene::Menu); // Main menu -> options -> main menu.
        audio.selectLevel("strand2.bmp");
        requireAudio(audio.musicStartCount() == menuStarts, "Menu/background preview restarted music");
        audio.selectLevel("custom:Audio Test");
        audio.setScene(SoundManager::Scene::Match, true);
        SDL_Delay(230);
        audio.update();
        requireAudio(audio.musicPath() == good + "/MUSIC.OGG",
                     "Match did not use the selected level's music");
        SDL_Delay(240); // Longer than the source; looping must keep playing.
        requireAudio(audio.musicPlaying(), "Level music did not loop");
        audio.setVolume(.8f);
        audio.setMusicVolume(.5f);
        audio.setEffectsVolume(0);
        requireAudio(Mix_VolumeMusic(-1) == int(MIX_MAX_VOLUME * .8f * .5f), "Music bus volume is wrong");
        requireAudio(audio.playSound(SoundManager::WHISTLE, 1) &&
                         audio.lastEffectPath() == good + "/pfiff.ogg",
                     "OGG effect override did not decode");
        requireAudio(Mix_Volume(0, -1) == 0, "Muted effects channel is audible");
        audio.setEffectsVolume(.25f);
        requireAudio(Mix_Volume(0, -1) == int(MIX_MAX_VOLUME * .8f * .25f),
                     "Live effects volume did not apply");
        const auto matchStarts = audio.musicStartCount();
        requireAudio(audio.playVictory() && !audio.playVictory() && audio.victoryPlayCount() == 1,
                     "Victory must play once per match, not once per frame");
        requireAudio(audio.musicPlaying() && audio.musicStartCount() == matchStarts,
                     "Victory interrupted the level soundtrack");
        audio.setMute(true);
        requireAudio(Mix_VolumeMusic(-1) == 0 && audio.musicPlaying(),
                     "Mute should preserve the music timeline");
        audio.setMute(false);
        requireAudio(audio.musicStartCount() == matchStarts, "Unmute restarted music");
        audio.selectLevel("custom:Broken Audio Test");
        SDL_Delay(230);
        audio.update();
        requireAudio(audio.musicPath() == "sounds/music.ogg",
                     "Broken level music did not use standard fallback");
        requireAudio(audio.playSound(SoundManager::IMPACT, 1) && audio.lastEffectPath() == "sounds/bums.wav",
                     "Broken Ogg effect did not fall back to built-in WAV");
        audio.setScene(SoundManager::Scene::Match, true);
        requireAudio(audio.playVictory() && audio.victoryPlayCount() == 2,
                     "Rematch did not reset victory gating");
    }
    for (auto it = created.rbegin(); it != created.rend(); ++it)
    {
        fs.deleteFile(*it);
    }
    fs.deleteFile(good);
    fs.deleteFile(bad);
    fs.deleteFile("Custom Levels/No Background");
    fs.deleteFile("Custom Levels");
    fs.deleteFile("sounds");
    fs.removeFromSearchPath(outputDirectory);
    requireAudio(availableLevels().size() == originalCount, "Temporary levels leaked into the catalog");
    std::cout << "Levels/audio: discovery, saved IDs, Ogg/WAV fallback, looping, live buses, mute and "
                 "once-only victory passed\n";
}
