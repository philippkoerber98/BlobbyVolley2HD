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

#include "SoundManager.h"
#include "FileRead.h"
#include "LevelCatalog.h"
#include <algorithm>
#include <climits>
#include <iostream>
#include <stdexcept>

namespace
{
float clampVolume(float value)
{
    return (std::max)(0.f, (std::min)(1.f, value));
}

std::vector<char> readAudio(const std::string& path)
{
    FileRead file(path);
    if (file.length() > INT_MAX)
    {
        throw std::runtime_error("Audio file is too large: " + path);
    }
    return file.readRawBytes(file.length());
}

std::string effectRole(const std::string& filename)
{
    const auto slash = filename.find_last_of('/');
    auto role = filename.substr(slash == std::string::npos ? 0 : slash + 1);
    return role.substr(0, role.find_last_of('.'));
}
} // namespace

SoundManager::SoundManager()
{
    mOwnAudio = !SDL_WasInit(SDL_INIT_AUDIO);
    if (mOwnAudio && SDL_InitSubSystem(SDL_INIT_AUDIO) != 0)
    {
        std::cerr << "Audio unavailable: " << SDL_GetError() << '\n';
        return;
    }
    if ((Mix_Init(MIX_INIT_OGG) & MIX_INIT_OGG) == 0)
    {
        std::cerr << "Ogg Vorbis unavailable: " << Mix_GetError() << '\n';
    }
    if (Mix_OpenAudio(48000, AUDIO_F32SYS, 2, 1024) != 0)
    {
        std::cerr << "Audio unavailable: " << Mix_GetError() << '\n';
        return;
    }
    Mix_AllocateChannels(EffectChannels);
    mReady = true;
}

SoundManager::~SoundManager()
{
    if (mReady)
    {
        Mix_HaltMusic();
        Mix_HaltChannel(-1);
        mMusic.reset();
        mChunks.clear();
        Mix_CloseAudio();
    }
    Mix_Quit();
    if (mOwnAudio)
    {
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
    }
}

void SoundManager::selectLevel(const std::string& level)
{
    const auto resolved = resolveLevel(level).id;
    if (mLevel == resolved)
    {
        return;
    }
    if (mReady)
    {
        Mix_HaltChannel(-1);
    }
    mChunks.clear();
    mFailedSounds.clear();
    mLevel = resolved;
    if (mScene == Scene::Match)
    {
        requestMusic(levelSoundCandidates(mLevel, "level"));
    }
}

void SoundManager::setScene(Scene scene, bool newMatch)
{
    if (scene != mScene || newMatch)
    {
        mVictoryPlayed = false;
        if (mReady)
        {
            Mix_HaltChannel(-1);
        }
    }
    mScene = scene;
    requestMusic(levelSoundCandidates(mScene == Scene::Match ? mLevel : "",
                                      mScene == Scene::Match ? "level" : "menu"));
}

bool SoundManager::playCandidates(const std::vector<std::string>& candidates, float volume)
{
    if (!mReady)
    {
        return false;
    }
    if (mMute)
    {
        return true;
    }
    for (const auto& path : candidates)
    {
        if (mFailedSounds.count(path))
        {
            continue;
        }
        try
        {
            auto found = mChunks.find(path);
            if (found == mChunks.end())
            {
                const auto bytes = readAudio(path);
                auto* rw = SDL_RWFromConstMem(bytes.data(), int(bytes.size()));
                auto* decoded = rw ? Mix_LoadWAV_RW(rw, 1) : nullptr;
                if (!decoded)
                {
                    throw std::runtime_error(Mix_GetError());
                }
                found = mChunks.emplace(path, Chunk(decoded, Mix_FreeChunk)).first;
            }
            // Set volume before starting a reused channel, avoiding a loud first buffer.
            for (int channel = 0; channel < EffectChannels; ++channel)
            {
                if (!Mix_Playing(channel))
                {
                    mEffectGains[channel] = clampVolume(volume);
                    Mix_Volume(channel,
                               int(MIX_MAX_VOLUME * mVolume * mEffectsVolume * mEffectGains[channel]));
                    mLastEffectPath = path;
                    return Mix_PlayChannel(channel, found->second.get(), 0) >= 0;
                }
            }
            return false;
        }
        catch (const std::exception& error)
        {
            mFailedSounds.insert(path);
            std::cerr << "Skipping audio " << path << ": " << error.what() << '\n';
        }
    }
    return false;
}

