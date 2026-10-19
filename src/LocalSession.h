#pragma once
#include "UserConfig.h"
#include "PlayerIdentity.h"
#include "server/Tournament.h"
#include <memory>
#include <vector>

// A participant is a person/bot, never a physical controller or a court side.
struct LocalParticipant
{
    std::string name, script = "hyp014";
    int color = Color(50, 189, 234).toInt(), delay = 10, profile = -1, side = 0, input = 0;
    bool human = true, cycling = false;
    PlayerIdentity identity(PlayerSide side) const;
};
struct PlayerProfiles
{
    std::vector<LocalParticipant> players;
    int selected = 0;
    PlayerProfiles();
    void save() const;
    void publishNetwork() const;
};
struct LocalSession
{
    UserConfig settings;
    std::vector<LocalParticipant> players;
    tournament::Tournament cup;
    bool tournament = false, started = false;
    int pending = -1;
    unsigned saveRevision = 0;
    LocalSession();
    void prepare(unsigned seed);
    void beginMatch();
    void finish(PlayerSide winner, int leftScore, int rightScore);
    std::pair<int, int> nextPlayers() const;
    void save();
    static std::shared_ptr<LocalSession> resume();
};
