#include "TournamentState.h"
#include "SpectatorOrder.h"
#include "BlobbyApp.h"
#include "RenderInterpolation.h"
#include "NetworkState.h"
#include "NetworkSearchState.h"
#include "DuelMatch.h"
#include "GenericIO.h"
#include "InputManager.h"
#include "UserConfig.h"
#include "WarmUI.h"
#include "raknet/RakClient.h"
#include <SDL.h>
#include <algorithm>
#include <sstream>
#include <functional>
using namespace tournament;
namespace
{
std::string tr(const std::string& key)
{
    return translateUI(WarmUI::language(), key);
}
std::string tokenKey(const ServerInfo& info)
{
    return "tournament_token_" + std::string(info.hostname) + "_" + std::to_string(info.port);
}
std::string personName(const Tournament& t, int id)
{
    return id < 0 ? tr("To be determined") : t.person(id).name;
}
std::string status(const Match& m)
{
    switch (m.status)
    {
    case MatchStatus::Playing:
        return tr("Live");
    case MatchStatus::Finished:
        return std::to_string(m.scoreA) + ":" + std::to_string(m.scoreB) +
               (m.forfeit ? " (" + tr("Forfeit") + ")" : "");
    case MatchStatus::Bye:
        return tr("Bye");
    case MatchStatus::Interrupted:
        return tr("Interrupted");
    default:
        return tr("Waiting");
    }
}
std::string formatName(Format f)
{
    return tr(f == Format::Knockout ? "Knockout" : f == Format::Groups ? "Groups + knockout" : "Round robin");
}
bool button(IMGUI& g, float x, float y, float w, const std::string& label, bool enabled = true)
{
    return g.doButton(g.getNextId(), Vector2(x, y), tr(label), TF_NORMAL, w, enabled);
}
std::string matchLabel(const Tournament& t, const Match& m)
{
    return "#" + std::to_string(m.id + 1) + "  " + personName(t, m.a) + " - " + personName(t, m.b) + "   " +
           status(m);
}
} // namespace
TournamentConnection::TournamentConnection(std::shared_ptr<RakClient> client, const ServerInfo& server)
    : client(std::move(client)), server(server)
{
}
void TournamentConnection::send(Command command, const std::string& payload)
{
    RakNet::BitStream out;
    writePacket(out, command, payload);
    client->Send(&out, HIGH_PRIORITY, RELIABLE_ORDERED, 0);
}
bool TournamentConnection::receive(const packet_ptr& packet)
{
    if (packet->data[0] == ID_CONNECTION_LOST || packet->data[0] == ID_DISCONNECTION_NOTIFICATION)
    {
        connected = false;
        error = "Connection lost. Rejoin the server to reconnect.";
        return true;
    }
    if (packet->data[0] != ID_TOURNAMENT)
    {
        return false;
    }
    if (packet->length >= 2 && packet->data[1] == unsigned(Command::Live))
    {
        return true;
    }
    try
    {
        RakNet::BitStream in(packet->data, packet->length, false);
        Command command;
        const auto body = readPacket(in, command);
        if (command == Command::Welcome)
        {
            auto colon = body.find(':');
            if (colon == std::string::npos)
            {
                throw std::runtime_error("Invalid welcome");
            }
            own = std::stoi(body.substr(0, colon));
            UserConfig config;
            config.loadFile("config.xml");
            config.setString(tokenKey(server), body.substr(colon + 1));
            config.saveFile("config.xml");
        }
        else if (command == Command::State)
        {
            cup = Tournament::parse(body);
            received = true;
        }
        else if (command == Command::Error)
        {
            error = body;
        }
    }
    catch (const std::exception& e)
    {
        error = e.what();
    }
    return true;
}
TournamentState::TournamentState(std::shared_ptr<TournamentConnection> connection, bool hello)
    : mConnection(std::move(connection)), mHello(hello)
{
}
void TournamentState::init()
{
    if (mHello)
    {
        UserConfig config;
        config.loadFile("config.xml");
        mConnection->send(Command::Hello, config.getString(tokenKey(mConnection->server), ""));
    }
    else
    {
        mConnection->activeMatch = -1;
        mConnection->send(Command::Return);
    }
}
void TournamentState::step_impl()
{
    const bool leaveWasOpen = mLeave;
    packet_ptr packet;
    while ((packet = mConnection->client->Receive()))
    {
        if (packet->data[0] == ID_RULES_CHECKSUM)
        {
            RakNet::BitStream in(packet->data, packet->length, false);
            in.IgnoreBytes(1);
            int checksum, points;
            if (in.Read(checksum) && in.Read(points))
            {
                switchState(new NetworkGameState(mConnection->client, checksum, points, mConnection));
                return;
            }
        }
        else
        {
            mConnection->receive(packet);
        }
    }
    auto& g = getIMGUI();
    auto& t = mConnection->cup;
    WarmUI::page(g, mConnection->received ? t.settings.name : tr("LAN tournament"));
    if (mClaim || mLeave)
    {
        g.doInactiveMode(true);
    }
    const char* tabs[] = {"Overview", "Bracket / schedule", "Standings", "Participants"};
    if (!mClaim && !mLeave && getInputMgr().previousTab())
    {
        mTab = (mTab + 3) % 4;
        g.resetSelection();
    }
    if (!mClaim && !mLeave && getInputMgr().nextTab())
    {
        mTab = (mTab + 1) % 4;
        g.resetSelection();
    }
    for (unsigned i = 0; i < 4; ++i)
    {
        if (mTab == i)
        {
            WarmUI::panel(g, 39 + i * 182, 87, 177, 36, Color(255, 207, 91));
        }
        if (button(g, 40 + i * 182, 88, 175, tabs[i]))
        {
            mTab = i;
            mSelected = 0;
            g.resetSelection();
        }
    }
    if (mConnection->received && mConnection->own >= 0)
    {
        if (mTab == 0)
        {
            overview();
        }
        else if (mTab == 1)
        {
            schedule();
        }
        else if (mTab == 2)
        {
            table();
        }
        else
        {
            participants();
        }
    }
    else
    {
        WarmUI::text(g, 40, 155, tr("Connecting to tournament..."), 22);
    }
    if (!mConnection->error.empty())
    {
        WarmUI::panel(g, 40, 451, 720, 45);
        WarmUI::text(g, 52, 462,
                     tr(mConnection->error) == mConnection->error && WarmUI::language() != "en"
                         ? tr("Tournament request failed")
                         : tr(mConnection->error),
                     15, 2);
        if (button(g, 620, 500, 140, "Dismiss"))
        {
            mConnection->error.clear();
        }
    }
    if (button(g, 620, 545, 140, "Leave server") || (!mClaim && !mLeave && is_exiting()))
    {
        mLeave = true;
        g.resetSelection();
    }
    if (mLeave)
    {
        g.doInactiveMode(!leaveWasOpen);
        WarmUI::panel(g, 140, 225, 520, 160);
        WarmUI::text(g, 160, 245, tr("Leave the tournament server?"), 25, 2);
        WarmUI::text(g, 160, 285, tr("The hosted server runs until this app closes."), 17);
        if (button(g, 160, 330, 230, "Leave server"))
        {
            mConnection->client->Disconnect(25);
            switchState(new LANSearchState);
        }
        if (button(g, 410, 330, 230, "Cancel"))
        {
            mLeave = false;
            g.resetSelection();
        }
    }
    WarmUI::text(g, 40, 589, tr("LB / RB: tabs    Changes are managed by the server"), 12);
}
void TournamentState::overview()
{
    auto& g = getIMGUI();
    auto& t = mConnection->cup;
    auto& me = t.person(mConnection->own);
    const bool admin = t.organizer == me.id;
    WarmUI::text(g, 40, 137,
                 formatName(t.settings.format) + " | " +
                     tr(t.phase == Phase::Registration ? "Registration"
                        : t.phase == Phase::Running    ? "Tournament running"
                                                       : "Tournament finished"),
                 22, 2);
    if (t.phase == Phase::Registration)
    {
        std::vector<std::string> entrants;
        int count = 0;
        for (const auto& p : t.people)
        {
            if (p.entered)
            {
                ++count;
                entrants.push_back(p.name + "  " +
                                   tr(p.connected ? (p.ready ? "Ready" : "Not ready") : "Disconnected"));
            }
        }
        WarmUI::text(
            g, 40, 177,
            tr("Entrants") + ": " + std::to_string(count) + " / " + std::to_string(t.settings.capacity), 20);
        g.doSelectbox(g.getNextId(), Vector2(40, 208), Vector2(390, 432), entrants, mSelected);
        auto change = [&](const std::string& label, int y, const std::function<void(Settings&, int)>& edit)
        {
            WarmUI::panel(g, 410, float(y), 350, 34);
            WarmUI::text(g, 452, float(y) + 7, label, 17);
            const bool minus = button(g, 410, float(y), 34, "-", admin);
            const bool plus = button(g, 726, float(y), 34, "+", admin);
            if (minus || plus)
            {
                auto copy = t;
                edit(copy.settings, minus ? -1 : 1);
                mConnection->send(Command::Configure, copy.xml());
            }
        };
        change(tr("Format") + ": " + formatName(t.settings.format), 177,
               [](Settings& s, int step) { s.format = Format((int(s.format) + step + 3) % 3); });
        change(tr("Entrant limit") + ": " + std::to_string(t.settings.capacity), 219,
               [&](Settings& s, int step)
               { s.capacity = (std::max)(count, (std::max)(2, (std::min)(t.slots, s.capacity + step))); });
        change(tr("Parallel matches") + ": " + std::to_string(t.settings.parallel), 261,
               [](Settings& s, int step) { s.parallel = (std::max)(1, (std::min)(16, s.parallel + step)); });
        change(tr("Points to win") + ": " + std::to_string(t.settings.points), 303,
               [](Settings& s, int step) { s.points = (std::max)(1, (std::min)(50, s.points + step)); });
        change(tr("Game speed") + ": " + std::to_string(t.settings.speed * 100 / 75) + " %", 345,
               [](Settings& s, int step) { s.speed = (std::max)(30, (std::min)(120, s.speed + 15 * step)); });
        WarmUI::text(g, 410, 396, tr("Rules") + ": " + t.settings.rules, 16);
        if (admin)
        {
            if (mName.empty())
            {
                mName = t.settings.name;
                mNameCursor = unsigned(mName.size());
            }
            g.doEditbox(g.getNextId(), Vector2(410, 430), 14, mName, mNameCursor);
            if (button(g, 660, 430, 100, "Rename"))
            {
                auto copy = t;
                copy.settings.name = mName;
                mConnection->send(Command::Configure, copy.xml());
            }
        }

        const int n = count;
        const int estimated = t.settings.format == Format::Knockout ? (std::max)(0, n - 1)
                              : t.settings.format == Format::RoundRobin
                                  ? n * (n - 1) / 2
                                  : (
                                        [&]()
                                        {
                                            int groups = t.groupCount(), sum = 0;
                                            for (int i = 0; i < groups; ++i)
                                            {
                                                int size = n / groups + (i < n % groups);
                                                sum += size * (size - 1) / 2;
                                            }
                                            return sum + groups * 2 - 1;
                                        })();
        WarmUI::text(g, 40, 442,
                     tr("Matches") + ": " +
                         std::to_string(estimated + (t.settings.format == Format::RoundRobin
                                                         ? t.settings.finalMatch
                                                         : (count >= 4 && t.settings.thirdPlace))),
                     17);
        if (button(g, 190, 442, 200,
                   (t.settings.format == Format::RoundRobin ? tr("Final match") : tr("Third place")) + ": " +
                       tr((t.settings.format == Format::RoundRobin ? t.settings.finalMatch
                                                                   : t.settings.thirdPlace)
                              ? "On"
                              : "Off"),
                   admin))
        {
            auto copy = t;
            if (t.settings.format == Format::RoundRobin)
            {
                copy.settings.finalMatch = !copy.settings.finalMatch;
            }
            else
            {
                copy.settings.thirdPlace = !copy.settings.thirdPlace;
            }
            mConnection->send(Command::Configure, copy.xml());
        }
        if (button(g, 300, 545, 240, "Draw preview",
                   admin && count >= (t.settings.format == Format::Groups ? 6 : 2)))
        {
            mConnection->send(Command::Draw);
        }

        if (button(g, 40, 500, 180, me.entered ? "Withdraw" : "Enter tournament"))
        {
            mConnection->send(Command::Enroll, me.entered ? "0" : "1");
        }
        if (button(g, 232, 500, 180, me.ready ? "Not ready" : "Ready", me.entered))
        {
            mConnection->send(Command::Ready, me.ready ? "0" : "1");
        }
        if (button(g, 424, 500, 180, "Start tournament",
                   admin && count >= (t.settings.format == Format::Groups ? 6 : 2) &&
                       std::all_of(t.people.begin(), t.people.end(),
                                   [](const Person& p) { return !p.entered || (p.connected && p.ready); })))
        {
            mConnection->send(Command::Start);
        }
    }
    else
    {
        int completed = 0;
        for (const auto& m : t.matches)
        {
            completed += m.status == MatchStatus::Finished || m.status == MatchStatus::Bye;
        }
        WarmUI::text(g, 40, 178,
                     tr("Matches completed") + ": " + std::to_string(completed) + " / " +
                         std::to_string(t.matches.size()),
                     20);
        bool eliminated = false;
        for (const auto& m : t.matches)
        {
            if (m.knockout && !m.placement && m.status == MatchStatus::Finished &&
                (m.a == me.id || m.b == me.id) && m.winner != me.id)
            {
                eliminated = true;
            }
        }
        if (t.settings.format == Format::Groups &&
            std::any_of(t.matches.begin(), t.matches.end(), [](const Match& m) { return m.knockout; }))
        {
            eliminated = std::none_of(t.matches.begin(), t.matches.end(), [&](const Match& m)
                                      { return m.knockout && (m.a == me.id || m.b == me.id); }) ||
                         eliminated;
        }
        bool canPlay = false;
        for (const auto& m : t.matches)
        {
            if (m.status == MatchStatus::Waiting || m.status == MatchStatus::Interrupted)
            {
                if (m.a == me.id || m.b == me.id)
                {
                    canPlay = true;
                }
                for (int source : {m.fromA, m.fromB})
                {
                    if (source >= 0)
                    {
                        const auto& parent = t.match(source);
                        if (parent.status == MatchStatus::Finished)
                        {
                            int candidate = m.placement ? (parent.winner == parent.a ? parent.b : parent.a)
                                                        : parent.winner;
                            if (candidate == me.id)
                            {
                                canPlay = true;
                            }
                        }
                    }
                }
            }
        }
        WarmUI::text(g, 40, 199,
                     tr(!me.entered              ? "You are watching the tournament"
                        : eliminated && !canPlay ? "Eliminated - you can watch matches"
                        : canPlay                ? "Ready up for your next match"
                                                 : "Waiting - you can watch other matches"),
                     17, 2);
        std::vector<std::string> entries;
        std::vector<int> ids;
        for (const auto& m : t.matches)
        {
            if (m.status == MatchStatus::Playing || m.status == MatchStatus::Interrupted ||
                (m.status == MatchStatus::Waiting && (m.a == me.id || m.b == me.id)))
            {
                entries.push_back(matchLabel(t, m));
                ids.push_back(m.id);
            }
        }
        g.doSelectbox(g.getNextId(), Vector2(40, 238), Vector2(760, 418), entries, mSelected);
        if (mSelected >= ids.size())
        {
            mSelected = 0;
        }
        int selected = ids.empty() ? -1 : ids[mSelected];
        if (t.phase != Phase::Finished && button(g, 40, 500, 180, me.ready ? "Not ready" : "Ready",
                                                 me.entered && canPlay && t.phase == Phase::Running))
        {
            mConnection->send(Command::Ready, me.ready ? "0" : "1");
        }
        if (button(g, 232, 500, 180, "Watch match",
                   selected >= 0 && t.match(selected).status == MatchStatus::Playing))
        {
            switchState(new TournamentSpectatorState(mConnection, selected));
        }
        if (button(g, 424, 500, 180, "Resume saved cup", admin && t.phase == Phase::Running))
        {
            mConnection->send(Command::Resume);
        }
        if (t.phase == Phase::Finished)
        {
            if (button(g, 40, 500, 240, "New tournament", admin))
            {
                mConnection->send(Command::NewCup);
            }
            int champion = t.champion();
            WarmUI::text(g, 40, 448, tr("Champion") + ": " + personName(t, champion), 28, 2);
        }
    }
    if (!admin && button(g, 40, 545, 240, "Organizer access"))
    {
        mClaim = !mClaim;
        g.resetSelection();
    }
    if (mClaim)
    {
        g.doInactiveMode(false);
        WarmUI::panel(g, 150, 240, 500, 160);
        WarmUI::text(g, 170, 255, tr("Organizer password"), 22);
        g.doEditbox(g.getNextId(), Vector2(170, 292), 63, mPassword, mPasswordCursor, TF_OBFUSCATE);
        if (button(g, 170, 345, 210, "Claim organizer"))
        {
            mConnection->send(Command::Claim, mPassword);
            mPassword.clear();
            mPasswordCursor = 0;
            mClaim = false;
        }
        if (button(g, 400, 345, 220, "Cancel"))
        {
            mClaim = false;
        }
        g.doInactiveMode(true);
    }
}
void TournamentState::schedule()
{
    auto& g = getIMGUI();
    auto& t = mConnection->cup;
    bool bracket = std::any_of(t.matches.begin(), t.matches.end(), [](const Match& m) { return m.knockout; });
    if (!bracket)
    {
        std::vector<std::string> rows;
        for (const auto& m : t.matches)
        {
            rows.push_back(matchLabel(t, m));
        }
        g.doSelectbox(g.getNextId(), Vector2(40, 150), Vector2(760, 435), rows, mSelected);
    }
    else
    {
        int maxRound = 0;
        for (const auto& m : t.matches)
        {
            if (m.knockout)
            {
                maxRound = (std::max)(maxRound, m.round);
            }
        }
        if (mRound > unsigned(maxRound))
        {
            mRound = 0;
        }
        if (button(g, 40, 133, 140, "Previous round"))
        {
            mRound = mRound ? mRound - 1 : unsigned(maxRound);
            mPage = 0;
        }
        if (button(g, 190, 133, 140, "Next round"))
        {
            mRound = (mRound + 1) % (maxRound + 1);
            mPage = 0;
        }
        if (button(g, 620, 133, 140, "Next page"))
        {
            ++mPage;
        }
        std::vector<const Match*> first;
        for (const auto& m : t.matches)
        {
            if (m.knockout && m.round == int(mRound))
            {
                first.push_back(&m);
            }
        }
        const unsigned pages = (std::max)(1u, unsigned((first.size() + 3) / 4));
        mPage %= pages;
        for (int col = 0; col < 3; ++col)
        {
            std::vector<const Match*> matches;
            for (const auto& m : t.matches)
            {
                if (m.knockout && m.round == int(mRound) + col)
                {
                    matches.push_back(&m);
                }
            }
            const unsigned start = (mPage * 4) >> col, count = 4u >> col;
            for (unsigned i = start; i < matches.size() && i < start + (std::max)(1u, count); ++i)
            {
                const auto& m = *matches[i];
                float x = 40.f + col * 244,
                      y = 185.f + 32.f * ((1 << col) - 1) + float(i - start) * (64.f * (1 << col));
                bool own = m.a == mConnection->own || m.b == mConnection->own;
                WarmUI::panel(g, x, y, 232, 44, own ? Color(104, 75, 35) : Color(65, 45, 29));
                const auto names = personName(t, m.a) + " / " + personName(t, m.b);
                const float nameSize =
                    (std::min)(18.f, 18.f * 216.f / (std::max)(1.f, RenderManager::menuTextWidth(names, 18)));
                WarmUI::text(g, x + 8, y + 3, names, nameSize, own ? 2 : 0);
                WarmUI::text(g, x + 8, y + 23,
                             "#" + std::to_string(m.id + 1) + "  " +
                                 (m.placement ? tr("Third place") + " " : "") + status(m),
                             13);
                if (col < 2)
                {
                    std::vector<const Match*> parents;
                    for (const auto& parent : t.matches)
                    {
                        if (parent.knockout && parent.round == m.round + 1)
                        {
                            parents.push_back(&parent);
                        }
                    }
                    for (unsigned p = 0; p < parents.size(); ++p)
                    {
                        if (!parents[p]->placement &&
                            (parents[p]->fromA == m.id || parents[p]->fromB == m.id))
                        {
                            const unsigned parentStart = (mPage * 4) >> (col + 1);
                            if (p < parentStart)
                            {
                                continue;
                            }
                            const float parentY = 185.f + 32.f * ((1 << (col + 1)) - 1) +
                                                  float(p - parentStart) * (64.f * (1 << (col + 1))) + 21;
                            const Color gold(209, 164, 68);
                            g.doOverlay(g.getNextId(), Vector2(x + 232, y + 21), Vector2(x + 239, y + 23),
                                        gold, .8f);
                            g.doOverlay(g.getNextId(), Vector2(x + 237, (std::min)(y + 21, parentY)),
                                        Vector2(x + 239, (std::max)(y + 23, parentY + 2)), gold, .8f);
                            g.doOverlay(g.getNextId(), Vector2(x + 238, parentY),
                                        Vector2(x + 244, parentY + 2), gold, .8f);
                        }
                    }
                }
            }
        }
        // A full list remains available for selecting matches across every round, including groups.
        std::vector<std::string> rows;
        for (const auto& m : t.matches)
        {
            rows.push_back(matchLabel(t, m));
        }
        g.doSelectbox(g.getNextId(), Vector2(40, 477), Vector2(760, 533), rows, mSelected);
    }
    if (mSelected >= t.matches.size())
    {
        mSelected = 0;
    }
    if (!t.matches.empty())
    {
        auto& m = t.matches[mSelected];
        if (button(g, 40, 545, 170, "Watch match", m.status == MatchStatus::Playing))
        {
            switchState(new TournamentSpectatorState(mConnection, m.id));
        }
        const bool resolve = t.organizer == mConnection->own && m.status == MatchStatus::Interrupted;
        if (button(g, 220, 545, 120, "Restart", resolve))
        {
            mConnection->send(Command::Restart, std::to_string(m.id));
        }
        if (button(g, 350, 545, 120, "Award left", resolve))
        {
            mConnection->send(Command::Award, std::to_string(m.id) + " " + std::to_string(m.a));
        }
        if (button(g, 480, 545, 120, "Award right", resolve))
        {
            mConnection->send(Command::Award, std::to_string(m.id) + " " + std::to_string(m.b));
        }
    }
}
void TournamentState::table()
{
    auto& g = getIMGUI();
    auto& t = mConnection->cup;
    if (t.settings.format == Format::Knockout)
    {
        WarmUI::text(g, 40, 160, tr("Knockout progress is shown in the bracket."), 22);
        return;
    }
    int group = -1;
    if (t.settings.format == Format::Groups)
    {
        mGroup %= unsigned(t.groupCount());
        group = int(mGroup);
        if (button(g, 40, 139, 230, tr("Group") + " " + std::string(1, char('A' + group))))
        {
            ++mGroup;
        }
    }
    WarmUI::text(g, 40, 184,
                 tr("Player") + "                         " + tr("Played / Wins / Points / Difference"), 18,
                 2);
    auto rows = t.standings(group);
    const unsigned pages = (std::max)(1u, unsigned((rows.size() + 7) / 8));
    mPage %= pages;
    for (unsigned i = mPage * 8; i < rows.size() && i < mPage * 8 + 8; ++i)
    {
        const auto& r = rows[i];
        float y = 216.f + (i - mPage * 8) * 29;
        if (r.person == mConnection->own)
        {
            WarmUI::panel(g, 40, y - 2, 720, 28, Color(104, 75, 35));
        }
        WarmUI::text(g, 50, y, std::to_string(i + 1) + ". " + t.person(r.person).name, 19,
                     r.person == mConnection->own ? 2 : 0);
        WarmUI::text(g, 410, y,
                     std::to_string(r.played) + " / " + std::to_string(r.wins) + " / " +
                         std::to_string(r.scored) + ":" + std::to_string(r.conceded) + " / " +
                         std::to_string(r.scored - r.conceded),
                     18);
    }
    if (button(g, 40, 500, 180, "Next page"))
    {
        ++mPage;
    }
    WarmUI::text(g, 40, 463, tr("Ties: wins, mini-table, point difference, points, seeded draw."), 14);
}
void TournamentState::participants()
{
    auto& g = getIMGUI();
    auto& t = mConnection->cup;
    std::vector<std::string> rows;
    for (const auto& p : t.people)
    {
        rows.push_back(p.name + " | " + tr(p.entered ? "Entrant" : "Spectator") + " | " +
                       tr(p.connected ? (p.ready ? "Ready" : "Connected") : "Disconnected") +
                       (p.id == t.organizer ? " | " + tr("Organizer") : ""));
    }
    g.doSelectbox(g.getNextId(), Vector2(40, 150), Vector2(760, 435), rows, mSelected);
    if (button(g, 40, 500, 240, "Remove entrant",
               t.organizer == mConnection->own && t.phase == Phase::Registration &&
                   mSelected < t.people.size() && t.people[mSelected].entered))
    {
        mConnection->send(Command::RemoveEntrant, std::to_string(mSelected));
    }
}
TournamentSpectatorState::TournamentSpectatorState(std::shared_ptr<TournamentConnection> connection,
                                                   int match)
    : GameState(new DuelMatch(true, DEFAULT_RULES_FILE, connection->cup.settings.points)),
      mConnection(std::move(connection)), mWatching(match)
{
}
void TournamentSpectatorState::init()
{
    const auto& m = mConnection->cup.match(mWatching);
    const auto& a=mConnection->cup.person(m.a); const auto& b=mConnection->cup.person(m.b);
    bool aLeft=spectatorLeft(a.name,a.id,b.name,b.id);
    const auto& left=aLeft?a:b; const auto& right=aLeft?b:a;
    mMatch->setPlayers(PlayerIdentity(left.name, Color(left.color), left.cycling, LEFT_PLAYER),
                       PlayerIdentity(right.name, Color(right.color), right.cycling, RIGHT_PLAYER));
    mMatch->pause();
    mConnection->send(Command::Watch, std::to_string(mWatching));
}
void TournamentSpectatorState::step_impl()
{
    packet_ptr packet;
    while ((packet = mConnection->client->Receive()))
    {
        if (packet->data[0] == ID_RULES_CHECKSUM)
        {
            mConnection->send(Command::StopWatching);
            RakNet::BitStream in(packet->data, packet->length, false);
            in.IgnoreBytes(1);
            int checksum, points;
            if (in.Read(checksum) && in.Read(points))
            {
                switchState(new NetworkGameState(mConnection->client, checksum, points, mConnection));
                return;
            }
        }
        if (packet->length >= 2 && packet->data[0] == ID_TOURNAMENT &&
            packet->data[1] == unsigned(Command::Live))
        {
            try
            {
                RakNet::BitStream in(packet->data, packet->length, false);
                in.IgnoreBytes(2);
                int match;
                if (in.Read(match) && match == mWatching && in.Read(mPaused))
                {
                    int timeMs = 0;
                    if (!in.Read(timeMs))
                    {
                        continue;
                    }
                    DuelMatchState state;
                    createGenericReader(&in)->generic<DuelMatchState>(state);
                    const auto now = SDL_GetTicks();
                    mSnapshotInterval =
                        mHasSnapshot ? (std::max)(15u, (std::min)(150u, now - mSnapshotAt)) : 33;
                    mPreviousSnapshot = mHasSnapshot ? mNextSnapshot : state;
                    mNextSnapshot = state;
                    mSnapshotAt = now;
                    mMatch->setState(state);
                    mMatch->setMatchTimeMs((std::max)(0, timeMs));
                    mHasSnapshot = true;
                }
            }
            catch (const std::exception&)
            {
            }
        }
        else
        {
            mConnection->receive(packet);
        }
    }
    if (mWatching < 0 || mWatching >= int(mConnection->cup.matches.size()))
    {
        mConnection->send(Command::StopWatching);
        switchState(new TournamentState(mConnection));
        return;
    }
    presentGame();
    presentGameUI();
    if (mConnection->cup.match(mWatching).status == MatchStatus::Finished)
    {
        const auto& m = mConnection->cup.match(mWatching);
        const auto& a=mConnection->cup.person(m.a); const auto& b=mConnection->cup.person(m.b);
        int left=spectatorLeft(a.name,a.id,b.name,b.id)?m.a:m.b;
        displayWinningPlayerScreen(m.winner == left ? LEFT_PLAYER : RIGHT_PLAYER);
    }
    auto& g = getIMGUI();
    g.useMenuStyle();
    g.doCursor();
    if (!mConnection->error.empty())
    {
        WarmUI::text(g, 40, 515, tr(mConnection->error), 16, 2);
    }
    WarmUI::text(g, 40, 551,
                 tr(!mHasSnapshot ? "Waiting for live match..."
                    : mPaused     ? "Spectating - paused"
                                  : "Spectating live"),
                 18, 2);
    if (button(g, 490, 545, 270, "Tournament overview") || is_exiting())
    {
        mConnection->send(Command::StopWatching);
        switchState(new TournamentState(mConnection));
    }
}

void TournamentSpectatorState::render(float alpha)
{
    if (!mHasSnapshot)
    {
        GameState::render(alpha);
        return;
    }
    auto& renderer = getApp().getRenderManager();
    renderer.setCourtWidth(800);
    const float blend = (std::min)(1.f, float(SDL_GetTicks() - mSnapshotAt) / mSnapshotInterval);
    renderer.drawGame(interpolateMatch(mPreviousSnapshot, mNextSnapshot, blend));
}
