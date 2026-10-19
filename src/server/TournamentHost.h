#pragma once
#include "Tournament.h"
#include "TournamentProtocol.h"
#include "NetworkGame.h"
#include <map>
#include <memory>
#include <string>
class DedicatedServer;
class NetworkPlayer;
class TournamentHost
{
  public:
    TournamentHost(DedicatedServer& server, int slots, std::string password);
    void packet(PlayerID source, RakNet::BitStream& stream);
    void disconnected(PlayerID source);
    void tick();
    bool contains(PlayerID source) const;
    bool active() const
    {
        return mActivated && mCup.phase != tournament::Phase::Registration;
    }

  private:
    DedicatedServer& mServer;
    tournament::Tournament mCup;
    int mSlots;
    std::string mPassword, mSavePath;
    std::map<PlayerID, int> mConnections;
    std::map<int, PlayerID> mPlayers;
    std::map<int, std::shared_ptr<NetworkGame>> mGames;
    std::map<int, NetworkGame::View> mInterrupted;
    std::map<int, uint32_t> mReservedUntil;
    std::map<PlayerID, int> mWatching;
    std::map<PlayerID, std::pair<uint32_t, int>> mClaimAttempts;
    unsigned mSavedRevision = ~0u;
    uint32_t mLastState = 0, mLastLive = 0;
    bool mNeedsResume = false, mActivated = false;
    void send(PlayerID source, tournament::Command command, const std::string& payload);
    void state();
    void save();
    void schedule();
    bool admin(int id) const;
};
