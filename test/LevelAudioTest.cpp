#include "BackgroundSettings.h"
#include "FileRead.h"
#include "FileSystem.h"
#include "FileWrite.h"
#include "LevelCatalog.h"
#include "RenderManager.h"
#include "SoundManager.h"
#include <SDL_image.h>
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstring>
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
// Capture the mixer output, including resampling, across the actual track boundary.
// The callback writes only into preallocated storage and never touches game state.
struct LoopCapture
{
    std::vector<float> samples = std::vector<float>(48000 * 2);
    std::atomic<size_t> count{0};

    static void SDLCALL record(void* context, Uint8* stream, int bytes)
    {
        auto& capture = *static_cast<LoopCapture*>(context);
        const auto offset = capture.count.load();
        const auto amount = (std::min)(size_t(bytes) / sizeof(float), capture.samples.size() - offset);
        std::memcpy(capture.samples.data() + offset, stream, amount * sizeof(float));
        capture.count.store(offset + amount);
    }
};

void checkTrackBoundary(SoundManager& audio)
{
    const auto duration = Mix_MusicDuration(nullptr);
    requireAudio(duration > .5, "Default soundtrack is missing or too short");
    SDL_Delay(230); // Let the initial scene fade finish before measuring the loop.
    requireAudio(Mix_SetMusicPosition(duration - .25) == 0, "Cannot seek to soundtrack loop boundary");
    const auto starts = audio.musicStartCount();
    LoopCapture capture;
    Mix_SetPostMix(LoopCapture::record, &capture);
    SDL_Delay(700);
    Mix_SetPostMix(nullptr, nullptr); // Waits for any in-flight mixer callback.
    const auto count = capture.count.load();
    requireAudio(count > 48000, "Insufficient audio captured across loop boundary");
    requireAudio(audio.musicPlaying() && audio.musicStartCount() == starts,
                 "Loop restarted through the application instead of the decoder");
    requireAudio(Mix_FadingMusic() == MIX_NO_FADING, "Loop unexpectedly faded music");
    requireAudio(Mix_GetMusicPosition(nullptr) < 1., "Soundtrack did not wrap to its beginning");
    size_t silentFrames = 0;
    for (size_t i = 0; i + 1 < count; i += 2)
    {
        silentFrames = std::abs(capture.samples[i]) < 1.e-7f && std::abs(capture.samples[i + 1]) < 1.e-7f
                           ? silentFrames + 1
                           : 0;
        requireAudio(silentFrames < 48, "Mixer inserted at least 1 ms of silence around the loop");
    }
    std::cout << "Soundtrack boundary: " << audio.musicPath()
              << " passed without restart, fade or inserted silence\n";
}
} // namespace

void testLevelAudio(const std::string& outputDirectory)
{
    auto& fs = FileSystem::getSingleton();
    const auto originalCount = availableLevels().size();
    {
        SoundManager audio;
        requireAudio(audio.ready(), "Dummy audio device or Vorbis support unavailable");
        audio.setScene(SoundManager::Scene::Menu);
        checkTrackBoundary(audio);
        audio.selectLevel("strand1.bmp");
        audio.setScene(SoundManager::Scene::Match, true);
        SDL_Delay(230);
        audio.update();
        checkTrackBoundary(audio);
    }
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
    write(good + "/LEVEL.OGG", tone);
    write(good + "/pfiff.ogg", tone);
    write("sounds/menu.ogg", tone);
    write("sounds/level.ogg", tone);
    write("sounds/victory.ogg", tone);
    write(bad + "/level.ogg", {'b', 'a', 'd'});
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
        SDL_Delay(360); // Longer than the source and fade-in; menu music must loop.
        requireAudio(audio.musicPlaying(), "Menu music did not loop");
        const auto menuStarts = audio.musicStartCount();
        audio.setScene(SoundManager::Scene::Menu); // Main menu -> options -> main menu.
        audio.selectLevel("strand2.bmp");
        requireAudio(audio.musicStartCount() == menuStarts, "Menu/background preview restarted music");
        audio.selectLevel("custom:Audio Test");
        audio.setScene(SoundManager::Scene::Match, true);
        SDL_Delay(230);
        audio.update();
        requireAudio(audio.musicPath() == good + "/LEVEL.OGG",
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
        requireAudio(audio.musicPath().empty() && !audio.musicPlaying(),
                     "Broken level music must stay silent, ignoring global level music");
        audio.selectLevel("mars.bmp");
        SDL_Delay(230);
        audio.update();
        requireAudio(audio.musicPath().empty() && !audio.musicPlaying(),
                     "A level without its own music used a global fallback");
        requireAudio(audio.playSound(SoundManager::WHISTLE, 1) &&
                         audio.lastEffectPath() == "sounds/pfiff.wav",
                     "Silent level lost standard sound effects");
        audio.selectLevel("custom:Broken Audio Test");
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
