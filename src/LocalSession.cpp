#include "LocalSession.h"
#include "FileSystem.h"
#include <algorithm>
#include <stdexcept>

namespace
{
void put(UserConfig& c, const std::string& k, const LocalParticipant& p)
{
    c.setString(k + "name", p.name);
    c.setString(k + "script", p.script);
    c.setInteger(k + "color", p.color);
    c.setInteger(k + "delay", p.delay);
    c.setInteger(k + "profile", p.profile);
    c.setInteger(k + "side", p.side);
    c.setInteger(k + "input", p.input);
    c.setBool(k + "human", p.human);
    c.setBool(k + "cycling", p.cycling);
}
LocalParticipant get(const UserConfig& c, const std::string& k)
{
    LocalParticipant p;
    p.name = c.getString(k + "name");
    p.script = c.getString(k + "script", "hyp014");
    p.color = c.getInteger(k + "color", 0x32bdea);
    p.delay = std::max(0, std::min(25, c.getInteger(k + "delay", 10)));
    p.profile = c.getInteger(k + "profile", -1);
    p.side = c.getInteger(k + "side", 0) == 1 ? 1 : 0;
    p.input = c.getInteger(k + "input", 0) == 1 ? 1 : 0;
    p.human = c.getBool(k + "human", true);
    p.cycling = c.getBool(k + "cycling");
    return p;
}
bool load(UserConfig& c, const std::string& path)
{
    try
    {
        return FileSystem::getSingleton().exists(path) && c.loadFile(path);
    }
    catch (const std::exception&)
    {
        return false;
    }
}
} // namespace
PlayerIdentity LocalParticipant::identity(PlayerSide side) const
{
    return PlayerIdentity(name, Color(color), cycling, side);
}
PlayerProfiles::PlayerProfiles()
{
    UserConfig c;
    if (load(c, "profiles.xml"))
    {
        int count = std::max(0, std::min(64, c.getInteger("count")));
        for (int i = 0; i < count; ++i)
        {
            players.push_back(get(c, std::to_string(i) + "_"));
        }
        selected = std::max(0, std::min(count - 1, c.getInteger("selected")));
    }
    if (players.empty())
    {
        c.loadFile("config.xml");
        for (auto side : {LEFT_PLAYER, RIGHT_PLAYER})
        {
            const auto id = c.loadPlayerIdentity(side, true);
            LocalParticipant p;
            p.name = id.getName();
            p.color = id.getStaticColor().toInt();
            p.cycling = id.getOscillating();
            p.side = c.getInteger("network_side") == 1 ? 1 : 0;
            p.profile = int(players.size());
            players.push_back(p);
        }
        save();
    }
}
void PlayerProfiles::save() const
{
    UserConfig c;
    c.setInteger("count", int(players.size()));
    c.setInteger("selected", selected);
    for (unsigned i = 0; i < players.size(); ++i)
    {
        put(c, std::to_string(i) + "_", players[i]);
    }
    if (!c.saveFile("profiles.xml"))
    {
        throw std::runtime_error("Could not save profiles.");
    }
}
void PlayerProfiles::publishNetwork() const
{
    UserConfig c;
    c.loadFile("config.xml");
    const auto& p = players.at(selected);
    const auto col = Color(p.color);
    c.setInteger("network_side", p.side);
    c.setInteger("network_input", p.input);
    for (const char* side : {"left_", "right_"})
    {
        const std::string k(side);
        c.setString(k + "player_name", p.name);
        c.setInteger(k + "blobby_color_r", col.r);
        c.setInteger(k + "blobby_color_g", col.g);
        c.setInteger(k + "blobby_color_b", col.b);
        c.setBool(k + "blobby_oscillate", p.cycling);
    }
    if (!c.saveFile("config.xml"))
    {
        throw std::runtime_error("Could not save settings.");
    }
}
LocalSession::LocalSession()
{
    settings.loadFile("config.xml");
    UserConfig preset;
    if (load(preset, "local-setup.xml"))
    {
        for (const char* k : {"rules", "scoretowin", "gamefps", "court_widescreen", "background"})
        {
            settings.setString(k, preset.getString(k, settings.getString(k)));
        }
    }
    PlayerProfiles profiles;
    for (int i = 0; i < 2; ++i)
    {
        auto p = profiles.players[i % profiles.players.size()];
        const std::string k = i ? "right_" : "left_";
        p.human = settings.getBool(k + "player_human", true);
        p.script = settings.getString(k + "script_name", "hyp014");
        p.delay = settings.getInteger(k + "script_strength", 10);
        players.push_back(p);
    }
    cup.settings.name = "Local Cup";
    cup.settings.parallel = 1;
    cup.slots = 32;
}
void LocalSession::prepare(unsigned seed)
{
    if (players.size() < 2 || players.size() > 32)
    {
        throw std::runtime_error("Choose 2 to 32 participants.");
    }
    if (!settings.saveFile("local-setup.xml"))
    {
        throw std::runtime_error("Could not save settings.");
    }
    if (!tournament)
    {
        pending = 0;
        return;
    }
    const auto options = cup.settings;
    cup = tournament::Tournament();
    cup.settings = options;
    cup.settings.parallel = 1;
    cup.slots = 32;
    cup.settings.capacity = 32;
    cup.settings.rules = settings.getString("rules", "default.lua");
    cup.settings.points = settings.getInteger("scoretowin", 15);
    cup.settings.speed = settings.getInteger("gamefps", 75);
    for (unsigned i = 0; i < players.size(); ++i)
    {
        const auto& p = players[i];
        if (p.name.find_first_not_of(" \t\r\n") == std::string::npos)
        {
            throw std::runtime_error("Enter a name.");
        }
        int id = cup.registerPerson(p.name, p.color, std::to_string(i));
        cup.enroll(id, true);
        cup.person(id).connected = true;
        cup.person(id).ready = true;
    }
    cup.start(seed);
    const auto available = cup.available();
    pending = available.empty() ? -1 : available.front();
}
std::pair<int, int> LocalSession::nextPlayers() const
{
    if (!tournament)
    {
        return {0, 1};
    }
    const auto& m = cup.match(pending);
    return {m.a, m.b};
}
void LocalSession::beginMatch()
{
    if (tournament && pending >= 0)
    {
        cup.match(pending).status = tournament::MatchStatus::Playing;
        save();
    }
}
void LocalSession::finish(PlayerSide winner, int a, int b)
{
    if (!tournament || pending < 0)
    {
        return;
    }
    auto& m = cup.match(pending);
    cup.result(pending, winner == LEFT_PLAYER ? m.a : m.b, a, b);
    for (auto& p : cup.people)
    {
        p.connected = true;
        p.ready = true;
    }
    auto available = cup.available();
    pending = available.empty() ? -1 : available.front();
    save();
}
void LocalSession::save()
{
    if (!tournament || !started)
    {
        return;
    }
    // New tournaments must supersede both slots from a previous competition.
    for (const char* path : {"local-cup-a.xml", "local-cup-b.xml"})
    {
        UserConfig previous;
        if (load(previous, path))
        {
            saveRevision = std::max(saveRevision, unsigned(std::max(0, previous.getInteger("revision"))));
        }
    }
    UserConfig c = settings;
    c.setInteger("revision", int(++saveRevision));
    c.setInteger("pending", pending);
    c.setString("cup", cup.xml(true));
    c.setInteger("participants", int(players.size()));
    for (unsigned i = 0; i < players.size(); ++i)
    {
        put(c, "entrant_" + std::to_string(i) + "_", players[i]);
    }
    if (!c.saveFile(saveRevision % 2 ? "local-cup-a.xml" : "local-cup-b.xml"))
    {
        throw std::runtime_error("Could not save tournament.");
    }
}
std::shared_ptr<LocalSession> LocalSession::resume()
{
    std::shared_ptr<LocalSession> best;
    for (const char* path : {"local-cup-a.xml", "local-cup-b.xml"})
    {
        UserConfig c;
        if (!load(c, path))
        {
            continue;
        }
        try
        {
            auto s = std::make_shared<LocalSession>();
            s->cup = tournament::Tournament::parse(c.getString("cup"), true);
            int count = c.getInteger("participants");
            if (count < 2 || count > 32 || count != int(s->cup.people.size()))
            {
                continue;
            }
            s->players.clear();
            for (int i = 0; i < count; ++i)
            {
                s->players.push_back(get(c, "entrant_" + std::to_string(i) + "_"));
            }
            s->settings = c;
            s->tournament = true;
            s->started = true;
            s->pending = c.getInteger("pending", -1);
            s->saveRevision = unsigned(c.getInteger("revision"));
            if (s->pending >= int(s->cup.matches.size()) || s->pending < -1)
            {
                continue;
            }
            if (s->pending >= 0)
            {
                const auto& next = s->cup.match(s->pending);
                if (next.a < 0 || next.b < 0 || next.a >= count || next.b >= count ||
                    (next.status != tournament::MatchStatus::Waiting &&
                     next.status != tournament::MatchStatus::Playing))
                {
                    continue;
                }
            }
            if (!best || s->saveRevision > best->saveRevision)
            {
                best = s;
            }
        }
        catch (const std::exception&)
        {
        }
    }
    return best;
}
