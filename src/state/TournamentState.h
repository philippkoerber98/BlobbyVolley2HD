#pragma once
#include "State.h"
#include "GameState.h"
#include "NetworkMessage.h"
#include "server/Tournament.h"
#include "TournamentProtocol.h"
#include <memory>
class RakClient;
struct TournamentConnection
{
    std::shared_ptr<RakClient> client;
    ServerInfo server;
    tournament::Tournament cup;
    int own = -1, activeMatch = -1;
    bool received = false, connected = true;
    std::string error;
    explicit TournamentConnection(std::shared_ptr<RakClient> client, const ServerInfo& server);
    void send(tournament::Command command, const std::string& payload = "");
    bool receive(const packet_ptr& packet);
};
class TournamentState : public State
{
  public:
    explicit TournamentState(std::shared_ptr<TournamentConnection> connection, bool hello = false);
    void init() override;
    void step_impl() override;
    const char* getStateName() const override
    {
        return "TournamentState";
    }

  private:
    std::shared_ptr<TournamentConnection> mConnection;
    bool mHello;
    unsigned mTab = 0, mSelected = 0, mGroup = 0, mRound = 0, mPage = 0;
    std::string mPassword, mName;
    unsigned mPasswordCursor = 0, mNameCursor = 0;
    bool mClaim = false, mLeave = false;
    void overview();
    void schedule();
    void table();
    void participants();
};
class TournamentSpectatorState : public GameState
{
  public:
    TournamentSpectatorState(std::shared_ptr<TournamentConnection> connection, int match);
    void init() override;
    void step_impl() override;
    void render(float alpha) override;
    const char* getStateName() const override
    {
        return "TournamentSpectatorState";
    }

  private:
    std::shared_ptr<TournamentConnection> mConnection;
    int mWatching;
    bool mPaused = false;
    bool mHasSnapshot = false;
    DuelMatchState mPreviousSnapshot, mNextSnapshot;
    uint32_t mSnapshotAt = 0, mSnapshotInterval = 33;
};
