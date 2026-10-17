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

/**
 * @file SoundManager.h
 * @brief Contains a class which loads and plays sound
 */

#pragma once
#include "BlobbyDebug.h"
#include <SDL.h>
#include <SDL_mixer.h>
#include <array>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

class SoundManager : public ObjectCounter<SoundManager>
{
  public:
    enum class Scene
    {
        Menu,
        Match
    };
    SoundManager();
    ~SoundManager();
    SoundManager(const SoundManager&) = delete;
    SoundManager& operator=(const SoundManager&) = delete;

    bool playSound(const std::string& filename, float volume);
    bool playVictory();
    void selectLevel(const std::string& level);
    void setScene(Scene scene, bool newMatch = false);
    void update();
    void setVolume(float volume);
    void setEffectsVolume(float volume);
    void setMusicVolume(float volume);
    void setMute(bool mute);

    bool ready() const
    {
        return mReady;
    }
    const std::string& musicPath() const
    {
        return mMusicPath;
    }
    bool musicPlaying() const;
    float effectsVolume() const
    {
        return mEffectsVolume;
    }
    float musicVolume() const
    {
        return mMusicVolume;
    }
    unsigned victoryPlayCount() const
    {
        return mVictoryPlayCount;
    }
    unsigned musicStartCount() const
    {
        return mMusicStartCount;
    }
    const std::string& lastEffectPath() const
    {
        return mLastEffectPath;
    }

    static constexpr const char* IMPACT = "sounds/bums.wav";
    static constexpr const char* WHISTLE = "sounds/pfiff.wav";
    static constexpr const char* CHAT = "sounds/chat.wav";

  private:
    using Chunk = std::unique_ptr<Mix_Chunk, decltype(&Mix_FreeChunk)>;
    using Music = std::unique_ptr<Mix_Music, decltype(&Mix_FreeMusic)>;
    static constexpr int EffectChannels = 32;
    std::map<std::string, Chunk> mChunks;
    std::set<std::string> mFailedSounds;
    // Keep compressed bytes alive for the streaming decoder and its SDL_RWops.
    std::vector<char> mMusicBytes;
    Music mMusic{nullptr, Mix_FreeMusic};
    std::vector<std::string> mRequestedMusic;
    std::array<float, EffectChannels> mEffectGains{};
    std::string mLevel, mMusicPath, mLastEffectPath;
    Scene mScene = Scene::Menu;
    float mVolume = 1, mEffectsVolume = 1, mMusicVolume = 1;
    bool mReady = false, mOwnAudio = false, mMute = false;
    bool mMusicPending = false, mVictoryPlayed = false;
    Uint32 mSwitchMusicAt = 0;
    unsigned mVictoryPlayCount = 0;
    unsigned mMusicStartCount = 0;

    bool playCandidates(const std::vector<std::string>& candidates, float volume);
    void requestMusic(std::vector<std::string> candidates);
    void startRequestedMusic();
    void applyVolumes();
};
