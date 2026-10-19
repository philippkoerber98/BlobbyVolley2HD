#include "TournamentHost.h"
#include "SpectatorOrder.h"
#include "DedicatedServer.h"
#include "FileSystem.h"
#include "FileRead.h"
#include "FileWrite.h"
#include "GenericIO.h"
#include "ThreadSafeRakServer.h"
#include "raknet/RakServer.h"
#include "tinyxml2.h"
#include <SDL.h>
#include <algorithm>
#include <iomanip>
#include <iostream>
#include <random>
#include <sstream>
#include <stdexcept>

using namespace tournament;
namespace
{
std::string token()
{
    std::random_device random;
    std::ostringstream out;
    for (int i = 0; i < 8; ++i)
    {
        out << std::hex << std::setw(8) << std::setfill('0') << random();
    }
    return out.str();
}
void need(bool value, const char* error)
{
    if (!value)
    {
        throw std::runtime_error(error);
    }
}
bool loopback(PlayerID source)
{
    return (source.binaryAddress & 255u) == 127;
}
} // namespace
TournamentHost::TournamentHost(DedicatedServer& server, int slots, std::string password)
    : mServer(server), mSlots(slots), mPassword(std::move(password))
{
    if (mPassword.empty())
    {
        mPassword = std::to_string(100000u + std::random_device{}() % 900000u);
        std::cout << "Tournament organizer code: " << mPassword << "\n";
    }
    mSavePath = "Tournaments/server-" + std::to_string(server.mServerInfo.port);
    unsigned highest = 0;
    bool found = false;
    for (const char* slot : {"-a.xml", "-b.xml"})
    {
        try
        {
            FileRead file(mSavePath + slot);
            const auto bytes = file.readRawBytes(file.length());
            auto saved = Tournament::parse(std::string(bytes.begin(), bytes.end()), true);
            if (!found || saved.revision >= highest)
            {
                highest = saved.revision;
                mCup = std::move(saved);
                found = true;
            }
        }
        catch (const std::exception&)
        {
        }
    }
    if (found)
    {
        mCup.recover();
        mNeedsResume = mCup.phase == Phase::Running;
    }
    mCup.slots = slots;
    mCup.settings.capacity = (std::min)(mCup.settings.capacity, slots);
    if (!found)
    {
        mCup.settings.rules = server.mTournamentRules;
    }
}
bool TournamentHost::contains(PlayerID source) const
{
    return mConnections.count(source) != 0;
}
bool TournamentHost::admin(int id) const
{
    return id == mCup.organizer;
}
void TournamentHost::send(PlayerID source, Command command, const std::string& payload)
{
    RakNet::BitStream out;
    writePacket(out, command, payload);
    mServer.mServer->Send(out, HIGH_PRIORITY, RELIABLE_ORDERED, source);
}
void TournamentHost::state()
{
    const auto xml = mCup.xml();
    for (const auto& c : mConnections)
    {
        send(c.first, Command::State, xml);
    }
    mLastState = SDL_GetTicks();
}
void TournamentHost::save()
{
    if (mSavedRevision == mCup.revision)
    {
        return;
    }
    auto& fs = FileSystem::getSingleton();
    need(fs.mkdir("Tournaments") || fs.isDirectory("Tournaments"), "Cannot create tournament save folder");
    FileWrite file(mSavePath + (mCup.revision % 2 ? "-a.xml" : "-b.xml"));
    file.write(mCup.xml(true));
    file.close();
    mSavedRevision = mCup.revision;
}
void TournamentHost::packet(PlayerID source, RakNet::BitStream& stream)
{
    try
    {
        Command command;
        const auto payload = readPacket(stream, command);
        if (command == Command::Hello)
        {
            if (contains(source))
            {
                state();
                return;
            }
            const auto player = mServer.mPlayerMap.find(source);
            need(player != mServer.mPlayerMap.end(), "Enter the server first");
            need(!player->second->getGame(), "Return from the current match first");
            int id = -1;
            if (!payload.empty())
            {
                for (const auto& p : mCup.people)
                {
                    if (!p.token.empty() && p.token == payload)
                    {
                        id = p.id;
                        break;
                    }
                }
            }
            need(id < 0 || !mCup.person(id).connected, "This identity is already connected");
            int reserved = 0;
            for (const auto& r : mReservedUntil)
            {
                if (!SDL_TICKS_PASSED(SDL_GetTicks(), r.second) && r.first != id)
                {
                    ++reserved;
                }
            }
            need(mServer.getConnectedClients() + reserved <= mSlots,
                 "Server slots are reserved for reconnecting players");
            if (id < 0)
            {
                id = mCup.registerPerson(player->second->getName(), player->second->getColor().toInt(),
                                         token());
            }
            if (mCup.phase == Phase::Registration)
            {
                mCup.person(id).name = player->second->getName();
                mCup.person(id).color = player->second->getColor().toInt();
                mCup.person(id).cycling = player->second->getIdentity().getOscillating();
            }
            mCup.person(id).connected = true;
            mCup.person(id).ready = false;
            mReservedUntil.erase(id);
            mConnections[source] = id;
            mPlayers[id] = source;
            if (loopback(source) && (mCup.organizer < 0 || !mCup.person(mCup.organizer).connected))
            {
                mCup.organizer = id;
            }
            ++mCup.revision;
            send(source, Command::Welcome, std::to_string(id) + ":" + mCup.person(id).token);
            state();
            save();
            return;
        }
        need(contains(source), "Open the tournament lobby first");
        const int id = mConnections.at(source);
        auto& person = mCup.person(id);
        if (command == Command::Claim)
        {
            auto& attempts = mClaimAttempts[source];
            const auto now = SDL_GetTicks();
            if (SDL_TICKS_PASSED(now, attempts.first + 60000))
            {
                attempts = {now, 0};
            }
            need(++attempts.second <= 5, "Too many organizer attempts. Wait one minute.");
            need(!mPassword.empty() && payload == mPassword, "Invalid organizer password");
            mCup.organizer = id;
            ++mCup.revision;
        }
        else if (command == Command::Configure)
        {
            need(admin(id) && mCup.phase == Phase::Registration,
                 "Only organizer can change registration settings");
            auto config = Tournament::parse(payload);
            need(config.settings.capacity <= mSlots, "Too many entrants for server slots");
            need(config.settings.rules == mServer.mTournamentRules, "Choose the server rules");
            int entered = 0;
            for (const auto& p : mCup.people)
            {
                entered += p.entered;
            }
            need(config.settings.capacity >= entered, "Entrant limit is below registered players");
            need(!config.settings.name.empty() && config.settings.name.size() <= 63,
                 "Choose a tournament name");
            mCup.matches.clear();
            mCup.settings = config.settings;
            for (auto& p : mCup.people)
            {
                p.ready = false;
            }
            ++mCup.revision;
        }
        else if (command == Command::Enroll)
        {
            mCup.enroll(id, payload == "1");
        }
        else if (command == Command::RemoveEntrant)
        {
            need(admin(id), "Only organizer can change registration settings");
            mCup.enroll(std::stoi(payload), false);
        }
        else if (command == Command::Ready)
        {
            need(person.entered && mCup.phase != Phase::Finished, "Only entrants can be ready");
            need(!mServer.mPlayerMap.at(source)->getGame(), "Return from the current match first");
            person.ready = payload == "1";
            ++mCup.revision;
            // Both players can resume a disconnected match without organizer intervention.
            for (auto& m : mCup.matches)
            {
                if (m.status == MatchStatus::Interrupted && mCup.person(m.a).connected &&
                    mCup.person(m.b).connected && mCup.person(m.a).ready && mCup.person(m.b).ready)
                {
                    mCup.restart(m.id);
                }
            }
        }
        else if (command == Command::Start)
        {
            need(admin(id), "Only organizer can start");
            need(mServer.getActiveGamesCount() == 0, "Finish existing matches first");
            mCup.start(mCup.matches.empty() ? std::random_device{}() : unsigned(mCup.seed));
            mActivated = true;
        }
        else if (command == Command::Draw)
        {
            need(admin(id), "Only organizer can draw");
            mCup.preview(std::random_device{}());
        }
        else if (command == Command::Resume)
        {
            need(admin(id), "Only organizer can resume");
            mNeedsResume = false;
            mActivated = true;
            for (auto& m : mCup.matches)
            {
                if (m.status == MatchStatus::Interrupted)
                {
                    mCup.restart(m.id);
                }
            }
            ++mCup.revision;
        }
        else if (command == Command::NewCup)
        {
            need(admin(id) && mCup.phase == Phase::Finished, "Finish the current tournament first");
            for (const auto& p : mServer.mPlayerMap)
            {
                need(!p.second->getGame(), "All players must return to the overview first");
            }
            for (auto& entry : mGames)
            {
                entry.second->stop();
            }
            mGames.clear();
            mWatching.clear();
            mInterrupted.clear();
            mCup.matches.clear();
            mCup.phase = Phase::Registration;
            mActivated = false;
            for (auto& p : mCup.people)
            {
                p.entered = false;
                p.ready = false;
                p.group = -1;
            }
            ++mCup.revision;
        }
        else if (command == Command::Return)
        {
            auto player = mServer.mPlayerMap.at(source);
            auto game = player->getGame();
            if (game)
            {
                for (auto& entry : mGames)
                {
                    if (entry.second == game)
                    {
                        auto& m = mCup.match(entry.first);
                        if (m.status == MatchStatus::Playing)
                        {
                            auto v = game->view();
                            mInterrupted[m.id] = v;
                            mCup.interrupt(m.id);
                            game->stop();
                            // Leaving an unfinished match is an interruption, never an invented result.
                        }
                    }
                }
                std::lock_guard<std::mutex> lock(mServer.mPlayerMapMutex);
                player->setGame(nullptr);
            }
            person.ready = false;
            ++mCup.revision;
        }
        else if (command == Command::Watch)
        {
            need(!mServer.mPlayerMap.at(source)->getGame(), "Leave your match before watching");
            const int match = std::stoi(payload);
            need(mGames.count(match) && mCup.match(match).status == MatchStatus::Playing,
                 "Match is not live");
            mWatching[source] = match;
        }
        else if (command == Command::StopWatching)
        {
            mWatching.erase(source);
        }
        else if (command == Command::Restart || command == Command::Award)
        {
            need(admin(id), "Only organizer can resolve interrupted matches");
            std::istringstream values(payload);
            int match = -1, winner = -1;
            values >> match >> winner;
            need(mCup.match(match).status == MatchStatus::Interrupted, "Match is not interrupted");
            if (command == Command::Restart)
            {
                mInterrupted.erase(match);
                mCup.restart(match);
            }
            else
            {
                mCup.result(match, winner, 0, 0, true);
            }
        }
        else
        {
            throw std::runtime_error("Unsupported tournament command");
        }
        save();
        state();
    }
    catch (const std::exception& e)
    {
        send(source, Command::Error, e.what());
        if (!contains(source))
        {
            const auto player = mServer.mPlayerMap.find(source);
            // A rejected tournament request must not eject an ordinary match player.
            // Local RakNet closes do not generate a disconnect event on the server.
            if (player == mServer.mPlayerMap.end() || !player->second->getGame())
            {
                mServer.mServer->access([&](RakServer& server) { server.CloseConnection(source, true); });
                mServer.mActiveConnections.erase(source);
                {
                    std::lock_guard<std::mutex> lock(mServer.mPlayerMapMutex);
                    mServer.mPlayerMap.erase(source);
                }
                mServer.mMatchMaker.removePlayer(source);
            }
        }
    }
}
void TournamentHost::disconnected(PlayerID source)
{
    mWatching.erase(source);
    mClaimAttempts.erase(source);
    auto it = mConnections.find(source);
    if (it == mConnections.end())
    {
        return;
    }
    int id = it->second;
    mConnections.erase(it);
    mPlayers.erase(id);
    auto& p = mCup.person(id);
    p.connected = false;
    p.ready = false;
    if (p.entered && mCup.phase == Phase::Running)
    {
        mReservedUntil[id] = SDL_GetTicks() + 60000;
    }
    ++mCup.revision;
    for (auto& game : mGames)
    {
        auto& m = mCup.match(game.first);
        if (m.status == MatchStatus::Playing && (m.a == id || m.b == id))
        {
            auto view = game.second->view();
            mInterrupted[m.id] = view;
            mCup.interrupt(m.id);
            game.second->stop();
        }
    }
    try
    {
        save();
    }
    catch (const std::exception&)
    {
    }
    state();
}
void TournamentHost::schedule()
{
    if (!mActivated || mNeedsResume || mCup.phase != Phase::Running)
    {
        return;
    }
    for (int id : mCup.available())
    {
        auto& m = mCup.match(id);
        auto left = mServer.mPlayerMap.at(mPlayers.at(m.a)), right = mServer.mPlayerMap.at(mPlayers.at(m.b));
        if (left->getGame() || right->getGame())
        {
            continue;
        }
        // Freeze the registered identity for every view, including reconnects.
        for (int person : {m.a, m.b})
        {
            const auto& identity = mCup.person(person);
            auto player = mServer.mPlayerMap.at(mPlayers.at(person));
            player->setDisplayName(identity.name);
            player->setColor(Color(identity.color));
            player->setCycling(identity.cycling);
        }
        // Always present each entrant on their configured side, just like ordinary LAN matches.
        PlayerSide switched = NO_PLAYER;
        if (left->getDesiredSide() == RIGHT_PLAYER)
        {
            std::swap(left, right);
        }
        if (left->getDesiredSide() == right->getDesiredSide())
        {
            switched = left->getDesiredSide() == LEFT_PLAYER ? RIGHT_PLAYER : LEFT_PLAYER;
        }
        m.status = MatchStatus::Playing;
        ++mCup.revision;
        state();
        auto resume = mInterrupted.find(id);
        try
        {
            mServer.createGame(*left, *right, switched, mCup.settings.rules, mCup.settings.points,
                               float(mCup.settings.speed),
                               resume == mInterrupted.end() ? nullptr : &resume->second.state,
                               resume == mInterrupted.end() ? 0 : resume->second.timeMs);
            mGames[id] = left->getGame();
            if (resume != mInterrupted.end())
            {
                mInterrupted.erase(resume);
            }
        }
        catch (const std::exception& e)
        {
            mCup.interrupt(id);
            for (const auto& c : mConnections)
            {
                send(c.first, Command::Error, e.what());
            }
            state();
            save();
            continue;
        }
        mWatching.erase(left->getID());
        mWatching.erase(right->getID());
        save();
    }
}
void TournamentHost::tick()
{
    bool changed = false;
    for (auto& entry : mGames)
    {
        auto& m = mCup.match(entry.first);
        if (m.status != MatchStatus::Playing)
        {
            continue;
        }
        auto view = entry.second->view();
        auto winner = view.state.getWinningPlayer();
        if (winner != NO_PLAYER)
        {
            const bool aLeft = entry.second->getPlayerID(LEFT_PLAYER) == mPlayers.at(m.a);
            mCup.result(m.id, (winner == LEFT_PLAYER) == aLeft ? m.a : m.b,
                        view.state.getScore(aLeft ? LEFT_PLAYER : RIGHT_PLAYER),
                        view.state.getScore(aLeft ? RIGHT_PLAYER : LEFT_PLAYER));
            entry.second->stop();
            changed = true;
        }
        else if (!entry.second->isGameValid())
        {
            mInterrupted[m.id] = view;
            mCup.interrupt(m.id);
            changed = true;
        }
    }
    for (auto it = mGames.begin(); it != mGames.end();)
    {
        const auto& match = mCup.match(it->first);
        bool retained = false;
        for (const auto& p : mServer.mPlayerMap)
        {
            retained |= p.second->getGame() == it->second;
        }
        if (match.status == MatchStatus::Finished && !retained)
        {
            for (auto w = mWatching.begin(); w != mWatching.end();)
            {
                if (w->second == it->first)
                {
                    w = mWatching.erase(w);
                }
                else
                {
                    ++w;
                }
            }
            it = mGames.erase(it);
        }
        else
        {
            ++it;
        }
    }
    schedule();
    if (changed)
    {
        save();
        state();
    }
    if (SDL_TICKS_PASSED(SDL_GetTicks(), mLastState + 2000))
    {
        state();
    }
    if (SDL_TICKS_PASSED(SDL_GetTicks(), mLastLive + 30))
    {
        mLastLive = SDL_GetTicks();
        for (const auto& w : mWatching)
        {
            auto it = mGames.find(w.second);
            if (it == mGames.end())
            {
                continue;
            }
            auto v = it->second->view();
            const auto& match = mCup.match(w.second);
            int left = spectatorLeft(mCup.person(match.a).name, match.a, mCup.person(match.b).name, match.b)
                           ? match.a
                           : match.b;
            if (mPlayers.count(left) && it->second->getPlayerID(LEFT_PLAYER) != mPlayers.at(left))
            {
                v.state.swapSides();
            }
            RakNet::BitStream out;
            out.Write((unsigned char)ID_TOURNAMENT);
            out.Write((unsigned char)Command::Live);
            out.Write(w.second);
            out.Write(v.paused);
            out.Write(v.timeMs);
            auto writer = createGenericWriter(&out);
            writer->generic<DuelMatchState>(v.state);
            mServer.mServer->Send(out, LOW_PRIORITY, UNRELIABLE_SEQUENCED, w.first);
        }
    }
}
