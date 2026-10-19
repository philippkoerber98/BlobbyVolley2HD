#include "server/Tournament.h"
#include "TournamentProtocol.h"
#include <iostream>
#include <set>
#include <stdexcept>
using namespace tournament;
static void need(bool value, const char* message)
{
    if (!value)
    {
        throw std::runtime_error(message);
    }
}
static Tournament create(int n, Format format)
{
    Tournament t;
    t.settings.capacity = 32;
    t.slots = 32;
    t.settings.parallel = 16;
    t.settings.format = format;
    for (int i = 0; i < n; ++i)
    {
        int id = t.registerPerson("Player " + std::to_string(i), i, "private-" + std::to_string(i));
        t.enroll(id, true);
        t.person(id).ready = true;
    }
    t.organizer = 0;
    t.start(12345);
    return t;
}
int main()
{
    try
    {
        for (int n = 2; n <= 32; ++n)
        {
            for (auto format : {Format::RoundRobin, Format::Knockout, Format::Groups})
            {
                if (format == Format::Groups && n < 6)
                {
                    continue;
                }
                auto t = create(n, format);
                int wins = 0, cycles = 0;
                while (t.phase != Phase::Finished)
                {
                    need(++cycles < 1000, "Tournament did not terminate");
                    for (auto& p : t.people)
                    {
                        p.ready = true;
                    }
                    auto next = t.available();
                    need(!next.empty(), "Tournament deadlocked");
                    std::set<int> busy;
                    need(next.size() <= size_t(t.settings.parallel), "Parallel limit exceeded");
                    for (int id : next)
                    {
                        auto& m = t.match(id);
                        need(busy.insert(m.a).second && busy.insert(m.b).second, "Player scheduled twice");
                        m.status = MatchStatus::Playing;
                    }
                    for (int id : next)
                    {
                        const auto m = t.match(id);
                        t.result(id, m.a, 15, 4);
                        ++wins;
                    }
                    auto copy = Tournament::parse(t.xml(true), true);
                    need(copy.xml(true) == t.xml(true), "Save round trip changed competition");
                }
                if (format == Format::Knockout)
                {
                    need(wins == n - 1, "Wrong knockout match count");
                }
                if (format == Format::RoundRobin)
                {
                    need(wins == n * (n - 1) / 2, "Wrong round robin match count");
                }
                if (format == Format::Groups)
                {
                    for (const auto& m : t.matches)
                    {
                        if (m.knockout && m.round == 0)
                        {
                            need(t.person(m.a).group != t.person(m.b).group, "Same-group semifinal pairing");
                        }
                    }
                }
                auto publicCopy = Tournament::parse(t.xml());
                for (const auto& p : publicCopy.people)
                {
                    need(p.token.empty(), "Reconnect token leaked");
                }
            }
        }
        for (auto f : {Format::Knockout, Format::Groups, Format::RoundRobin})
        {
            auto cup = create(8, f);
            cup.phase = Phase::Registration;
            cup.settings.thirdPlace = true;
            cup.settings.finalMatch = true;
            cup.preview(99);
            auto preview = cup.matches;
            cup.start(unsigned(cup.seed));
            need(cup.matches.size() == preview.size(), "Draw preview changed at start");
            for (size_t i = 0; i < preview.size(); ++i)
            {
                need(cup.matches[i].a == preview[i].a && cup.matches[i].b == preview[i].b,
                     "Preview pairing changed");
            }
            for (int cycle = 0; cup.phase != Phase::Finished; ++cycle)
            {
                need(cycle < 100, "Final/bronze tournament stalled");
                for (auto& p : cup.people)
                {
                    p.ready = true;
                }
                auto ids = cup.available();
                need(!ids.empty(), "Final/bronze deadlocked");
                for (int id : ids)
                {
                    auto m = cup.match(id);
                    cup.match(id).status = MatchStatus::Playing;
                    cup.result(id, m.a, 15, 3);
                }
            }
            need(cup.champion() >= 0, "Champion missing");
            for (const auto& m : cup.matches)
            {
                if (m.placement)
                {
                    need(m.winner != cup.champion(), "Bronze winner became champion");
                }
            }
        }
        auto t = create(8, Format::Knockout);
        auto next = t.available();
        t.match(next[0]).status = MatchStatus::Playing;
        t.interrupt(next[0]);
        need(t.match(next[0]).status == MatchStatus::Interrupted, "Interruption failed");
        t.restart(next[0]);
        t.match(next[0]).status = MatchStatus::Playing;
        const auto m = t.match(next[0]);
        t.result(m.id, m.b, 0, 0, true);
        bool rejected = false;
        try
        {
            t.result(m.id, m.b, 0, 0, true);
        }
        catch (...)
        {
            rejected = true;
        }
        need(rejected, "Duplicate result accepted");
        auto recovered = Tournament::parse(t.xml(true), true);
        recovered.recover();
        for (const auto& p : recovered.people)
        {
            need(!p.connected && !p.ready, "Recovery retained live connections");
        }
        RakNet::BitStream out;
        writePacket(out, Command::Hello, "token");
        Command c;
        need(readPacket(out, c) == "token" && c == Command::Hello, "Protocol roundtrip failed");
        RakNet::BitStream bad;
        bad.Write((unsigned char)ID_TOURNAMENT);
        bad.Write((unsigned char)Command::State);
        bad.Write(0xffffffffu);
        rejected = false;
        try
        {
            readPacket(bad, c);
        }
        catch (...)
        {
            rejected = true;
        }
        need(rejected, "Oversized packet accepted");
        std::cout << "Tournament formats 2..32, byes, scheduling, group pairing, results, saves, privacy and "
                     "protocol passed\n";
    }
    catch (const std::exception& e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