bool SoundManager::playSound(const std::string& filename, float volume)
{
    const auto role = effectRole(filename);
    if (role == "bums" || role == "chat" || role == "pfiff")
    {
        return playCandidates(levelSoundCandidates(mLevel, role), volume);
    }
    return playCandidates({filename}, volume);
}

bool SoundManager::playVictory()
{
    if (mScene != Scene::Match || mVictoryPlayed)
    {
        return false;
    }
    mVictoryPlayed = true;
    const bool played = playCandidates(levelSoundCandidates(mLevel, "victory"), 1);
    if (played)
    {
        ++mVictoryPlayCount;
    }
    return played;
}

void SoundManager::requestMusic(std::vector<std::string> candidates)
{
    if (candidates == mRequestedMusic)
    {
        return;
    }
    mRequestedMusic = std::move(candidates);
    if (!mReady)
    {
        return;
    }
    mMusicPending = true;
    if (Mix_PlayingMusic())
    {
        Mix_FadeOutMusic(180);
        mSwitchMusicAt = SDL_GetTicks() + 180;
    }
    else
    {
        mSwitchMusicAt = SDL_GetTicks();
        update();
    }
}

void SoundManager::update()
{
    if (mReady && mMusicPending && SDL_TICKS_PASSED(SDL_GetTicks(), mSwitchMusicAt))
    {
        startRequestedMusic();
    }
}

void SoundManager::startRequestedMusic()
{
    mMusicPending = false;
    Mix_HaltMusic();
    mMusic.reset();
    mMusicBytes.clear();
    mMusicPath.clear();
    for (const auto& path : mRequestedMusic)
    {
        try
        {
            mMusicBytes = readAudio(path);
            auto* rw = SDL_RWFromConstMem(mMusicBytes.data(), int(mMusicBytes.size()));
            mMusic.reset(rw ? Mix_LoadMUS_RW(rw, 1) : nullptr);
            if (!mMusic)
            {
                throw std::runtime_error(Mix_GetError());
            }
            Mix_VolumeMusic(mMute ? 0 : int(MIX_MAX_VOLUME * mVolume * mMusicVolume));
            if (Mix_FadeInMusic(mMusic.get(), -1, 180) != 0)
            {
                throw std::runtime_error(Mix_GetError());
            }
            mMusicPath = path;
            ++mMusicStartCount;
            return;
        }
        catch (const std::exception& error)
        {
            mMusic.reset();
            mMusicBytes.clear();
            std::cerr << "Skipping music " << path << ": " << error.what() << '\n';
        }
    }
}

bool SoundManager::musicPlaying() const
{
    return mReady && Mix_PlayingMusic() != 0;
}

void SoundManager::applyVolumes()
{
    if (!mReady)
    {
        return;
    }
    Mix_VolumeMusic(mMute ? 0 : int(MIX_MAX_VOLUME * mVolume * mMusicVolume));
    for (int channel = 0; channel < EffectChannels; ++channel)
    {
        Mix_Volume(channel,
                   mMute ? 0 : int(MIX_MAX_VOLUME * mVolume * mEffectsVolume * mEffectGains[channel]));
    }
}

void SoundManager::setVolume(float value)
{
    mVolume = clampVolume(value);
    applyVolumes();
}
void SoundManager::setEffectsVolume(float value)
{
    mEffectsVolume = clampVolume(value);
    applyVolumes();
}
void SoundManager::setMusicVolume(float value)
{
    mMusicVolume = clampVolume(value);
    applyVolumes();
}
void SoundManager::setMute(bool value)
{
    mMute = value;
    applyVolumes();
}
